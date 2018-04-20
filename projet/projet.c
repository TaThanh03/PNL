#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/vfs.h>
MODULE_DESCRIPTION("Projet PNL VFS");
MODULE_AUTHOR("Thanh TA et Jan KLOS");
MODULE_LICENSE("GPL");
MODULE_VERSION("1");



/*
TODO Create a device (note in question 3)
*/

static struct dentry *pnlfs_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	pr_info(KERN_INFO "[pnlfs] pnlfs_mount\n"); // TODO this never been call maybe i'm missing the device?
	//return mount_bdev(fs_type, flags, dev_name, data, efs_fill_super);
	return -1;
}

static void kill_pnlfs_super(struct super_block *s)
{
	pr_info(KERN_INFO "[pnlfs] kill_block_super\n");
	//struct efs_sb_info *sbi = SUPER_INFO(s);
	kill_block_super(s);
	//kfree(sbi);
}

static struct file_system_type pnlfs_type = {
	    .name			= "pnl_VFS",
	    .owner 		= THIS_MODULE,
			.mount		= pnlfs_mount,
			.kill_sb	= kill_pnlfs_super,
};
MODULE_ALIAS_FS("pnl_VFS");

static int pnlfs_init(void)
{
	int err;
	err = register_filesystem(&pnlfs_type);
	if (err < 0)
		goto error_register_filesystem;
	pr_info(KERN_INFO "[pnlfs] module loaded(everything is OK)\n");
	return 0;
/*ERROR HANDLE*/
error_register_filesystem:
	pr_info(KERN_INFO "[pnlfs] register failed");
	return -1;
}
module_init(pnlfs_init);

static void pnlfs_exit(void)
{
	unregister_filesystem(&pnlfs_type);
	pr_info(KERN_INFO "[pnlfs] module unloaded(everything is OK)\n");
}
module_exit(pnlfs_exit);

