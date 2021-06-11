#define LINUX
#define DIRECTORY "mp3"
#define FILENAME "status"

#define REGISTRATION 'R'
#define DEREGISTRATION 'U'

#define SAMPLING_INTERVAL_MS 50
#define NUMBER_PAGES 128
#define CHAR_DEV_NAME "my_mp3_char_device"
#define MEM_BUFFER_CAP 65536 // number of pages times 4kB per page / 8 bytes
#define BYTE_IN_KB 1024

#define DEBUG 1

#include "mp3_given.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/jiffies.h>

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/uaccess.h>

#include <linux/time.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/page-flags.h>
#include <linux/cdev.h>
#include <linux/device.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("HGENG4");
MODULE_DESCRIPTION("CS-423 MP3");


struct proc_dir_entry * mp3_dir; // create /proc/mp3
struct proc_dir_entry * status_file; // create /proc/mp3/status

spinlock_t list_lock;
unsigned long sampling_interval_jiffies;

struct workqueue_struct * wq;

unsigned long * profiler;
int profiler_idx = 0;

struct cdev cdevice;
dev_t device;


int on_register(char * usr_msg_without_type);
void on_deregister(char * usr_msg_without_type);

ssize_t mp3_write(struct file *file, const char __user *buffer, size_t count, loff_t *data);
ssize_t mp3_read(struct file *file, char __user *buffer, size_t count, loff_t *data);

void update_profiler(struct work_struct *work);

int mmap_cdev(struct file *fp, struct vm_area_struct *vma);
int open_cdev(struct inode* inode, struct file* file);
int close_cdev(struct inode* inode, struct file* file);


struct mp3_task_struct{
    struct list_head list;
    struct task_struct *linux_task;

    int pid;
};

const struct file_operations status_fops = {
     .owner = THIS_MODULE,
     .write = mp3_write,
     .read = mp3_read,
};

const struct file_operations cdev_fops = {
    .owner   = THIS_MODULE,
    .open    = open_cdev,
    .release = close_cdev,
    .mmap    = mmap_cdev
};


LIST_HEAD(mp3_list);
DECLARE_DELAYED_WORK(dwq, update_profiler);


int on_register(char * usr_msg_without_type){ 
	struct mp3_task_struct * new_pair_cursor;
	int pass;
	int new_pid;
	unsigned long lock_flag;

	sscanf(strsep(&usr_msg_without_type, "\n"), "%u", &new_pid); // read pid

	new_pair_cursor = (struct mp3_task_struct*)kmalloc(sizeof(struct mp3_task_struct), GFP_KERNEL);
	INIT_LIST_HEAD(& (new_pair_cursor->list));

	// set fields
	new_pair_cursor->pid = new_pid;    
	new_pair_cursor->linux_task = find_task_by_pid(new_pair_cursor->pid); //set task struct

	//mutex_lock_interruptible(&list_lock);
	spin_lock_irqsave(&list_lock, lock_flag); 
    
    if (list_empty(&mp3_list)) { // start queueing when enqueue the first process
        queue_delayed_work(wq, &dwq, sampling_interval_jiffies);
    }
	list_add(&(new_pair_cursor->list), &mp3_list);
	//mutex_unlock(&list_lock);
	spin_unlock_irqrestore(&list_lock, lock_flag);

	return 1; // sucessful registration
}


void on_deregister(char * usr_msg_without_type){
   unsigned int pid;
   struct mp3_task_struct * cursor;
   struct mp3_task_struct * temp;
   unsigned long lock_flag;
   int ret_code;

   sscanf(usr_msg_without_type, "%u", &pid);

   //mutex_lock_interruptible(&list_lock);
   spin_lock_irqsave(&list_lock, lock_flag);
   list_for_each_entry_safe(cursor, temp, &mp3_list, list) {
      if(cursor->pid == pid){
        list_del(&cursor->list); // remove the process
        kfree(cursor);
        // break;
      }
   }

   if (list_empty(&mp3_list)){ // cancel the work when all processes are gone
        cancel_delayed_work(&dwq);
        flush_workqueue(wq);
   } 

   //mutex_unlock(&list_lock);  
   spin_unlock_irqrestore(&list_lock, lock_flag);
}


