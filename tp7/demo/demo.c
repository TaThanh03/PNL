#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/shrinker.h>
#include <linux/mempool.h>

MODULE_DESCRIPTION("A process monitor");
MODULE_AUTHOR("Maxime Lorrillere <maxime.lorrillere@lip6.fr>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1");

unsigned short target = 1; /* default pid to monitor */
unsigned frequency = 1; /* sampling frequency */

module_param(target, ushort, 0400);
module_param(frequency, uint, 0600);

struct task_monitor {
	struct list_head head;
	struct mutex m;
	int compteur;
	struct pid *pid;
};

struct task_monitor *tm;

struct shrink_control *sc;

struct task_struct *monitor_thread;

static struct kmem_cache *kmem_cache_object;

static mempool_t *mempool_object;

struct task_sample {
	struct list_head list;
	//struct mutex m;
	struct kref ref;
	cputime_t utime;
	cputime_t stime;
};

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
	if (alive){
		task_cputime(task, &sample->utime, &sample->stime);

	}
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
	struct task_sample* ts;
	bool alive;
	//ts = kmalloc(sizeof(struct task_sample),GFP_KERNEL);
	pr_info("avant cacche");
	//ts = kmem_cache_alloc(kmem_cache_object,GFP_KERNEL); Exo 3
	ts=mempool_alloc_slab(GFP_KERNEL,kmem_cache_object);
	pr_info("apres cache");
	alive = get_sample(tm,ts);

	INIT_LIST_HEAD(&(ts->list));
	//mutex_init(&ts->m);

	if (alive){
		mutex_lock(&tm->m);
		//mutex_lock(&ts->m);
		list_add(&(ts->list),&(tm->head));
		tm->compteur++;
		pr_info("%lu %lu",sizeof(struct task_sample),ksize(ts));
		//mutex_unlock(&ts->m);
		mutex_unlock(&tm->m);
	}		
}

int monitor_fn(void *data)
{
	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		//if (schedule_timeout(1U)		
		if (schedule_timeout(max(HZ/frequency,1U)))
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
	//struct list_head* head=kmalloc(sizeof(struct list_head),GFP_KERNEL);
	//tm->head=*head;
	INIT_LIST_HEAD(&tm->head);
	tm->compteur=0;
	//struct mutex* mut=kmalloc(sizeof(struct mutex),GFP_KERNEL);
	//tm->m=*mut;
	mutex_init(&tm->m);

	return 0;
}


static ssize_t taskmonitor_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	struct task_sample *pos;
	int count=0;
	int pid=pid_nr(tm->pid);
	int taille=0;
	char * buffer;
	buffer=kmalloc(500,GFP_KERNEL);
	mutex_lock(&tm->m);

	//char buff[PAGE_SIZE];
	list_for_each_entry(pos,&tm->head,list){/*il ignore la premiere element de la list de type monitor*/
		//mutex_lock(&pos->m);
		taille = scnprintf(buffer,500,"pid %hd usr %lu sys %lu \n",pid, pos->utime,pos->stime);
		if((count+taille) <= PAGE_SIZE){
			scnprintf(buf+count,taille,"%s",buffer);
			count+=taille;
		}
		else
			break;
	//mutex_unlock(&pos->m);
	}
	mutex_unlock(&tm->m);
	//kfree(buffer);
	return count;
}


static struct kobj_attribute taskmonitor_attribute =__ATTR_RO(taskmonitor);

static unsigned long count_objects(struct shrinker* sh,struct shrink_control* sc){
	//struct task_sample *pos;
	unsigned long count=0;
	mutex_lock(&tm->m);
	// pr_info("count_obj");
	// list_for_each_entry(pos,&tm->head,list){il ignore la premiere element de la list de type monitor
	// 	//mutex_lock(&pos->m);
	// 	count++;
	// 	//mutex_unlock(&pos->m);
	count=tm->compteur;

	// }
	// pr_info("fin_count  %d");
	mutex_unlock(&tm->m);

	sc->nr_to_scan=tm->compteur;
	pr_info("count_objects %lu \n",count);
	return count;
}

