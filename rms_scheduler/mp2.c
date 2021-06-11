#define LINUX
#define DIRECTORY "mp2"
#define FILENAME "status"

#define REGISTRATION 'R'
#define YIELD 'Y'
#define DEREGISTRATION 'D'

#define READY 0
#define RUNNING 1
#define SLEEPING 2

#define MULTIPLER 10000
#define ADMISSION_BOUND 6930

#define DEBUG 1

#include "mp2_given.h"

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

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HGENG4");
MODULE_DESCRIPTION("CS-423 MP2");


struct mp2_schedule_info_list{
   struct list_head list;

   struct task_struct *linux_task;
   struct timer_list wakeup_timer;
   
   int pid; 
   unsigned int period;
   unsigned int processing_time; 
   unsigned int state; // running , ready or sleeping

   unsigned long deadline_jiff;

   int deadline_set;
};


struct proc_dir_entry * mp2_dir; // create /proc/mp2
struct proc_dir_entry * status_file; // create /proc/mp2/status

struct kmem_cache * mp2_schedule_info_cache;
struct mp2_schedule_info_list * current_running;

struct task_struct * dispatching_thread;


LIST_HEAD(mp2_list);

DEFINE_MUTEX(list_lock);
DEFINE_MUTEX(running_task_lock);
spinlock_t timer_lock;


struct mp2_schedule_info_list * get_task(unsigned int pid){
   struct mp2_schedule_info_list * cursor;
   list_for_each_entry(cursor, &mp2_list, list) {
      if (cursor->pid == pid) {
         return cursor;
      }
   } 

   return NULL;
}



int pass_admission_control(unsigned int period, unsigned int processing_time){
   struct mp2_schedule_info_list * cursor;
   unsigned int total_percentage = 0;
   
   mutex_lock_interruptible(&list_lock);
   list_for_each_entry(cursor, &mp2_list, list) {
      total_percentage += (cursor->processing_time * MULTIPLER) / cursor->period;
   }
   mutex_unlock(&list_lock);

   total_percentage += (processing_time * MULTIPLER) / period;

   if (total_percentage > ADMISSION_BOUND){
      return 0;
   }

   return 1;
}


void switch_context(struct mp2_schedule_info_list * task, int context_macro, int priority){
   struct sched_param sparam;
   sparam.sched_priority = priority;
   sched_setscheduler(task->linux_task, context_macro, &sparam);
}


int compare_priority(struct mp2_schedule_info_list * first, struct mp2_schedule_info_list * second){
   // return 1, first task is more prioritized
   // return 0, otherwise
   return first->period <= second->period;
   //return this_period <= other_period;
}


struct mp2_schedule_info_list * get_most_prioritized_ready_task(void){
   struct mp2_schedule_info_list * cursor;
   struct mp2_schedule_info_list * most_prioritized_task = NULL;
   int found_first_ready_task = 0;

   mutex_lock_interruptible(&list_lock);
   list_for_each_entry(cursor, &mp2_list, list){
      if (cursor->state != READY){
         continue;
      }

      if (!found_first_ready_task){
         most_prioritized_task = cursor; // there is at least one READY task. 
         found_first_ready_task = 1;
      } else if (compare_priority(cursor, most_prioritized_task)) {
         most_prioritized_task = cursor; // found a READY task that has a smaller period;
      }
   }
   mutex_unlock(&list_lock);

   return most_prioritized_task; // no READY tasks, return 0
}


void timer_callback(unsigned long pid){
   unsigned long lock_flag;
   struct mp2_schedule_info_list * task;

   spin_lock_irqsave(&timer_lock, lock_flag);
   task = get_task(pid);
   task->state = READY;
   spin_unlock_irqrestore(&timer_lock, lock_flag);

   wake_up_process(dispatching_thread);
}


int on_register(char * usr_msg_without_type){ 
   struct mp2_schedule_info_list * new_pair_cursor;
   int pass;
   int new_pid, new_period, new_processing_time;

   sscanf(strsep(&usr_msg_without_type, ","), "%u", &new_pid); 
   sscanf(strsep(&usr_msg_without_type, ","), "%u", &new_period); 
   sscanf(strsep(&usr_msg_without_type, "\n"), "%u", &new_processing_time); 
   
   pass = pass_admission_control(new_period, new_processing_time);
   if(!pass){
      return 0; // fail to pass admission control
   }

   new_pair_cursor = (struct mp2_schedule_info_list *)kmem_cache_alloc(mp2_schedule_info_cache, GFP_KERNEL);
   INIT_LIST_HEAD(& (new_pair_cursor->list));

   // set fields
   new_pair_cursor->pid = new_pid;   
   new_pair_cursor->period = new_period;
   new_pair_cursor->processing_time = new_processing_time;
   new_pair_cursor->deadline_jiff = 0;
   new_pair_cursor->deadline_set = 0;
   new_pair_cursor->state = SLEEPING;
   
   setup_timer(&new_pair_cursor->wakeup_timer, timer_callback, new_pair_cursor->pid); //set timer
   new_pair_cursor->linux_task = find_task_by_pid(new_pair_cursor->pid); //set task struct
   
   mutex_lock_interruptible(&list_lock);
   list_add(&(new_pair_cursor->list), &mp2_list);
   mutex_unlock(&list_lock);

   return 1; // sucessful registration
}


