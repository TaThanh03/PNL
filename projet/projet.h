#include <linux/ioctl.h>

#define IOCTL_HELLO_TYPE 6969
#define HELLO _IOR(IOCTL_HELLO_TYPE, 3, char*)
#define WHO   _IOW(IOCTL_HELLO_TYPE, 4, char*)

static int etx_open(struct inode *inode, struct file *file);
static int etx_release(struct inode *inode, struct file *file);
static ssize_t etx_read(struct file *filp, char __user *buf, size_t len,loff_t * off);
static ssize_t etx_write(struct file *filp, const char *buf, size_t len, loff_t * off);
static long etx_ioctl(struct file *file, unsigned int cmd, unsigned long arg);


// Function to set the bits
// Example:
// n = 00000001
// m = 10
// i = 2, j = 4
//	Result 
//	Bits:  00001001
//  Index: 76543210
uint64_t setBits(uint64_t n, uint64_t m, int i, int j)
{
    // number with all 1's
    uint64_t allOnes = ~0;
 
    // Set all the bits in the left of j
    uint64_t left = allOnes << (j + 1);
 
    // Set all the bits in the right of j
    uint64_t right = ((1 << i) - 1);
 
    // Do Bitwsie OR to get all the bits 
    // set except in the range from i to j
    uint64_t mask = left | right;
 
    // clear bits j through i
    uint64_t masked_n = n & mask;
 
    // move m into the correct position
    uint64_t m_shifted = m << i;
 
    // return the Bitwise OR of masked_n 
    // and shifted_m
    return (masked_n | m_shifted);
}
