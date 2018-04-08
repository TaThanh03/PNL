#include <linux/ioctl.h>

#define IOCTL_TASKMONITOR 6969
#define GET_SAMPLE _IOR(IOCTL_TASKMONITOR, 3, char*)
#define TASKMON_STOP _IOR(IOCTL_TASKMONITOR, 4, char*)
#define TASKMON_START _IOR(IOCTL_TASKMONITOR, 5, char*)
#define TASKMON_SET_PID _IOR(IOCTL_TASKMONITOR, 6, char*)

static int etx_open(struct inode *inode, struct file *file);
static int etx_release(struct inode *inode, struct file *file);
static ssize_t etx_read(struct file *filp, char __user *buf, size_t len,loff_t * off);
static ssize_t etx_write(struct file *filp, const char *buf, size_t len, loff_t * off);
static long etx_ioctl(struct file *file, unsigned int cmd, unsigned long arg);



