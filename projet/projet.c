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
	//Le constructeur doit allouer une structure struct pnlfs_inode_info
	struct pnlfs_inode_info *i;
	i = kmem_cache_alloc(pnl_inode_cachep, GFP_KERNEL);
	//initialiser la struct inode qu’elle embarque avec la fonction inode_init_once() 
	inode_init_once(&i->vfs_inode);
	if (i)
		pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) inode allocated\n");
	//renvoyer l’adresse de cette struct inode	
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
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) inode destroyed\n");
}

static void pnlfs_put_super(struct super_block *sb)
{	//défaire ce que la fonction pnlfs_fill_super() a fait
	kfree(sb->s_fs_info);
	sb->s_fs_info = NULL;
	//TODO mis à jour sur disque ?
}

static const struct super_operations pnlfs_sops = {
	.put_super	= pnlfs_put_super,	
	.alloc_inode	= pnlfs_alloc_inode,
	.destroy_inode	= pnl_destroy_inode,
};

static int pnlfs_readdir(struct file *file, struct dir_context *ctx)
{
	struct pnlfs_inode_info *pnli;
	struct inode *i = file_inode(file);
	uint32_t nr_ents;
	loff_t limit;
	unsigned long curofs = 1;
	limit = i->i_size;
	
	char *name = "foo foo";
	if (!(i->i_mode == S_IFDIR)){
		pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) pnlfs_readdir Not a directory!!\n");
		return -EINVAL;
	}

	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) pnlfs_readdir for dir_i #%lu\n file %pD2, pos=%d\n", i->i_ino, file, (int)ctx->pos);
	if (ctx->pos < 0)
		return -EINVAL;

	if (!dir_emit_dots(file, ctx))
		return 0;

	pnli = PNLFS_I(i); 
	nr_ents = pnli->nr_entries;
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) pnlfs_readdir Number of files/dirs in directory %ld\n", (unsigned long)nr_ents);
	for (i = 0; i < nr_ents; i++) {
		curofs++;
		/* First loop: curofs = 2; pos = 2 */
		if (curofs < ctx->pos) {
			pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) pnlfs_readdir Skipping dirent because curofs %ld < offset %ld\n", curofs, (unsigned long)ctx->pos);
			continue;
		}
		pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) pnlfs_readdir Dirent %ld\n", (unsigned long)ctx->pos);
		//type  = DT_DIR or DT_REG
		if (!dir_emit(ctx, name, strlen(name), i->i_ino, DT_REG))
			break;
		ctx->pos++;
		
	}
	return 0;



/*

	if (ctx->pos == 0) {
		pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) pnlfs_readdir dir_emit_dots\n");
		if (!dir_emit_dots(file, ctx))
			goto out;
		ctx->pos++;
	}

	if (ctx->pos > limit)
		goto out;

	return 0;
out:
	return 0;
	*/
}

static ino_t pnlfs_inode_by_name(struct inode *dir, struct dentry *dentry)
{
  ino_t ino = 0;;
  const char *name = dentry->d_name.name;
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) pnlfs_inode_by_name\n"); 

	//how to know in which dir we are?
	//how to search for correspond ino base on given "name"?
  switch (dir->i_ino) {
  	case 0: //the root directory
    	if (strcmp(name,"foo")==0) ino=1;
    	break;
		default:
			break;
  }
	return (ino);
}
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
	ino_t ino;
  struct inode *ip = NULL;
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) pnlfs_lookup\n"); 
	//get inode base on name
	ino = pnlfs_inode_by_name(dir, dentry);
	if (ino) {
		ip = pnlfs_iget(dir->i_sb, ino);
		if (IS_ERR(ip))
			return ERR_CAST(ip);
	}
	d_add(dentry, ip);
	return NULL;
}

static const struct inode_operations pnlfs_inops = {
	.lookup = pnlfs_lookup,
};