void on_yield(char * usr_msg_without_type){
   unsigned int pid;
   struct mp2_schedule_info_list * task;
   int skip_yielding = 0;

   sscanf(usr_msg_without_type, "%u", &pid);
   
   #ifdef DEBUG
   printk(KERN_ALERT "YIELD: looking for task ... :\n");
   #endif

   mutex_lock_interruptible(&list_lock);
   task = get_task(pid);
   mutex_unlock(&list_lock);

   // set deadline
   if (!task->deadline_set){
      task->deadline_set = 1;
      task->deadline_jiff = jiffies + msecs_to_jiffies(task->period);
      #ifdef DEBUG
      printk(KERN_ALERT "pid %u curr jiff: %lu, deadline: %lu", pid, jiffies, task->deadline_jiff);
      #endif
   } else{
      task->deadline_jiff += msecs_to_jiffies(task->period);
      if (jiffies > task->deadline_jiff){
         #ifdef DEBUG
         printk(KERN_ALERT "YIELD: deadline passed ... skip yielding %d", pid);
         printk(KERN_ALERT "curr jiff: %lu, deadline: %lu", jiffies, task->deadline_jiff);
         #endif
         skip_yielding = 1;
      }
   }

   if(skip_yielding){
      return;
   }

   #ifdef DEBUG
   printk(KERN_ALERT "set up timer %u", pid);
   #endif

   mod_timer(&task->wakeup_timer, task->deadline_jiff);
   task->state = SLEEPING;
   mutex_lock_interruptible(&running_task_lock);
   current_running = NULL;
   mutex_unlock(&running_task_lock);
   
   wake_up_process(dispatching_thread);
   
   set_task_state(task->linux_task, TASK_UNINTERRUPTIBLE);
   schedule();
}


void on_deregister(char * usr_msg_without_type){
   unsigned int pid;
   struct mp2_schedule_info_list * cursor;
   struct mp2_schedule_info_list * temp;

   sscanf(usr_msg_without_type, "%u", &pid);

   // find and delete the task from mp2_task_struct_list
   mutex_lock_interruptible(&list_lock);
   list_for_each_entry_safe(cursor, temp, &mp2_list, list) {
      if(cursor->pid == pid){
        // found pid to be deregistered
         if(current_running == cursor){
            current_running = NULL;
            wake_up_process(dispatching_thread);
         }

        del_timer(&cursor->wakeup_timer);
        list_del(&cursor->list);
        kmem_cache_free(mp2_schedule_info_cache, cursor);
      }
   }
   mutex_unlock(&list_lock);   
}


ssize_t mp2_write(struct file *file, const char __user *buffer, size_t count, loff_t *data){
   char *copy_from_user_buffer;

   copy_from_user_buffer = (char *)kmalloc(count, GFP_KERNEL);
   copy_from_user(copy_from_user_buffer, buffer, count); 

   // remove the msg identifier before sending to handler
   switch(copy_from_user_buffer[0]){
   		case REGISTRATION:
   			on_register(copy_from_user_buffer + 3); 
            break;
   		
         case YIELD:
   			on_yield(copy_from_user_buffer + 3); 
            break;

         case DEREGISTRATION:
   			on_deregister(copy_from_user_buffer + 3); 
            break;
         
         default:
            printk(KERN_ALERT "User message not recognized.\n");
   }

   kfree(copy_from_user_buffer);
	return count;
}


ssize_t mp2_read(struct file *file, char __user *buffer, size_t count, loff_t *data){
   unsigned long size_transferred_data = 0;
   struct mp2_schedule_info_list *cursor;
   ssize_t retval = 0;
   
   char * copy_to_user_buffer;
   copy_to_user_buffer = (char *)kmalloc(count, GFP_KERNEL);
   
   if (data != NULL && *data > 0){
      return retval; 
   }
   
   #ifdef DEBUG
   printk(KERN_ALERT "MP2 READ FUNC CALLED\n");
   #endif

   mutex_lock_interruptible(&list_lock);
   list_for_each_entry(cursor, &mp2_list, list){
      size_transferred_data += sprintf(
         copy_to_user_buffer + size_transferred_data, 
         "%u: %u, %u, %u\n", 
         cursor->pid, 
         cursor->period, 
         cursor->processing_time,
         cursor->state
      );
   }
   mutex_unlock(&list_lock);

   retval = simple_read_from_buffer(buffer, count, data, copy_to_user_buffer, size_transferred_data); 
   kfree(copy_to_user_buffer);
   return retval;
}


