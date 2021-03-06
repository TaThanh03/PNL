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

static ino_t pnlfs_inode_by_name(struct inode *dir, struct dentry *dentry);
static int pnlfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
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
		pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) [pnlfs_alloc_inode] inode allocated\n");
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
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) [pnl_destroy_inode] inode destroyed\n");
}

static void pnlfs_put_super(struct super_block *sb)
{	
	//TODO mettre à jour sur disque ex10
	//défaire ce que la fonction pnlfs_fill_super() a fait
	kfree(sb->s_fs_info);
	sb->s_fs_info = NULL;
}

static const struct super_operations pnlfs_sops = {
	.put_super	= pnlfs_put_super,	
	.alloc_inode	= pnlfs_alloc_inode,
	.destroy_inode	= pnl_destroy_inode,
};

static int pnlfs_readdir(struct file *file, struct dir_context *ctx)
{
	struct buffer_head * bh;
	struct pnlfs_inode_info *dir_info;
	struct inode *i_dir = file_inode(file);
	struct pnlfs_dir_block *dir_block;
	uint32_t nr_ents;
	int i, pos_start;

	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) [pnlfs_readdir] directory #%lu file %pD2, pos=%d\n", i_dir->i_ino, file, (int)ctx->pos);
	if (!(i_dir->i_mode == S_IFDIR)){
		pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) [pnlfs_readdir] Not a directory!!\n");
		return -EINVAL;
	}
	if (ctx->pos < 0)
		return -EINVAL;
	//ctx->pos == 2 after the first "dir_emit_dots"
	if (ctx->pos == 0){
		dir_emit_dots(file, ctx);
		return 0;
	}
	//Lire directory meta-info
	dir_info = PNLFS_I(i_dir); 
	//nr_ents = the number of files/directories in this directory
	nr_ents = dir_info->nr_entries;
	//Use nr_ents and ctx->pos to find out how many elements left we have to add to dir_context
	//Example: if nr_ents=10 but ctx->pos=4 => we have to add 8 mores to dir_context
	//If directory already filled-out, return immediatement
	if (ctx->pos >= (nr_ents + 2))
		return 0;
	//Else start filling the missing elements
	pos_start = ctx->pos - 2;
	bh = sb_bread(i_dir->i_sb, dir_info->index_block);
	if (!bh) 
		pr_info(KERN_INFO "¯\\_(ツ)_/¯ [pnlfs_readdir] unable to read directory %lu\n", i_dir->i_ino);
	dir_block = (struct pnlfs_dir_block *) bh->b_data;
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) [pnlfs_readdir] nr_entries=%ld\n", (unsigned long)nr_ents);
	for (i = pos_start; i < nr_ents; i++) {
		dir_emit(ctx, 
			 dir_block->files[i].filename, 
			 strlen(dir_block->files[i].filename), 
			 le32_to_cpu(dir_block->files[i].inode), 
			 DT_UNKNOWN);
		ctx->pos++;
	}
	brelse(bh);
	return 0;
}

//Return an inode coressond with the name we search for
static ino_t pnlfs_inode_by_name(struct inode *dir, struct dentry *dentry)
{
	const char *name = dentry->d_name.name;
	ino_t ino = 0; 
	struct buffer_head *bh;
	struct pnlfs_dir_block *dir_block;
	struct pnlfs_inode_info *dir_info;
	int i;
	
	//lire directory meta-info
	dir_info = PNLFS_I(dir); 
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) [pnlfs_inode_by_name] Directory infos (Inode infos):\n"
		"\ti_ino=%lu\n"
		"\tindex_block=%u\n"
		"\tnr_entries=%u\n",
		dir_info->vfs_inode.i_ino,
		dir_info->index_block,
		dir_info->nr_entries
	);
	bh = sb_bread(dir->i_sb, dir_info->index_block);
	if (!bh) 
		pr_info(KERN_INFO "¯\\_(ツ)_/¯ [pnlfs_inode_by_name] unable to read directory %lu\n", dir->i_ino);
	dir_block = (struct pnlfs_dir_block *) bh->b_data;

	//trouver le inode correspond au nom
	for (i = 0; i < dir_info->nr_entries; i++){
		if (strcmp(dir_block->files[i].filename, name) == 0) {
			ino = le32_to_cpu(dir_block->files[i].inode);
			break;
		}
	}
	brelse(bh);
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
	If you wish to overload the dentry methods then you shoulddir
	initialise the "d_dop" field in the dentry; this is a pointer
	to a struct "dentry_operations".
	This method is called with the directory inode semaphore held

	A dentry is an object with a string name (d_name), 
	a pointer to an inode (d_inode), 
	and a pointer to the parent dentry (d_parent).
*/
static struct dentry *pnlfs_lookup(struct inode *dir, struct dentry *dentry, 
	unsigned int flags)
{
	ino_t ino;
	struct inode *ip = NULL;
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) [pnlfs_lookup]\n"); 
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

static struct inode *pnlfs_get_free_inode(struct inode *dir)
{
	struct inode *fresh_inode;
	struct pnlfs_sb_info *pnlsb_info;
	unsigned long *i_bitmap;
	int i, pos;	
	
