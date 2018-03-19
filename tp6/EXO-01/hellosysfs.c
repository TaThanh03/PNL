#include <linux/module.h>
#include <linux/timer.h>
#include <linux/kernel_stat.h>
#include <linux/printk.h> 
#include <linux/kobject.h> 
#include <linux/sysfs.h> 
#include <linux/init.h> 
#include <linux/fs.h> 
#include <linux/string.h> 

MODULE_DESCRIPTION("A hello kernel module");
MODULE_AUTHOR("Maxime Lorrillere <maxime.lorrillere@lip6.fr>");
MODULE_LICENSE("GPL");

static char buff_hello[7] ;
static ssize_t hello_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
        return sprintf(buf, "Hello %s\n", buff_hello);
}
static ssize_t hello_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	//strncpy(buff_hello, buf, 32);
	snprintf(buff_hello, strlen(buff_hello), "%s", buf);
	return count;
}
static struct kobj_attribute hello_attribute = __ATTR_RW(hello);
static int hellosysfs_init(void)
{
	pr_info("hellosysfs module loaded\n");
	sysfs_create_file(kernel_kobj, &hello_attribute.attr); 
	buff_hello[0] = 'b';
	buff_hello[1] = 'l';
	buff_hello[2] = 'a';
	buff_hello[3] = 'b';
	buff_hello[4] = 'l';
	buff_hello[5] = 'a';
	buff_hello[6] = 0;
	return 0;
}
module_init(hellosysfs_init);

static void hellosysfs_exit(void)
{
	pr_info("hellosysfs module unloaded\n");
	sysfs_remove_file(kernel_kobj, &hello_attribute.attr);
}
module_exit(hellosysfs_exit);
