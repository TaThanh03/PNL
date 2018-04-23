#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/uaccess.h> 
#include <linux/buffer_head.h>
#include <linux/slab.h>

#include "pnlfs.h"

MODULE_DESCRIPTION("Projet PNL VFS");
MODULE_AUTHOR("Thanh TA et Jan KLOS");
MODULE_LICENSE("GPL");
MODULE_VERSION("1");

//TODO Create a device ?(note in question 3)


static int major;
static struct kmem_cache *pnl_inode_cachep;

static void pnl_put_super(struct super_block *sb);

static struct inode *pnl_alloc_inode(struct super_block *sb)
{
	struct pnlfs_inode_info *pnl_inode;
	pnl_inode = kmem_cache_alloc(pnl_inode_cachep, GFP_KERNEL);
	if (!pnl_inode)
		return NULL;
	inode_init_once(&pnl_inode->vfs_inode);

	return &i->vfs_inode;
}

static inline struct pnlfs_inode_info *PNLFS_I(struct inode *inode)
{
	return container_of(inode, struct pnlfs_inode_info, vfs_inode);
}

static void pnl_i_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);
	kmem_cache_free(pnl_inode_cachep, PNLFS_I(inode));
}

static void pnl_destroy_inode(struct inode *inode)
{
	call_rcu(&inode->i_rcu, pnl_i_callback);
}

static const struct super_operations pnlfs_sops = {
	.put_super	= pnl_put_super,	
	.alloc_inode	= pnl_alloc_inode,
	.destroy_inode	= pnl_destroy_inode,
};

static void pnl_put_super(struct super_block *sb)
{
	//défaire ce que la fonction pnl_fill_super() a fait
	kfree(sb->s_fs_info);
	sb->s_fs_info = NULL;
}

static int pnl_fill_super(struct super_block *sb, void *d, int silent);

static struct dentry *pnlfs_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) pnlfs_mount\n");
	return mount_bdev(fs_type, flags, dev_name, data, pnl_fill_super);
}

static void kill_pnlfs_super(struct super_block *s)
{
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) kill_block_super\n");
	kill_block_super(s);
}

static struct file_system_type pnlfs_type = {
	    .name			= "pnl_VFS",
	    .owner 		= THIS_MODULE,
			.mount		= pnlfs_mount,
			.kill_sb	= kill_pnlfs_super,
};
MODULE_ALIAS_FS("pnl_VFS");


static int pnl_fill_super(struct super_block *sb, void *d, int silent)
{
	//struct pnlfs_superblock *sb_
	struct pnlfs_sb_info *pnlsb;
	struct buffer_head *bh;
	int ret = -EINVAL;

	pnlsb = kzalloc(sizeof(*pnlsb), GFP_KERNEL);
	if (!pnlsb)
		return -ENOMEM;
	sb->s_fs_info = pnlsb;
	/*initialiser la struct super_block passée en paramètre*/
	sb->s_magic = PNLFS_MAGIC;
	sb->s_blocksize = PNLFS_BLOCK_SIZE;
	sb->s_maxbytes = PNLFS_MAX_FILESIZE;
	sb->s_op = &pnlfs_sops;
	/*Lire ensuite le super bloc ( struct pnl_superblock dans pnlfs.h) 
	sur le périphérique disque à l’aide de l’API buffer head*/
	if (!(bh = sb_bread(sb, PNLFS_SB_BLOCK_NR))) { //Super block is the 0th block
		pr_info(KERN_INFO "¯\\_(ツ)_/¯ unable to read superblock");
		ret = -EIO;
		goto error;
	}
	//TODO Once done, bh should be released using: brelse(bh);
	//TODO the relationship between struct super_block *sb and struct pnl_superblock dans pnlfs.h ????
	//memcpy(sb, bh->b_data, SIMULA_FS_BLOCK_SIZE);
	brelse(bh);

	//TODO Attention, les bitmaps doivent être copiés par lignes de 64 bits !
	return -1;
error:
	sb->s_fs_info = NULL;
	kfree(pnlsb);
	return ret;
}

static int pnlfs_init(void)
{
	int err;
	err = register_filesystem(&pnlfs_type);
	if (err < 0)
		goto error_register_filesystem;

	pnl_inode_cachep = 
		kmem_cache_create("pnl_inode_cache", 
											sizeof(struct pnl_inode_info), 
											0,
			    						SLAB_RECLAIM_ACCOUNT|SLAB_MEM_SPREAD|SLAB_ACCOUNT,
			    						init_once);
	if (pnl_inode_cachep == NULL)
		goto error_kmem_cache_create;



	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) module loaded\n");
	return 0;
/*ERROR HANDLE*/
error_kmem_cache_create:
	unregister_filesystem(&pnlfs_type);
	pr_info(KERN_INFO "¯\\_(ツ)_/¯ kmem_cache_create failed");
	return -ENOMEM;
error_register_filesystem:
	pr_info(KERN_INFO "¯\\_(ツ)_/¯ register_filesystem failed");
	return -1;
}
module_init(pnlfs_init);

static void pnlfs_exit(void)
{
	unregister_filesystem(&pnlfs_type);
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) module unloaded\n");
}
module_exit(pnlfs_exit);