static const struct file_operations pnlfs_operations = {
	.iterate_shared	= pnlfs_readdir,
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
	//TODO Do we have to convert from vfs's inode number to pnlfs's inode number ?
	bh = sb_bread(sb, inode->i_ino);
	if (!bh) {
		pr_info(KERN_INFO "¯\\_(ツ)_/¯ unable to read inode %lu\n", inode->i_ino);
		goto bad_inode;
	}
	raw_inode = (struct pnlfs_inode *) bh->b_data;
	//Initialiser les champs

/*
lecture depuis le disque leX_to_cpu() 
écriture vers le disque cpu_to_leX()
*/	
	if (ino == 0)
		inode->i_mode = S_IFDIR;
	else
		inode->i_mode = (umode_t) le32_to_cpu(raw_inode->mode);	
	inode->i_op     = &pnlfs_inops;
	inode->i_fop    = &pnlfs_operations;
	inode->i_sb     = sb;
	inode->i_ino    = ino;
	inode->i_size   = (loff_t) le32_to_cpu(raw_inode->filesize);
	inode->i_blocks = (blkcnt_t) le32_to_cpu(raw_inode->nr_used_blocks); 
	inode->i_atime  = CURRENT_TIME;
	inode->i_mtime  = CURRENT_TIME;  
	inode->i_ctime  = CURRENT_TIME;

	//TODO How to know if i've init the inode correctly?????
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) Inode:\n"
		"\tinode->i_mode=%u\n"
		"\tinode->i_ino=%lu\n"
		"\tinode->i_size=%lld\n"
		"\tinode->i_blocks=%lu\n",
		inode->i_mode, 
		inode->i_ino, 
		inode->i_size,
		inode->i_blocks);

	unlock_new_inode(inode);
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) new inode ok!\n");
	brelse(bh);
	return inode;
bad_inode:
	iget_failed(inode);
	return ERR_PTR(-EIO);
}

/*
A fill_super() callback implementation has the following arguments:

  struct super_block *sb: the superblock structure. The callback
  	must initialize this properly.

  void *data: arbitrary mount options, usually comes as an ASCII
	string (see "Mount Options" section)

  int silent: whether or not to be silent on error

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
	static int i;
	pnlsb = kmalloc(sizeof(*pnlsb), GFP_KERNEL);
	if (!pnlsb)
		return -ENOMEM;
	pnlsb_info = kzalloc(sizeof(*pnlsb_info), GFP_KERNEL);
	if (!pnlsb_info)
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
	pnlsb_info->nr_blocks = (uint32_t) le32_to_cpu(pnlsb->nr_blocks);
	pnlsb_info->nr_inodes = (uint32_t) le32_to_cpu(pnlsb->nr_inodes);
	pnlsb_info->nr_istore_blocks = (uint32_t) le32_to_cpu(pnlsb->nr_istore_blocks);
	pnlsb_info->nr_ifree_blocks = (uint32_t) le32_to_cpu(pnlsb->nr_ifree_blocks);
	pnlsb_info->nr_bfree_blocks = (uint32_t) le32_to_cpu(pnlsb->nr_bfree_blocks);
	pnlsb_info->nr_free_inodes = (uint32_t) le32_to_cpu(pnlsb->nr_free_inodes);
	pnlsb_info->nr_free_blocks = (uint32_t) le32_to_cpu(pnlsb->nr_free_blocks);


	//how to alocate the ifree_bitmap 
	pnlsb_info->ifree_bitmap = kmalloc(PNLFS_BLOCK_SIZE, GFP_KERNEL);
	//where is the bitmaps?
	//we know it's only 1 bloc	
	//-inside le bloc after sb and nr_istore_blocks
	//-we have to calculate to know where is the bloc of bitmaps
	if (!(bh = sb_bread(sb, pnlsb->nr_istore_blocks + 1))) {
		pr_info(KERN_INFO "¯\\_(ツ)_/¯ unable to read superblock");
		ret = -EIO;
		goto error;
	}
	//TODO Attention, les bitmaps doivent être copiés par lignes de 64 bits !
  /*nombre de lignes =  1*4096*8/64   */
	//il faut 1 lignes de 64 bits at a time
	//we copy 1 byte at a time
	//for(int i = 0; i < nr_ifree_blocks * PNLFS_BLOCK_SIZE * 8 / 64; i++){	
	for(i = 0; i < 512; i=i+16)
		memcpy(pnlsb_info->ifree_bitmap + sizeof(char), bh->b_data, 1); //16 lines (1bytes=16*64bits)
	
	pnlsb_info->bfree_bitmap = kmalloc(PNLFS_BLOCK_SIZE, GFP_KERNEL);
	if (!(bh = sb_bread(sb, pnlsb->nr_istore_blocks + 2))) {
		pr_info(KERN_INFO "¯\\_(ツ)_/¯ unable to read superblock");
		ret = -EIO;
		goto error;
	}
 	for(i = 0; i < 512; i=i+16) 
		memcpy(pnlsb_info->bfree_bitmap + sizeof(char), bh->b_data, 1); 



	sb->s_fs_info = pnlsb_info;
	//Once done, bh should be released
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

The get_sb_bdev( ) VFS function allocates and initializes a new superblock suitable for disk-based
filesystems ; it receives the address of the ext2_fill_super( ) function, which reads the disk
superblock from the Ext2 disk partition.
*/
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
	.name	 = "pnl_VFS",
	.owner 	 = THIS_MODULE,
	.mount	 = pnlfs_mount,
	.kill_sb = kill_pnlfs_super,
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
