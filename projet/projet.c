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

static const struct super_operations pnlfs_sops;
static const struct inode_operations pnlfs_inops;
static const struct file_operations pnlfs_operations;

static struct dentry *pnlfs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);
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

static void pnlfs_put_super(struct super_block *sb)
{	//défaire ce que la fonction pnlfs_fill_super() a fait
	kfree(sb->s_fs_info);
	sb->s_fs_info = NULL;
}

static const struct super_operations pnlfs_sops = {
	.put_super	= pnlfs_put_super,	
	.alloc_inode	= pnlfs_alloc_inode,
	.destroy_inode	= pnl_destroy_inode,
};
/*
lookup: called when the VFS needs to look up an inode in a parent
	directory. The name to look for is found in the dentry. This
	method must call d_add() to insert the found inode into the
	dentry. The "i_count" field in the inode structure should be
	incremented. If the named inode does not exist a NULL inode
	should be inserted into the dentry (this is called a negative
	dentry). Returning an error code from this routine must only
	be done on a real error, otherwise creating inodes with system
	calls like create(2), mknod(2), mkdir(2) and so on will fail.
	If you wish to overload the dentry methods then you should
	initialise the "d_dop" field in the dentry; this is a pointer
	to a struct "dentry_operations".
	This method is called with the directory inode semaphore held

	A dentry is an object with a string name (d_name), 
	a pointer to an inode (d_inode), 
	and a pointer to the parent dentry (d_parent).
*/
static struct dentry *pnlfs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)
{
  int ino;
  struct inode *inode;
  const char *name;

  ino = -1;
  inode = NULL;
  name = dentry->d_name.name;

  switch (dir->i_ino) {
  	case 0: //the root directory
    	if (strcmp(name,"foo")==0) ino=1;
    	break;
		default:
			break;
  }

  if (ino>=0) {
    inode=pnlfs_iget(dir->i_sb, ino);
  }	

  d_add(dentry,inode);
  return NULL;
}

static const struct inode_operations pnlfs_inops = {
	.lookup = pnlfs_lookup,
};


static struct inode *pnlfs_iget(struct super_block *sb, unsigned long ino)
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

/*
In general, data pointed to by the s_fs_info field is information from the disk duplicated in memory for
reasons of efficiency. Each disk-based filesystem needs to access and update its allocation bitmaps in order to
allocate or release disk blocks. The VFS allows these filesystems to act directly on the s_fs_info field of
the superblock in memory without accessing the disk.
This approach leads to a new problem, however: the VFS superblock might end up no longer synchronized
with the corresponding superblock on disk. It is thus necessary to introduce an s_dirt flag, which specifies
whether the superblock is dirtythat is, whether the data on the disk must be updated.
*/
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
	pnlsb = kmalloc(sizeof(*pnlsb), GFP_KERNEL);
	if (!pnlsb)
		return -ENOMEM;
	//initialiser la struct super_block passée en paramètre
	/*initialiser les trois premiers champs dans la struct super_block passée en paramètre*/
	sb->s_magic     = PNLFS_MAGIC;
	sb->s_blocksize = PNLFS_BLOCK_SIZE;
	sb->s_maxbytes  = PNLFS_MAX_FILESIZE;
	sb->s_op        = &pnlfs_sops;

	/*Lire ensuite le super bloc ( struct pnl_superblock dans pnlfs.h) 
	sur le périphérique disque à l’aide de l’API buffer head*/
	if (!(bh = sb_bread(sb, PNLFS_SB_BLOCK_NR))) { //Super block is the 0th block
		pr_info(KERN_INFO "¯\\_(ツ)_/¯ unable to read superblock");
		ret = -EIO;
		goto error;
	}
	//Attention, les bitmaps doivent être copiés par lignes de 64 bits ! TODO tips: romfs_blk_read in fs/romfs/storage.c
	memcpy(pnlsb, bh->b_data, PNLFS_BLOCK_SIZE);
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) Superblock: (%ld)\n"
		"\tmagic=%#x\n"
		"\tnr_blocks=%u\n"
		"\tnr_inodes=%u (istore=%u blocks)\n"
		"\tnr_ifree_blocks=%u\n"
		"\tnr_bfree_blocks=%u\n"
		"\tnr_free_inodes=%u\n"
		"\tnr_free_blocks=%u\n",
		sizeof(struct pnlfs_superblock),
		pnlsb->magic, pnlsb->nr_blocks, pnlsb->nr_inodes, pnlsb->nr_istore_blocks,
		pnlsb->nr_ifree_blocks, pnlsb->nr_bfree_blocks, pnlsb->nr_free_inodes,
		pnlsb->nr_free_blocks);
	/*Le champ s fs info peut être utilisé pour stocker des données spécifiques au FS, 
	comme celles de la struct pnlfs_sb_info (pnlfs.h) */
	
	sb->s_fs_info = pnlsb;
	/*
	memcpy(pnlsb_info, bh->b_data, PNLFS_BLOCK_SIZE);
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) Superblock info: (%ld)\n"
		"\tnr_blocks=%u\n"
		"\tnr_inodes=%u (istore=%u blocks)\n"
		"\tnr_ifree_blocks=%u\n"
		"\tnr_bfree_blocks=%u\n"
		"\tnr_free_inodes=%u\n"
		"\tnr_free_blocks=%u\n",
		sizeof(struct pnlfs_sb_info),
		pnlsb_info->nr_blocks, pnlsb_info->nr_inodes, pnlsb_info->nr_istore_blocks,
		pnlsb_info->nr_ifree_blocks, pnlsb_info->nr_bfree_blocks, pnlsb_info->nr_free_inodes,
		pnlsb_info->nr_free_blocks);
	sb->s_fs_info = pnlsb_info;
	*/

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
        if(pnlsb_info)
                kfree(pnlsb_info);
        if(pnlsb)
                kfree(pnlsb);
        return ret;
}

/*
The mount() method must return the root dentry of the tree requested by
caller.  An active reference to its superblock must be grabbed and the
superblock must be locked.  On failure it should return ERR_PTR(error).
*/

/*The get_sb_bdev( ) VFS function allocates and initializes a new superblock suitable for disk-based
filesystems ; it receives the address of the ext2_fill_super( ) function, which reads the disk
superblock from the Ext2 disk partition.*/
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
