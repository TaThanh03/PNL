#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/uaccess.h> 
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/init.h>

#include "pnlfs.h"

MODULE_DESCRIPTION("Projet PNL VFS");
MODULE_AUTHOR("Thanh TA et Jan KLOS");
MODULE_LICENSE("GPL");
MODULE_VERSION("1");

//TODO Create a device ?(note in question 3)
/*================================================================*/
static inline struct pnlfs_inode_info *PNLFS_I(struct inode *inode);

static struct inode *pnlfs_alloc_inode(struct super_block *sb);
static void pnlfs_i_callback(struct rcu_head *head);
static void pnl_destroy_inode(struct inode *inode);
static struct inode *pnlfs_iget(struct super_block *sb, unsigned long ino);
const struct inode_operations pnlfs_inops;
const struct file_operations pnlfs_operations;

static struct dentry *pnlfs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data);
static void kill_pnlfs_super(struct super_block *s);

static int pnlfs_fill_super(struct super_block *sb, void *d, int silent);
static void pnlfs_put_super(struct super_block *sb);

static struct kmem_cache *pnl_inode_cachep;
/*================================================================*/
static inline struct pnlfs_inode_info *PNLFS_I(struct inode *inode)
{
	return container_of(inode, struct pnlfs_inode_info, vfs_inode);
}

static struct inode *pnlfs_alloc_inode(struct super_block *sb)
{
	struct pnlfs_inode_info *i;
	i = kmem_cache_alloc(pnl_inode_cachep, GFP_KERNEL);
	return i ? &i->vfs_inode : NULL;
}

static void pnlfs_i_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);
	kmem_cache_free(pnl_inode_cachep, PNLFS_I(inode));
}

static void pnl_destroy_inode(struct inode *inode)
{
	call_rcu(&inode->i_rcu, pnlfs_i_callback);
}

static const struct super_operations pnlfs_sops = {
	.put_super	= pnlfs_put_super,	
	.alloc_inode	= pnlfs_alloc_inode,
	.destroy_inode	= pnl_destroy_inode,
};

struct inode *pnlfs_iget(struct super_block *sb, unsigned long ino)
{
	struct inode *inode;
	struct pnlfs_inode_info *pnli;
	struct pnlfs_inode *raw_inode;
	struct buffer_head * bh;
	
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) inode %lu\n", ino);
	inode = iget_locked(sb, ino);
	if (!inode)
		return ERR_PTR(-ENOMEM);
	//If the inode is in cache
	if (!(inode->i_state & I_NEW)) 
		return inode;
	//If the inode is not in cache
	pnli = PNLFS_I(inode); 
	//Lire cette inode sur le disque
	/*TODO Do we have to convert from vfs's inode number to pnlfs's inode number ?*/
	bh = sb_bread(sb, inode->i_ino);
	if (!bh) {
		pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) unable to read inode %lu\n", inode->i_ino);
		goto bad_inode;
	}
	raw_inode = (struct pnlfs_inode *) bh->b_data;
	//Initialiser les champs
	inode->i_mode = (umode_t) le32_to_cpu(raw_inode->mode);
	inode->i_op = &pnlfs_inops;
	inode->i_fop = &pnlfs_operations;
	//inode->i_sb = &  ?????????????;
	//inode->i_ino =   ?????????????;
	//inode->i_size =   ?????????????;
	//inode->i_blocks =   ?????????????;
	inode->i_atime = CURRENT_TIME;
	inode->i_mtime = CURRENT_TIME;  
	inode->i_ctime = CURRENT_TIME;
	unlock_new_inode(inode);
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) new inode ok!\n");
	return inode;
bad_inode:
	iget_failed(inode);
	return ERR_PTR(-EIO);
}

static struct dentry *pnlfs_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) pnlfs_mount\n");
	return mount_bdev(fs_type, flags, dev_name, data, pnlfs_fill_super);
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

static int pnlfs_fill_super(struct super_block *sb, void *d, int silent)
{
	struct inode *root;
	struct pnlfs_superblock *pnlsb;
	struct pnlfs_sb_info *pnlsb_info;
	struct buffer_head *bh;
	//unsigned long img_size;
	int ret = -EINVAL;

	pnlsb_info = kzalloc(sizeof(*pnlsb_info), GFP_KERNEL);
	if (!pnlsb_info)
		return -ENOMEM;
	/*initialiser la struct super_block passée en paramètre*/
	sb->s_magic = PNLFS_MAGIC;
	sb->s_blocksize = PNLFS_BLOCK_SIZE;
	sb->s_maxbytes = PNLFS_MAX_FILESIZE;
	sb->s_op = &pnlfs_sops;
	/*Lire ensuite le super bloc ( struct pnl_superblock dans pnlfs.h) 
	sur le périphérique disque à l’aide de l’API buffer head*/
	pnlsb = kmalloc(sizeof(*pnlsb), GFP_KERNEL);
	if (!pnlsb)
		return -ENOMEM;
	if (!(bh = sb_bread(sb, PNLFS_SB_BLOCK_NR))) { //Super block is the 0th block
		pr_info(KERN_INFO "¯\\_(ツ)_/¯ unable to read superblock");
		ret = -EIO;
		goto error;
	}
	//TODO Attention, les bitmaps doivent être copiés par lignes de 64 bits ! TODO tips: romfs_blk_read in storage.c
	memcpy(pnlsb, bh->b_data, PNLFS_BLOCK_SIZE);
	
	//img_size = le32_to_cpu(pnlsb->size);
	//sb->s_fs_info = (void *) img_size;
	sb->s_fs_info = pnlsb;
	//TODO Once done, bh should be released using: brelse(bh);
	brelse(bh);
	//récupérer l’inode racine du disque (inode 0)	
	root = pnlfs_iget(sb, 0);
	if (IS_ERR(root)) {
		pr_err("get root inode failed\n");
		return PTR_ERR(root);
	}
	//définir ses propriétaires
	inode_init_owner(root, NULL, root->i_mode);
	//puis la déclarer comme inode racine
	sb->s_root = d_make_root(root);
	if (!(sb->s_root)) {
		pr_err("get root dentry failed\n");
		return -ENOMEM;
	}
	return 0;
error:
	sb->s_fs_info = NULL;
	kfree(pnlsb_info);
	return ret;
}

static void pnlfs_put_super(struct super_block *sb)
{	//défaire ce que la fonction pnlfs_fill_super() a fait
	kfree(sb->s_fs_info);
	sb->s_fs_info = NULL;
}

static void pnlfs_init_once(void *p)
{
	struct pnlfs_inode_info *i = p;
	inode_init_once(&i->vfs_inode);
}

static int pnlfs_init(void)
{
	int err;
	err = register_filesystem(&pnlfs_type);
	if (err < 0)
		goto error_register_filesystem;

	pnl_inode_cachep = kmem_cache_create("pnl_inode_cache", 
											sizeof(struct pnlfs_inode_info), 0,
			    						SLAB_RECLAIM_ACCOUNT|SLAB_MEM_SPREAD|SLAB_ACCOUNT,
			    						pnlfs_init_once);
	if (pnl_inode_cachep == NULL)
		goto error_kmem_cache_create;

	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) module loaded\n");
	return 0;
/*ERROR HANDLE*/
error_kmem_cache_create:
	pr_info(KERN_INFO "¯\\_(ツ)_/¯ kmem_cache_create failed");
	unregister_filesystem(&pnlfs_type);
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