	//look in ifree_bitmap to find a free inode
	pnlsb_info = dir->i_sb->s_fs_info;	
	i_bitmap = pnlsb_info->ifree_bitmap;
	pos = 0;	
	for (i = 0; i < 32768; i++){		
		if(i_bitmap[0] & (1 << i)){
			//change the bit in this possition to "0"	
			i_bitmap[0] & ~(1 << i);
			set_bit(i_bitmap, pos);
			break;
		}
		pos++;
	}
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) [pnlfs_get_free_inode] pos= %u\n", pos);
 	fresh_inode = pnlfs_iget(dir->i_sb, pos);
	return fresh_inode;
}

static void addin_directory(struct inode *dir, struct dentry *dentry, 
	struct inode *fresh_inode, umode_t mode)
{
	int i;
	struct buffer_head *bh;
	struct pnlfs_dir_block *dir_block;
	struct pnlfs_inode_info *dir_info;
	//lire directory meta-info
	dir_info = PNLFS_I(dir); 
	bh = sb_bread(dir->i_sb, dir_info->index_block);
	if (!bh) 
		pr_info(KERN_INFO "¯\\_(ツ)_/¯ [addin_directory] unable to read directory %lu\n", dir->i_ino);
	dir_block = (struct pnlfs_dir_block *) bh->b_data;
	//+1 nombre de files/directories dans le repertoire 	
	dir_info->nr_entries++;
	//ajouter le nom
	strcpy(dir_block->files[dir_info->nr_entries - 1].filename, dentry->d_name.name);
	//ajouter l'inode
	dir_block->files[dir_info->nr_entries - 1].inode = cpu_to_le32(fresh_inode->i_ino);
}

//create a regular file
static int pnlfs_create(struct inode *dir, struct dentry *dentry, 
	umode_t mode, bool excl)
{
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) [pnlfs_create]\n");
	struct inode *fresh_inode;
	//find a inode available by looking at "ifree_bitmap"
	fresh_inode = pnlfs_get_free_inode(dir);
	//add this new fresh inode and his "name" inside inode "dir"
	addin_directory(dir, dentry, fresh_inode, mode);
	return 0;
}

static int pnlfs_unlink(struct inode *dir, struct dentry *dentry)
{
	int ret;
	ino_t ino;
	struct inode *ip = NULL;
	//get inode base on name
	ino = pnlfs_inode_by_name(dir, dentry);
	if (ino) {
		ip = pnlfs_iget(dir->i_sb, ino);
		if (IS_ERR(ip))
			return ERR_CAST(ip);
	}
	if (!ino){
		pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) [pnlfs_unlink] File not found!\n");
		return 0;
	}
	//release this inode
	
	//remove from directory

	//change the bitmap "0" to "1"

	return ret;
}



static const struct inode_operations pnlfs_inops = {
	.lookup = pnlfs_lookup,
	.create = pnlfs_create,
	.unlink = pnlfs_unlink,
	//.mkdir  = pnlfs_mkdir,
	//.rmdir  = pnlfs_rmdir,
 	//.rename = pnlfs_rename,
};

static const struct file_operations pnlfs_operations = {
	.iterate_shared	= pnlfs_readdir,
};

static struct inode *pnlfs_iget(struct super_block *sb, unsigned long ino)
{
	int block_no;
	struct inode *inode;
	struct pnlfs_inode_info *pnli;
	struct pnlfs_inode *raw_inode;
	struct buffer_head * bh;
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) [pnlfs_iget] inode %lu\n", ino);
	
	//iget_locked appellera le constructeur pnlfs_alloc_inode
	inode = iget_locked(sb, ino); 
	if (!inode)
		return ERR_PTR(-ENOMEM);

	//If the inode is in cache
	if (!(inode->i_state & I_NEW)) 
		return inode;

	//If the inode is not in cache
	pnli = PNLFS_I(inode); 
	
	//trouver le bloque correspondant l'inode 
	//example: inode 35 est dans bloque 2 = 1 + 35/30 
	block_no = 1 + ino / PNLFS_INODES_PER_BLOCK;
	
	//Lire cette inode sur le disque
	bh = sb_bread(sb, block_no); 
	if (!bh) {
		pr_info(KERN_INFO "¯\\_(ツ)_/¯ [pnlfs_iget] unable to read inode %lu\n", inode->i_ino);
		goto bad_inode;
	}
	raw_inode = (struct pnlfs_inode *) bh->b_data;
	
	//Initialiser les champs
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

	//sauvegarder les metadonnees pour directory
	if (inode->i_mode = S_IFDIR){
		pnli->index_block = le32_to_cpu(raw_inode->index_block);  
		pnli->nr_entries  = le32_to_cpu(raw_inode->nr_entries);
	}

	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) [pnlfs_iget] Inode:\n"
		"\tinode->i_mode=%u\n"	
		"\tinode->i_ino=%lu\n"
		"\tinode->i_size=%lld\n"
		"\tinode->i_blocks=%lu\n"
		"\tpnli->index_block=%u\n"
		"\tpnli->nr_entries=%u\n",
		inode->i_mode, 
		inode->i_ino, 
		inode->i_size,
		inode->i_blocks,
		pnli->index_block,
		pnli->nr_entries
	);

	unlock_new_inode(inode);
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) [pnlfs_iget] new inode ok!\n");
	brelse(bh);
	return inode;