ssize_t mp3_write(struct file *file, const char __user *buffer, size_t count, loff_t *data){
   char *copy_from_user_buffer;
   int success;

   copy_from_user_buffer = (char *)kmalloc(count, GFP_KERNEL);
   copy_from_user(copy_from_user_buffer, buffer, count); 

   // remove the msg identifier before sending to handler
   switch(copy_from_user_buffer[0]){
        case REGISTRATION:
            #ifdef DEBUG
            printk(KERN_ALERT "REGISTRATION STARTING\n");
            #endif
            
            success = on_register(copy_from_user_buffer + 2); // routine for register
   			
    		#ifdef DEBUG
    		printk(KERN_ALERT "REGISTRATION DONE %d\n", success);
    		#endif
                
            break;

		case DEREGISTRATION:
            #ifdef DEBUG
            printk(KERN_ALERT "DE-REGISTRATION STARTING\n");
            #endif
   			
            on_deregister(copy_from_user_buffer + 2); // rountine for deregister

            #ifdef DEBUG
            printk(KERN_ALERT "DE-REGISTRATION DONE\n");
            #endif
            break;

        default:
          printk(KERN_ALERT "USR MESSAGE NOT RECOGNIZED.\n");
   }

   kfree(copy_from_user_buffer);
   return count;
}


ssize_t mp3_read(struct file *file, char __user *buffer, size_t count, loff_t *data){
	unsigned long size_transferred_data = 0;
   	unsigned long lock_flag;
   	struct mp3_task_struct *cursor;
   	ssize_t retval = 0;
   
   	char *copy_to_user_buffer;
   	copy_to_user_buffer = (char *)kmalloc(count, GFP_KERNEL);
   
   	if (data != NULL && *data > 0){
      	return retval; 
   	}
   
   	spin_lock_irqsave(&list_lock, lock_flag);
    //mutex_lock_interruptible(&list_lock);
   	list_for_each_entry(cursor, &mp3_list, list){
       	size_transferred_data += sprintf(copy_to_user_buffer + size_transferred_data, "%u \n", cursor->pid); // read pid
   	}
    //mutex_unlock(&list_lock);
   	spin_unlock_irqrestore(&list_lock, lock_flag);
   	retval = simple_read_from_buffer(buffer, count, data, copy_to_user_buffer, size_transferred_data); // copy the data and update the offset pointer
   	kfree(copy_to_user_buffer);
   	return retval;
}


int mmap_cdev(struct file *fp, struct vm_area_struct *vma){
    unsigned long mapping_size = (vma->vm_end) - (vma->vm_start);
    unsigned long start_addr = vma->vm_start;
    int index;
    int ret;
    unsigned long page_frame_number;

    if (mapping_size > NUMBER_PAGES * PAGE_SIZE){
        #ifdef DEBUG
        printk(KERN_ALERT "MAPPING SIZE OVERFLOW\n");
        #endif
        return -1;
    }


    #ifdef DEBUG
    printk(KERN_ALERT "MAPPING CDEV\n");
    #endif

    for (index = 0; index < mapping_size; index += PAGE_SIZE){
        page_frame_number = vmalloc_to_pfn((void *)(((unsigned long)profiler) + index)); // get page frame number
        ret = remap_pfn_range(vma, start_addr+index, page_frame_number, PAGE_SIZE, vma->vm_page_prot); //map a virtual page of a user process to a physical page 
        if (ret){
            #ifdef DEBUG
            printk(KERN_ALERT "MAPPING TO USER FAILED\n");
            #endif
            return -1;
        }
    }

    #ifdef DEBUG
    printk(KERN_ALERT "MAPPING CDEV DONE\n");
    #endif
    return 0;
}

int close_cdev(struct inode* inode, struct file* file){
    return 0;
}

int open_cdev(struct inode* inode, struct file* file){
    return 0;
}


void update_profiler(struct work_struct *work){
    unsigned long lock_flag;
    struct mp3_task_struct *cursor;
    int ret_code;
    unsigned long min_flt, maj_flt, utime, stime;
    
    unsigned long min_flt_curr_total = 0;
    unsigned long maj_flt_curr_total = 0;
    unsigned long cpu_time_curr_total = 0;


    spin_lock_irqsave(&list_lock, lock_flag);
    list_for_each_entry(cursor, &mp3_list, list) {
        ret_code = get_cpu_use(cursor->pid, &min_flt, &maj_flt, &utime, &stime);
        if (ret_code == -1){ // process gone
            continue;
        }

        min_flt_curr_total += min_flt;
        maj_flt_curr_total += maj_flt;
        cpu_time_curr_total += utime + stime;
    }

    profiler[profiler_idx ++] = jiffies;
    profiler[profiler_idx ++] = min_flt_curr_total;
    profiler[profiler_idx ++] = maj_flt_curr_total;
    profiler[profiler_idx ++] = jiffies_to_msecs(cputime_to_jiffies(cpu_time_curr_total));

    if (profiler_idx >= MEM_BUFFER_CAP){
        printk(KERN_ALERT "BUFFER FULL ... wrapping around \n");
        profiler_idx = 0;
    }

    spin_unlock_irqrestore(&list_lock, lock_flag);
    queue_delayed_work(wq, &dwq, sampling_interval_jiffies);
}


