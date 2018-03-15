#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/slab.h>

MODULE_DESCRIPTION("A hellosysfs monitor");
MODULE_AUTHOR("KLOS, TA");
MODULE_LICENSE("GPL");
MODULE_VERSION("1");

static int toto;

static ssize_t show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        return sprintf("Hello sysfs!\n");
}

static int hellosysfs_init(void)
{
        kobject = kobject_create_and_add("hello", kernel_kobj); 
        if(!kobject) return -ENOMEM;
        static struct kobj_attribute attribute =__ATTR(toto, __ATTR_RO, show,  NULL);
        sysfs_create_file(kobject, attribute);
        pr_info("hellosysfs module loaded\n");
}

static void hellosysfs_exit(void)
{
        kobject_put(kobject);
        pr_info("hellosysfs module unloaded\n");
}

module_init(hellosysfs_init);
module_exit(hellosysfs_exit);