int dispatching_thread_func(void * data){
   struct mp2_schedule_info_list * most_prioritized_task = NULL;
   int is_current_more_prioritized;


   while(!kthread_should_stop()){
      set_current_state(TASK_INTERRUPTIBLE);  
      #ifdef DEBUG
      printk(KERN_ALERT "dispatching_thread waiting for timmer ... ");
      #endif
      schedule(); // wait at here until woken up by either a YIELD msg or timer expires

      mutex_lock_interruptible(&running_task_lock);
      most_prioritized_task = get_most_prioritized_ready_task();
      if(most_prioritized_task != NULL){
         #ifdef DEBUG
         printk(KERN_ALERT "at least one ready state exists %d", most_prioritized_task->pid);
         #endif
         if ((current_running != NULL)){
            is_current_more_prioritized = compare_priority(
               current_running, 
               most_prioritized_task
            );

            if(!is_current_more_prioritized){
               #ifdef DEBUG
               printk(KERN_ALERT "preempt the current running task %d", current_running->pid);
               #endif
               current_running->state = READY;
               switch_context(current_running, SCHED_NORMAL, 0);
            }
         }

         #ifdef DEBUG
         printk(KERN_ALERT "picked to running %d ", most_prioritized_task->pid);
         #endif

         most_prioritized_task->state = RUNNING;

         #ifdef DEBUG
         printk(KERN_ALERT "wake up process for %d", most_prioritized_task->pid);
         #endif

         wake_up_process(most_prioritized_task->linux_task);
         switch_context(most_prioritized_task, SCHED_FIFO, 99);

         #ifdef DEBUG
         printk(KERN_ALERT "set current running to most prioritized ready task");
         #endif

         current_running = most_prioritized_task;
      } else if (current_running != NULL) { 
         #ifdef DEBUG
         printk(KERN_ALERT "none READY task found .. simply preempt the current running task %d", current_running->pid);
         #endif

         switch_context(current_running, SCHED_NORMAL, 0);
      }
      mutex_unlock(&running_task_lock);
   }

   #ifdef DEBUG
   printk(KERN_ALERT "stopping dispatching return ... ");
   #endif
   return 0;
}


const struct file_operations status_fops = {
     .owner	= THIS_MODULE,
     .write = mp2_write,
     .read = mp2_read,
};


// mp2_init - Called when module is loaded
int __init mp2_init(void){
   #ifdef DEBUG
   printk(KERN_ALERT "MP2 MODULE LOADING\n");
   #endif

   // get /proc/mp2
   mp2_dir = proc_mkdir(DIRECTORY, NULL);
   if (!mp2_dir) {
      return -ENOMEM;
   }

   // get /proc/mp2/status
   status_file = proc_create(FILENAME, 0666, mp2_dir, &status_fops);
   if (!status_file) {
      return -ENOMEM;
   }

   mp2_schedule_info_cache = KMEM_CACHE(mp2_schedule_info_list, SLAB_PANIC);

   current_running = NULL;

   spin_lock_init(&timer_lock); // init timer lock

   dispatching_thread = kthread_run(dispatching_thread_func, NULL, "dispatching_thread");

   printk(KERN_ALERT "MP2 MODULE LOADED\n");
   return 0;   
}


// mp2_exit - Called when module is 
void __exit mp2_exit(void){
   struct mp2_schedule_info_list *cursor;
   struct mp2_schedule_info_list *temp;

   #ifdef DEBUG
   printk(KERN_ALERT "MP2 MODULE UNLOADING\n");
   #endif

   // remove /proc/mp2 file system
   remove_proc_entry(FILENAME, mp2_dir);
   remove_proc_entry(DIRECTORY, NULL);

   kthread_stop(dispatching_thread);

   mutex_destroy(&list_lock);
   mutex_destroy(&running_task_lock);

   list_for_each_entry_safe(cursor, temp, &mp2_list, list) {
      del_timer(&cursor->wakeup_timer);
      list_del(&cursor->list);
      kmem_cache_free(mp2_schedule_info_cache, cursor);
   }

   kmem_cache_destroy(mp2_schedule_info_cache);

   printk(KERN_ALERT "MP2 MODULE UNLOADED\n");
}


// Register init and exit funtions
module_init(mp2_init);
module_exit(mp2_exit);




