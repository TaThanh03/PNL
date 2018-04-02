#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/mutex.h>

MODULE_DESCRIPTION("A process monitor");
MODULE_AUTHOR("Maxime Lorrillere <maxime.lorrillere@lip6.fr>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1");

unsigned short target = 1; /* default pid to monitor */
unsigned frequency = 1; /* sampling frequency */
module_param(target, ushort, 0400);
module_param(frequency, uint, 0600);


struct task_monitor {
	struct pid *pid;
};
struct task_monitor *tm;

//thread qui surveille le target
struct task_struct *monitor_thread;

struct task_sample {
	struct list_head list;
	cputime_t utime;
	cputime_t stime;
};
struct task_sample ts_head;

struct mutex mutex_list;

static char user_command[32];
static int sample_list_cpt = 0;

int compare_str(char *str1, char *str2)
{
	while (*str1 && *str1 == *str2){
		str1++;
		str2++;
	}
	return *str1 - *str2;
}

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

void save_sample(void)
{
	//allouer une nouvelle struct task_sample
	struct task_sample *ts_new;
	ts_new = kmalloc(sizeof(*ts_new), GFP_KERNEL);
	//l’initialiser à l’aide de la fonction get_sample
	get_sample(tm, ts_new);
	//l’ajouter dans la liste du struct task_monitor
	INIT_LIST_HEAD(&ts_new->list);	
	mutex_lock(&mutex_list);
	list_add_tail(&(ts_new->list), &(ts_head.list));
	mutex_unlock(&mutex_list);
}

int monitor_fn(void *data)
{
	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (schedule_timeout(max(10*HZ/frequency, 1U)))
			return -EINTR;
		//print_sample(tm);
		save_sample();
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

static ssize_t taskmonitor_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct task_sample ts;
	pid_t pid = pid_nr(tm->pid);
	static char buf_show[32];
	bool alive;
	alive = get_sample(tm, &ts);
	if (!alive)
		sprintf(buf_show, "%d is dead", pid);	
	else
		sprintf(buf_show, "%d usr %lu sys %lu", pid, ts.utime, ts.stime);
	return scnprintf(buf, PAGE_SIZE, "%s\n", buf_show);
}

/*
The arguments for "%.*s"  are the string width and the target string. It's syntax is :
printf("%.*s", string_width, string);
*/
static ssize_t taskmonitor_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int err;
	snprintf(user_command, sizeof(user_command), "%.*s", (int)min(count, sizeof(user_command)-1), buf);
	if (compare_str("stop", user_command) == 0){
		if (monitor_thread)
			kthread_stop(monitor_thread);
		put_pid(tm->pid);
		kfree(tm);
	} else if (!compare_str("start", user_command)){
		err = monitor_pid(target);
		monitor_thread = kthread_run(monitor_fn, NULL, "monitor_fn");
		if (IS_ERR(monitor_thread)) {
			err = PTR_ERR(monitor_thread);
			goto abort_store;
		}
	} else {
		pr_info("user command error %s\n", user_command);
	}	
	return count;
abort_store:
	put_pid(tm->pid);
	kfree(tm);
	return err;
}
static struct kobj_attribute taskmonitor_attribute = __ATTR_RW(taskmonitor);

static int monitor_init(void)
{
	//init list head
	LIST_HEAD_INIT(&ts_head->list);	
	//mutex
	mutex_init(&mutex_list);
	//sysfs
	sysfs_create_file(kernel_kobj, &taskmonitor_attribute.attr); 
	int err = monitor_pid(target);
	if (err)
		return err;
	pr_info("Monitoring module loaded\n");
	return 0;
}
module_init(monitor_init);

static void monitor_exit(void)
{
	struct task_sample *ts_index;
	list_for_each_entry(ts_index, &ts_head->list, list) {
        //access the member from ts_index
        printk(KERN_INFO "usr %lu sys %lu", ts_index->utime, ts_index->stime);
  }
  printk(KERN_INFO "n");

	//sysfs
	sysfs_remove_file(kernel_kobj, &taskmonitor_attribute.attr);
	pr_info("Monitoring module unloaded\n");
}
module_exit(monitor_exit);
