#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <linux/timekeeping.h>

MODULE_DESCRIPTION("Module \"show_sb\" pour noyau linux");
MODULE_AUTHOR("Thanh TA - JAN Klos, LIP6");
MODULE_LICENSE("GPL");

static char *type = "ext4";
module_param(type, charp, 0644);
MODULE_PARM_DESC(type, "search for this type in SB");

static void print_super(struct super_block *super, void *nothing)
{
	pr_info("uuid=%.2x%.2x%.2x%.2x %.2x%.2x %.2x%.2x %.2x%.2x %.2x%.2x %.2x%.2x%.2x%.2x type=%s date=%ld.%ld\n",
		super->s_uuid[0],
		super->s_uuid[1],
		super->s_uuid[2],
		super->s_uuid[3],
		super->s_uuid[4],
		super->s_uuid[5],
		super->s_uuid[6],
		super->s_uuid[7],
		super->s_uuid[8],
		super->s_uuid[9],
		super->s_uuid[10],
		super->s_uuid[11],
		super->s_uuid[12],
		super->s_uuid[13],
		super->s_uuid[14],
		super->s_uuid[15],
		super->s_type->name,
		super->s_print_date);
	return;
}


static int __init update_sb_init(void)
{
	iterate_supers_type(get_fs_type(type), &print_super, NULL);
	return 0;
}
module_init(update_sb_init);

static void __exit update_sb_exit(void)
{
	pr_info(KERN_INFO "Goodbye, update_sb module\n");
}
module_exit(update_sb_exit);

