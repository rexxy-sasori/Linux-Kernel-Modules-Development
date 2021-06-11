
Design Methodology:

Add new process to the linked list:
Use copy_from_user to move the pid to kernel space. 
Set the pid of new element and initialize the cpu_time to 0


Read linked list when calling cat /proc/mp1/status:
Use simple_read_from_buffer to move all process data to user space
Iterate through the linked list and use sprintf to set the kernal buffer 

Both read and write function use spin_lock to ensure successive writing/reading to/from the linked list

Work function:
Use the given function to update the linked list with new cpu_time every 5ms (conversion using jiffies.h)
Remove the linked list if process has terminated (lock protected)

Work gets scheduled by work_queue every 5ms when timer wakes up