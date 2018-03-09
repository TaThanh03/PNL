#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/io.h>

#include <linux/utsname.h>

MODULE_DESCRIPTION("Module \"change uname\" pour noyau linux");
MODULE_AUTHOR("Thanh TA - JAN Klos, LIP6");
MODULE_LICENSE("GPL");


static char * new_name = "default_name";
module_param(new_name, charp, 0644);
MODULE_PARM_DESC(new_name, "new uname");

static char save[__NEW_UTS_LEN + 1];

extern struct uts_namespace init_uts_ns;

static int __init change_uname_init(void)
{
	strcpy(save, init_uts_ns.name.sysname);
	strcpy(init_uts_ns.name.sysname, new_name);
	pr_info(KERN_INFO "name changed:%s\n",init_uts_ns.name.sysname);
	return 0;
}
module_init(change_uname_init);

static void __exit change_uname_exit(void)
{
	strcpy(init_uts_ns.name.sysname, save);
	pr_info(KERN_INFO "Goodbye,  change_uname module\n");
}
module_exit(change_uname_exit);