int __init mp3_init(void){
    int ret_code, index;

	#ifdef DEBUG
	printk(KERN_ALERT "MP3 MODULE LOADING\n");
	#endif

    sampling_interval_jiffies = msecs_to_jiffies(SAMPLING_INTERVAL_MS);

	// get /proc/mp3
    #ifdef DEBUG
    printk(KERN_ALERT "INIT MP3 MODULE DIRECTORY\n");
    #endif
	mp3_dir = proc_mkdir(DIRECTORY, NULL);
	if (!mp3_dir) {
		return -ENOMEM;
	}

	// get /proc/mp3/status
    #ifdef DEBUG
    printk(KERN_ALERT "INIT MP3 MODULE STATUS FILE\n");
    #endif
	status_file = proc_create(FILENAME, 0666, mp3_dir, &status_fops);
	if (!status_file) {
		return -ENOMEM;
	}

	// init lock
    #ifdef DEBUG
    printk(KERN_ALERT "INIT MP3 MODULE LOCK\n");
    #endif
	spin_lock_init(&list_lock);


    #ifdef DEBUG
    printk(KERN_ALERT "INIT MP3 MODULE WORKQUEUE\n");
    #endif
    wq = create_singlethread_workqueue("wq");


    #ifdef DEBUG
    printk(KERN_ALERT "INIT MP3 MODULE PROFILER_BUFFER\n");
    #endif
    profiler = vmalloc(NUMBER_PAGES * PAGE_SIZE);
    memset(profiler, -1, NUMBER_PAGES * PAGE_SIZE);
    for(index = 0; index < NUMBER_PAGES * PAGE_SIZE; index+=PAGE_SIZE){
        SetPageReserved(vmalloc_to_page((void *)(((unsigned long)profiler) + index)));
    }


    #ifdef DEBUG
    printk(KERN_ALERT "INIT MP3 MODULE CHARACTER DEVICE\n");
    #endif
    alloc_chrdev_region(&device, 0, 1, CHAR_DEV_NAME);
    cdev_init(&cdevice, &cdev_fops);
    cdev_add(&cdevice, device, 1);

	printk(KERN_ALERT "MP3 MODULE LOADED\n");
	return 0;   
}


void __exit mp3_exit(void){
	struct mp3_task_struct *cursor;
   	struct mp3_task_struct *temp;
    int index = 0;

    printk(KERN_ALERT "MP3 MODULE UNLOADING\n");


    #ifdef DEBUG
    printk(KERN_ALERT "REMOVING DELAYED WORK QUEUE\n");
    #endif
    cancel_delayed_work(&dwq);
    flush_workqueue(wq);
    destroy_workqueue(wq);


   	//mutex_destroy(&list_lock);
    #ifdef DEBUG
    printk(KERN_ALERT "REMOVING REGISTERED PROCESSES\n");
    #endif
   	list_for_each_entry_safe(cursor, temp, &mp3_list, list) {
        list_del(&cursor->list);
        kfree(cursor);
    }


    #ifdef DEBUG
    printk(KERN_ALERT "REMOVING VMEM BUFFER\n");
    #endif
    for(index = 0; index < NUMBER_PAGES * PAGE_SIZE; index+=PAGE_SIZE){
        ClearPageReserved(vmalloc_to_page((void *)(((unsigned long)profiler) + index)));
    }
    vfree(profiler);


    #ifdef DEBUG
    printk(KERN_ALERT "REMOVING CHARACTER DEVICE\n");
    #endif
    cdev_del(&cdevice);
    unregister_chrdev_region(device, 1);


    #ifdef DEBUG
    printk(KERN_ALERT "REMOVING FILE SYSTEM\n");
    #endif
    remove_proc_entry(FILENAME, mp3_dir);
    remove_proc_entry(DIRECTORY, NULL);


    printk(KERN_ALERT "MP3 MODULE UNLOADED\n");
}


module_init(mp3_init);
module_exit(mp3_exit);