static unsigned long scan_objects(struct shrinker* sh,struct shrink_control* sc){
	struct task_sample *pos;
	unsigned long count=0;
	struct task_sample* tmp;
	mutex_lock(&tm->m);
	pr_info("scan_obj");
	if(sc->nr_to_scan==0)return 0;
	else{
		list_for_each_entry_safe(pos,tmp,&tm->head,list){
		//mutex_lock(&pos->m);
			if(sc->nr_to_scan>0){
				list_del(&pos->list);
				kfree(&pos->list);
				tm->compteur--;
				sc->nr_to_scan--;
			}
			else
				break;
		//mutex_unlock(&pos->m);
		}
	}
	mutex_unlock(&tm->m);
	return count;
}
static struct shrinker sh ={
	.count_objects=&count_objects,
	.scan_objects=&scan_objects,
	.seeks=DEFAULT_SEEKS,
};


static int monitor_init(void)
{
	int err = monitor_pid(target);
	int RET = -1;
	struct task_sample* ts;
	struct task_sample* tmp;
	if (err)
		return err;
	monitor_thread = kthread_run(monitor_fn, NULL, "monitor_fn");
	if (IS_ERR(monitor_thread)) {
		err = PTR_ERR(monitor_thread);
		goto abort;
	}
	pr_info("Monitoring module loaded\n");
	err=sysfs_create_file(kernel_kobj,&(taskmonitor_attribute.attr));
	if (err){
		goto sysfs_error;
	}
	pr_info("taskmonitor via /sys/kernel module loaded\n");
	/*Exo 2*/
	RET=register_shrinker(&sh);
	if(RET<0) goto register_error;
	pr_info("shrinker on");
	/*Exo 3*/
	kmem_cache_object=KMEM_CACHE(task_sample,0);
	if(!kmem_cache_object) {
		pr_err("KMEM_CACHE ERR");
		goto kmem_cache_error;
	}
	/*Exo4*/
	mempool_object =(mempool_t*) mempool_create_slab_pool(5,kmem_cache_object);
	if(!mempool_object){
		pr_err("MEMPOOL ERR");
		goto mempool_error;
	}
	pr_info("all good");
	return 0;
mempool_error:
	kmem_cache_destroy(kmem_cache_object);
kmem_cache_error:
	unregister_shrinker(&sh);
register_error:
	sysfs_remove_file(kernel_kobj,&taskmonitor_attribute.attr);
sysfs_error:
	if (monitor_thread)
		kthread_stop(monitor_thread);
	list_for_each_entry_safe(ts,tmp,&tm->head,list){
		list_del(&ts->list);
		kfree(&ts->list);
		//kmem_cache_free(kmem_cache_object,ts); Exo 3
		/*Exo4*/
		mempool_free_slab(ts,kmem_cache_object);
	}
	mutex_destroy(&tm->m);
abort:
	put_pid(tm->pid);
	kfree(tm);
	return err;
}

static void monitor_exit(void)
{
	struct task_sample* ts;
	struct task_sample* tmp;
	if (monitor_thread)
		kthread_stop(monitor_thread);
	list_for_each_entry_safe(ts,tmp,&tm->head,list){
		list_del(&ts->list);
		kfree(&ts->list);
		//kmem_cache_free(kmem_cache_object,ts);EXo3
		/*Exxo4*/
		mempool_free_slab(ts,kmem_cache_object);
	}
	list_del(&tm->head);
	put_pid(tm->pid);
	mutex_destroy(&tm->m);
	kfree(tm);

	sysfs_remove_file(kernel_kobj,&taskmonitor_attribute.attr);
	/*Exo 2*/
	unregister_shrinker(&sh);
	/*Exo 3*/
	kmem_cache_destroy(kmem_cache_object);
	/*Exo 4*/
	mempool_destroy(mempool_object);
	pr_info("Monitoring module unloaded\n");
}
module_init(monitor_init);
module_exit(monitor_exit);

