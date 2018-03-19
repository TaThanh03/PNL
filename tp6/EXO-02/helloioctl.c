#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h> 
#include "helloioctl.h"

MODULE_DESCRIPTION("A helloioctl monitor");
MODULE_AUTHOR("KLOS, TA");
MODULE_LICENSE("GPL");
MODULE_VERSION("1");

static int major;
char my_string[32];

 
static struct file_operations file_op =
{
 .owner = THIS_MODULE,
 .read = etx_read,
 .write = etx_write,
 .open = etx_open,
 .unlocked_ioctl = etx_ioctl,
 .release = etx_release,
};

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
	switch(cmd) {
                case HELLO:
        		printk(KERN_INFO "etx_ioctl read from device\n");
                        copy_to_user((char*) arg, &my_string, sizeof(my_string));
                        break;	
                case WHO:
        		printk(KERN_INFO "etx_ioctl write to device\n");
                        copy_from_user(&my_string ,(char*) arg, sizeof(my_string));
                        printk(KERN_INFO "my_string = %s\n", my_string);
                        break;	
        }
        return -ENOTTY;
}

static int __init helloioctl_init(void)
{
        major = register_chrdev(0, "hello", &file_op);
        pr_info("helloioctl module loaded, major =%d\n",major );
	
	my_string[0] = 'h';
	my_string[1] = 'e';
	my_string[2] = 'l';
	my_string[3] = 'l';
	my_string[4] = 'o';
	my_string[5] = '_';
	my_string[6] = 'i';
	my_string[7] = 'o';
	my_string[8] = 'c';
	my_string[9] = 't';
	my_string[10] = 'l';
	my_string[11] = 0;


        return 0;
}
module_init(helloioctl_init);

static void __exit helloioctl_exit(void)
{
        unregister_chrdev(major, "hello");
        pr_info("helloioctl module unloaded\n");
}
module_exit(helloioctl_exit);

