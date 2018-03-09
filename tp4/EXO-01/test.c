#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/io.h>
/* for the function to iterate and the structure */
#include <linux/fs.h>

MODULE_DESCRIPTION("Module \"show_sb\" pour noyau linux");
MODULE_AUTHOR("William Fabre, LIP6");
MODULE_LICENSE("GPL");

/* static char *kernel_name= "m_arch-alain";*/
/* module_param_named(my_name, kernel_name, charp, 0644);*/
/* MODULE_PARM_DESC(my_name, " Who are you?");*/

void my_print_block(struct super_block *s_b, void *unknow)
{
	pr_info(
	"uuid= %.2x%.2x%.2x%.2x %.2x%.2x %.2x%.2x %.2x%.2x %.2x%.2x %.2x%.2x%.2x%.2x type=%s\n",
			s_b->s_uuid[0],
			s_b->s_uuid[1],
			s_b->s_uuid[2],
			s_b->s_uuid[3],
			s_b->s_uuid[4],
			s_b->s_uuid[5],
			s_b->s_uuid[6],
			s_b->s_uuid[7],
			s_b->s_uuid[8],
			s_b->s_uuid[9],
			s_b->s_uuid[10],
			s_b->s_uuid[11],
			s_b->s_uuid[12],
			s_b->s_uuid[13],
			s_b->s_uuid[14],
			s_b->s_uuid[15],
			s_b->s_type->name);
	return;
}

static int __init hello_init(void)
{

	pr_info("begin of showing the whole superblock\n");
	iterate_supers(my_print_block, NULL);

	return 0;
}
module_init(hello_init);

static void __exit hello_exit(void)
{
	pr_info("end of showing the whole super block\n");
}
module_exit(hello_exit);

