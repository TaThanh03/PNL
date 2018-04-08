#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h> 
//#include "task_sample.h"
#include "taskmonitor.h"

MODULE_DESCRIPTION("A process monitor");
MODULE_AUTHOR("TA Thanh et Jan KLOS - UPMC");
MODULE_LICENSE("GPL");
MODULE_VERSION("1");

unsigned short target = 1; /* default pid to monitor */
unsigned frequency = 1; /* sampling frequency */
module_param(target, ushort, 0400);
module_param(frequency, uint, 0600);

struct task_monitor {
	struct pid *pid;
};
struct task_sample {
	cputime_t utime;
	cputime_t stime;
};
struct task_monitor *tm;
struct task_struct *monitor_thread;
int compare_str(char *str1, char *str2)
{
	while (*str1 && *str1 == *str2){
		pr_info("%c:%c\n", *str1, *str2);
		str1++;
		str2++;
	}
	pr_info("%d\n", *str1 - *str2);
	return *str1 - *str2;
}
//////////////////////////////////////////////////////////////
bool get_sample(struct task_monitor *tm, struct task_sample *sample)
{
	struct task_struct *task;
	bool alive = false;
	task = get_pid_task(tm->pid, PIDTYPE_PID);
	if (!task) {
		pr_err("can't find task for pid %u\n", pid_nr(tm->pid));
		goto out;
	}
	task_lock(task);
	alive = pid_alive(task);
	if (alive)
		task_cputime(task, &sample->utime, &sample->stime);
	task_unlock(task);
	put_task_struct(task);
out:
	return alive;
}

void print_sample(struct task_monitor *tm)
{
	struct task_sample ts;
	pid_t pid = pid_nr(tm->pid);
	bool alive;
	alive = get_sample(tm, &ts);
	if (!alive)
		pr_err("%hd is dead\n",	pid);
	else
		pr_info("%hd usr %lu sys %lu\n", pid, ts.utime, ts.stime);
}

void copy_sample_to_buf(struct task_monitor *tm, char *buf)
{
	struct task_sample ts;
	pid_t pid = pid_nr(tm->pid);
	bool alive;
	alive = get_sample(tm, &ts);
	if (!alive)
		sprintf(buf, "%hd is dead\n", pid);
	else
		sprintf(buf, "%d usr %lu sys %lu", pid, ts.utime, ts.stime);
}


int monitor_fn(void *data)
{
	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (schedule_timeout(max(10*HZ/frequency, 1U)))
			return -EINTR;
		print_sample(tm);
	}
	return 0;
}

int monitor_pid(pid_t pid)
{
	struct pid *p = find_get_pid(pid);
	if (!p) {
		pr_err("pid %hu not found\n", pid);
		return -ESRCH;
	}
	tm = kmalloc(sizeof(*tm), GFP_KERNEL);
	tm->pid = p;
	return 0;
}

////////////////////////////////////////////////////////////////////////////////

static struct file_operations file_op =
{
 .owner = THIS_MODULE,
 .read = etx_read,
 .write = etx_write,
 .open = etx_open,
 .unlocked_ioctl = etx_ioctl,
 .release = etx_release,
};
static int major;
char my_string[32];

static int etx_open(struct inode *inode, struct file *file)
{
        printk(KERN_INFO "Device File Opened...!!!\n");
        return 0;
}

static int etx_release(struct inode *inode, struct file *file)
{
        printk(KERN_INFO "Device File Closed...!!!\n");
        return 0;
}

static ssize_t etx_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
        printk(KERN_INFO "Read Function\n");
        return 0;
}
static ssize_t etx_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
        printk(KERN_INFO "Write function\n");
        return 0;
}

static long etx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int err;
	char my_buf[256];
	switch(cmd) {
		case GET_SAMPLE:
			printk(KERN_INFO "etx_ioctl read from device\n");
			copy_sample_to_buf(tm, my_buf);
			copy_to_user((char*) arg, my_buf, sizeof(my_buf));
			break;	
		case TASKMON_STOP:
			printk(KERN_INFO "stop thread\n");
			if (monitor_thread)
				kthread_stop(monitor_thread);
			put_pid(tm->pid);
			kfree(tm);
			break;
		case TASKMON_START:
			if (monitor_thread){
				pr_info("thread already exist!\n");
				break;
			}
			err = monitor_pid(target);
			monitor_thread = kthread_run(monitor_fn, NULL, "monitor_fn");
			if (IS_ERR(monitor_thread)) {
				err = PTR_ERR(monitor_thread);
				goto abort_create_thread;
			}
			break;
		case TASKMON_SET_PID:

			/*
			err = monitor_pid(target);
			monitor_thread = kthread_run(monitor_fn, NULL, "monitor_fn");
			if (IS_ERR(monitor_thread)) {
				err = PTR_ERR(monitor_thread);
				goto abort_create_thread;
			}
			*/
			break;
		default:
			pr_info("user command error\n");
			break;        
	}
	return 0;
abort_create_thread:
	put_pid(tm->pid);
	kfree(tm);
	return err;
}
////////////////////////////////////////////////////////////////////////////////
static int monitor_init(void)
{	
	int err = monitor_pid(target);
	if (err)
		return err;
	//ioctl
	major = register_chrdev(0, "ex_06", &file_op);
	//kthread
	monitor_thread = kthread_run(monitor_fn, NULL, "monitor_fn");
	if (IS_ERR(monitor_thread)) {
		err = PTR_ERR(monitor_thread);
		goto abort;
	}
	pr_info("Monitoring module loaded\n");
	return 0;
abort:
	put_pid(tm->pid);
	kfree(tm);
	return err;
}
module_init(monitor_init);

static void monitor_exit(void)
{
	//ioctl 
	unregister_chrdev(major, "ex_06");
	//kthread
	if (monitor_thread)
		kthread_stop(monitor_thread);
	put_pid(tm->pid);
	kfree(tm);
	pr_info("Monitoring module unloaded\n");
}
module_exit(monitor_exit);

