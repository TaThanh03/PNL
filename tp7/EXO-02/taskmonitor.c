#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/list.h>
MODULE_DESCRIPTION("A process monitor");
MODULE_AUTHOR("Maxime Lorrillere <maxime.lorrillere@lip6.fr>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1");

unsigned short target = 1; /* default pid to monitor */
unsigned frequency = 1; /* sampling frequency */
module_param(target, ushort, 0400);
module_param(frequency, uint, 0600);

struct mutex mutex_list;
struct task_sample {
	cputime_t utime;
	cputime_t stime;
	struct list_head ts_item;
};
struct task_monitor {
	struct pid *pid;
	struct list_head ts_head;
	int ts_cpt;
};
struct task_monitor *tm;
//thread qui surveille le target
struct task_struct *monitor_thread;

static char user_command[32];


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
	ts_new = kmalloc(sizeof(struct task_sample), GFP_KERNEL);
	//l’initialiser à l’aide de la fonction get_sample
	pid_t pid = pid_nr(tm->pid);
	bool alive;
	get_sample(tm, ts_new);
	alive = get_sample(tm, ts_new);
	if (!alive)
		pr_err("%hd is dead\n",	pid);
	else{
		//l’ajouter dans la liste du struct task_monitor
		INIT_LIST_HEAD(&ts_new->ts_item);	
		mutex_lock(&mutex_list);
  	list_add(&(ts_new->ts_item), &(tm->ts_head));
		tm->ts_cpt++;
		pr_info("%hd usr %lu sys %lu\n", pid, ts_new->utime, ts_new->stime);
		mutex_unlock(&mutex_list);
	}
}

int monitor_fn(void *data)
{
	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (schedule_timeout(max(3*HZ/frequency, 1U)))
		//if (schedule_timeout(1U)) //pour shrinker
			return -EINTR;
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
	tm->ts_cpt = 0;
	INIT_LIST_HEAD(&tm->ts_head);	
	return 0;
}
//sysfs/////////////////////////////////////////////////////////////
static ssize_t taskmonitor_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct task_sample *ts_index;
	pid_t pid = pid_nr(tm->pid);
	static char *buf_show;
	buf_show=kmalloc(PAGE_SIZE,GFP_KERNEL);
	int count = 0;
	int taille = 0;
	mutex_lock(&mutex_list);
	pr_info("line total=%d\n", tm->ts_cpt);
	list_for_each_entry(ts_index, &tm->ts_head, ts_item) {
		//access the member from ts_index
		taille = sprintf(buf_show, "%d usr %lu sys %lu \n", pid, ts_index->utime, ts_index->stime);
		if((count + taille) <= PAGE_SIZE){
			scnprintf(buf + count, taille, "%s", buf_show);	
			count += taille;
		}
	}
	mutex_unlock(&mutex_list);
	return count;

}

static ssize_t taskmonitor_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int err;
	snprintf(user_command, sizeof(user_command), "%.*s", (int)min(count, sizeof(user_command)-1), buf);
	if (strcmp("stop", user_command) == 0){
		if (monitor_thread)
			kthread_stop(monitor_thread);
		//put_pid(tm->pid);
		//kfree(tm);
	} else if (!strcmp("start", user_command)){
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
////////////////////////////////////////////////////////////////////
static int monitor_init(void)
{
	struct task_sample *ts_index, *tmp;
	//mutex
	mutex_init(&mutex_list);
	//pid et init kthread
	int err = monitor_pid(target);
	if (err)
		return err;
	monitor_thread = kthread_run(monitor_fn, NULL, "monitor_fn");
	if (IS_ERR(monitor_thread)) {
		err = PTR_ERR(monitor_thread);
		goto abort;
	}
	pr_info("kthread loaded\n");
	//sysfs
	err = sysfs_create_file(kernel_kobj, &taskmonitor_attribute.attr); 
	if (err)
		goto sysfs_error;
	pr_info("sysfs loaded\n");
	pr_info("Monitoring module loaded(everything is OK)\n");
	return 0;
sysfs_error:
	if (monitor_thread)
		kthread_stop(monitor_thread); 
	list_for_each_entry_safe(ts_index, tmp, &tm->ts_head, ts_item){
		printk(KERN_INFO "freeing node usr %lu sys %lu\n", ts_index->utime, ts_index->stime);
		list_del(&ts_index->ts_item);
		kfree(&ts_index->ts_item);
	}
	mutex_destroy(&mutex_list);
abort:
	put_pid(tm->pid);
	kfree(tm);
	return err;
}
module_init(monitor_init);

static void monitor_exit(void)
{
	struct task_sample *ts_index, *tmp;
	//stop kthread
	if (monitor_thread)
		kthread_stop(monitor_thread);
	//delete list
	list_for_each_entry_safe(ts_index, tmp, &tm->ts_head, ts_item){
		printk(KERN_INFO "freeing node usr %lu sys %lu\n", ts_index->utime, ts_index->stime);
		list_del(&ts_index->ts_item);
		kfree(&ts_index->ts_item);
	}
	pr_info("number of line =%d\n", tm->ts_cpt);
	pr_info(KERN_INFO "deleting the list\n");
	//release pid
	put_pid(tm->pid);
	kfree(tm);
	//remove sysfs
	sysfs_remove_file(kernel_kobj, &taskmonitor_attribute.attr);
	pr_info(KERN_INFO "sysfs removed\n");
	//destroy mutex	
	mutex_destroy(&mutex_list);
	pr_info(KERN_INFO "mutex destroyed\n");
	pr_info(KERN_INFO "Monitoring module unloaded(everything is OK)\n");
}
module_exit(monitor_exit);

