#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/slab.h>

MODULE_DESCRIPTION("A process monitor");
MODULE_AUTHOR("Jan Klos et TA Thanh");
MODULE_LICENSE("GPL");
MODULE_VERSION("1");

unsigned short target = 1;
module_param(target, ushort, 0400);
unsigned frequency = 1;
module_param(frequency, uint, 0600);

struct task_monitor {
	struct pid *pid;
};
struct task_monitor *my_tm;

int monitor_fn(void *unused)
{
	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (schedule_timeout(max(HZ/frequency, 1U)))
			return -EINTR;
		struct task_struct *task;
		task = get_pid_task(my_tm->pid, PIDTYPE_PID);
	}
	return 0;
}

int monitor_pid(pid_t pid)
{
	struct pid *my_p = find_get_pid(pid);
	if (!my_p){
		pr_err("pid %hu not found\n", pid);
		return -ESRCH;	
	}
	my_tm = kmalloc(sizeof(*my_tm), GFP_KERNEL); 
	my_tm->pid = my_p;
	return 0;
}

static int monitor_init(void)
{
	int err = monitor_pid(target);
	if (err)
		return err;
	pr_info("Monitoring module loaded\n");
}
module_init(monitor_init);

static void monitor_exit(void)
{
	put_pid(my_tm->pid);
	kfree(my_tm);
	pr_info("Monitoring module unloaded\n");
}
module_exit(monitor_exit);

