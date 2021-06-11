#define LINUX
#define DIRECTORY "mp1"
#define FILENAME "status"

#define UPDATE_TIME_INTERVAL_MS 5000

#include "mp1_given.h"


#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/jiffies.h>

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/uaccess.h>


#include <linux/time.h>
#include <linux/timer.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("HGENG4");
MODULE_DESCRIPTION("CS-423 MP1");

#define DEBUG 1

struct mp1_pid_cpu_time_list{
   struct list_head list;
   
   int pid; 
   unsigned long cpu_time;
};

LIST_HEAD(mp1_list); // defind (pid, cput_time) list

spinlock_t lock; // lock for adding, reading, and removing element from mp1_list

struct proc_dir_entry* mp1_dir; // create /proc/mp1
struct proc_dir_entry* status_file; // create /proc/mp1/status
struct timer_list timer; 

struct work_struct* cpu_time_update_work; 
struct workqueue_struct* cpu_time_update_work_queue; 


ssize_t mp1_read(struct file *file, char __user *buffer, size_t count, loff_t *data){
   unsigned long size_transferred_data = 0;
   unsigned long lock_flag;
   struct mp1_pid_cpu_time_list *cursor;
   ssize_t retval = 0;
   
   char *copy_to_user_buffer;
   copy_to_user_buffer = (char *)kmalloc(count, GFP_KERNEL);
   
   if (data != NULL && *data > 0){
      //linked list has been read, return
      return retval; 
   }
   
   spin_lock_irqsave(&lock, lock_flag);
   // CRITICAL: traverse a list read pid, cpu time
   list_for_each_entry(cursor, &mp1_list, list){
       size_transferred_data += sprintf(copy_to_user_buffer + size_transferred_data, "PID %u: %lu cpu_time\n", cursor->pid, cursor->cpu_time);
   }
   spin_unlock_irqrestore(&lock, lock_flag);
   retval = simple_read_from_buffer(buffer, count, data, copy_to_user_buffer, size_transferred_data); // copy the data and update the offset pointer
   kfree(copy_to_user_buffer);
   return retval;
}


ssize_t mp1_write(struct file *file, const char __user *buffer, size_t count, loff_t *data){
   unsigned long lock_flag;
   char *copy_from_user_buffer;

   struct mp1_pid_cpu_time_list *new_pair_cursor;
   new_pair_cursor = (struct mp1_pid_cpu_time_list *)kmalloc(sizeof(struct mp1_pid_cpu_time_list), GFP_KERNEL);
   INIT_LIST_HEAD(& (new_pair_cursor->list));
   
   new_pair_cursor->cpu_time = 0; // initialize cpu_time to 0 upon registration

   copy_from_user_buffer = (char *)kmalloc(count, GFP_KERNEL);
   copy_from_user(copy_from_user_buffer, buffer, count); 
   sscanf(copy_from_user_buffer, "%u", &new_pair_cursor->pid); // set PID of th process 

   spin_lock_irqsave(&lock, lock_flag);
   list_add(&(new_pair_cursor->list), &mp1_list); // CRITICAL: add elements
	spin_unlock_irqrestore(&lock, lock_flag);
   
   kfree(copy_from_user_buffer);
   
   return count;
};


const struct file_operations status_fops = {
     .owner	= THIS_MODULE,
     .write = mp1_write,
     .read = mp1_read,
};


void update_proccess_cpu_time(struct work_struct *work){
   unsigned long lock_flag;
   
   struct mp1_pid_cpu_time_list *cursor;
   struct mp1_pid_cpu_time_list *temp;

   spin_lock_irqsave(&lock, lock_flag);
   list_for_each_entry_safe(cursor, temp, &mp1_list, list) {
      int ret_code = get_cpu_use(cursor->pid, &cursor->cpu_time); //update cpu_time using function given
      if (ret_code == -1) {
         list_del(&cursor->list); // process finished, remove the process from the linked list
         kfree(cursor);
      }
   }
   spin_unlock_irqrestore(&lock, lock_flag);
   mod_timer(&timer, jiffies + msecs_to_jiffies(UPDATE_TIME_INTERVAL_MS)); // wake up until next 5ms mark
};


void timer_callback(unsigned long data){
   queue_work(cpu_time_update_work_queue, cpu_time_update_work); // schedule work on to the workqueue
}


// mp1_init - Called when module is loaded
int __init mp1_init(void){
   #ifdef DEBUG
   printk(KERN_ALERT "MP1 MODULE LOADING\n");
   #endif

   // get /proc/mp1
   mp1_dir = proc_mkdir(DIRECTORY, NULL);
   if (!mp1_dir) {
      return -ENOMEM;
   }

   // get /proc/mp1/status
   status_file = proc_create(FILENAME, 0666, mp1_dir, &status_fops);
   if (!status_file) {
      return -ENOMEM;
   }
   
   // init lock
   spin_lock_init(&lock);

   // init timer
   setup_timer(&timer, timer_callback, 0);
   mod_timer(&timer, jiffies + msecs_to_jiffies(UPDATE_TIME_INTERVAL_MS));

   // init work
   cpu_time_update_work = (struct work_struct*)kmalloc(sizeof(struct work_struct), GFP_KERNEL);
   INIT_WORK(cpu_time_update_work, update_proccess_cpu_time);

   // init work queue
   cpu_time_update_work_queue = create_workqueue("cpu_time_update_work_queue");
   if (cpu_time_update_work_queue == NULL) {
      return -ENOMEM;
   }

   printk(KERN_ALERT "MP1 MODULE LOADED\n");
   return 0;   
};

// mp1_exit - Called when module is unloaded
void __exit mp1_exit(void){
   struct mp1_pid_cpu_time_list *cursor;
   struct mp1_pid_cpu_time_list *temp;

   #ifdef DEBUG
   printk(KERN_ALERT "MP1 MODULE UNLOADING\n");
   #endif

   //remove /proc/mp1 file system
   remove_proc_entry(FILENAME, mp1_dir);
   remove_proc_entry(DIRECTORY, NULL);

   // delete timer
   del_timer_sync(&timer);

   //free workqueue
   flush_workqueue(cpu_time_update_work_queue);
   destroy_workqueue(cpu_time_update_work_queue);

   kfree(cpu_time_update_work); //free work

   //free the list
   list_for_each_entry_safe(cursor, temp, &mp1_list, list){
      list_del(&cursor->list);
      kfree(cursor);
   }

   printk(KERN_ALERT "MP1 MODULE UNLOADED\n");
};


// Register init and exit funtions
module_init(mp1_init);
module_exit(mp1_exit);