bad_inode:
	iget_failed(inode);
	return ERR_PTR(-EIO);
}

static int pnlfs_fill_super(struct super_block *sb, void *d, int silent)
{
	struct inode *root;
	struct pnlfs_superblock *pnlsb;
	struct pnlfs_sb_info *pnlsb_info;
	struct buffer_head *bh;
	//unsigned long img_size;
	int ret = -EINVAL;
	static int i, j;
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
		pr_info(KERN_INFO "¯\\_(ツ)_/¯ [pnlfs_fill_super] unable to read superblock");
		ret = -EIO;
		goto error;
	}
	memcpy(pnlsb, bh->b_data, PNLFS_BLOCK_SIZE);
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) [pnlfs_fill_super] Superblock: (%ld)\n"
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

	//alocate the ifree_bitmap and bfree_bitmap storage 
	pnlsb_info->ifree_bitmap = kmalloc(PNLFS_BLOCK_SIZE / 8 * pnlsb_info->nr_ifree_blocks * sizeof(uint64_t), GFP_KERNEL);
	memset(pnlsb_info->ifree_bitmap, 0xff, PNLFS_BLOCK_SIZE);

	pnlsb_info->bfree_bitmap = kmalloc(PNLFS_BLOCK_SIZE / 8 * pnlsb_info->nr_bfree_blocks * sizeof(uint64_t), GFP_KERNEL);
	memset(pnlsb_info->bfree_bitmap, 0xff, PNLFS_BLOCK_SIZE);

	//we don't know how to copy ifree_bitmap from disk so we do this 
	pnlsb_info->ifree_bitmap[0] = cpu_to_le64(0xfffffffffffffffc);	

	//we don't know how to copy bfree_bitmap from disk so we do this 
	uint64_t mask, line;
	uint32_t nr_used = pnlsb_info->nr_istore_blocks +
		pnlsb_info->nr_ifree_blocks +
		pnlsb_info->nr_bfree_blocks + 4;	
	i = 0;
	while (nr_used) {
		line = 0xffffffffffffffff;
		for (mask = 0x1; mask != 0x0; mask <<= 1) {
			line &= ~mask;
			nr_used--;
			if (!nr_used)
				break;
		}
		pnlsb_info->bfree_bitmap[i] = cpu_to_le64(line);
		i++;
	}

/*
	//we have tried to do this to copy the bitmaps
	for (i = 0; i < pnlsb_info->nr_ifree_blocks; i++){
		if (!(bh = sb_bread(sb, pnlsb->nr_istore_blocks + 1 + i))) {
			pr_info(KERN_INFO "¯\\_(ツ)_/¯ unable to read superblock");
			ret = -EIO;
			goto error;
		}
		for(j = 0; j < 512 * pnlsb_info->nr_ifree_blocks ; j++)
			*(pnlsb_info->ifree_bitmap) << 64*j = le64_to_cpu(bh->b_data << 64*j);
	}
*/


	sb->s_fs_info = pnlsb_info;
	//Once done, bh should be released
	brelse(bh);
	//récupérer l’inode racine du disque (inode 0)	
	root = pnlfs_iget(sb, 0);
	if (IS_ERR(root)) {
		pr_info(KERN_INFO "¯\\_(ツ)_/¯ [pnlfs_fill_super] get root inode failed\n");
		return PTR_ERR(root);
	}
	//définir ses propriétaires
	inode_init_owner(root, NULL, root->i_mode);
	//puis la déclarer comme inode racine
	sb->s_root = d_make_root(root);
	if (!(sb->s_root)) {
		pr_info(KERN_INFO "¯\\_(ツ)_/¯ [pnlfs_fill_super] get root dentry failed\n");
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

static struct dentry *pnlfs_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) [pnlfs_mount]\n");
	return mount_bdev(fs_type, flags, dev_name, data, pnlfs_fill_super);
}

static void kill_pnlfs_super(struct super_block *s)
{
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) [kill_block_super]\n");
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

	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) [pnlfs_init] module loaded\n");
	return 0;

error_kmem_cache_create:
	pr_info(KERN_INFO "¯\\_(ツ)_/¯ [pnlfs_init] kmem_cache_create failed");
	unregister_filesystem(&pnlfs_type);
	return -ENOMEM;
error_register_filesystem:
	pr_info(KERN_INFO "¯\\_(ツ)_/¯ [pnlfs_init] register_filesystem failed");
	return -1;
}
module_init(pnlfs_init);

static void pnlfs_exit(void)
{
	unregister_filesystem(&pnlfs_type);
	pr_info(KERN_INFO "( ͡° ͜ʖ ͡°) [pnlfs_exit] module unloaded\n");
}
module_exit(pnlfs_exit);
