#include <linux/ioctl.h>

#define IOCTL_HELLO_TYPE 6969
#define HELLO _IOR(IOCTL_HELLO_TYPE, 3, char*)
#define WHO   _IOW(IOCTL_HELLO_TYPE, 4, char*)

static int etx_open(struct inode *inode, struct file *file);
static int etx_release(struct inode *inode, struct file *file);
static ssize_t etx_read(struct file *filp, char __user *buf, size_t len,loff_t * off);
static ssize_t etx_write(struct file *filp, const char *buf, size_t len, loff_t * off);
static long etx_ioctl(struct file *file, unsigned int cmd, unsigned long arg);


