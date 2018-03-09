#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

MODULE_DESCRIPTION("Module \"hello word\" pour noyau linux");
MODULE_AUTHOR("Julien Sopena, LIP6");
MODULE_LICENSE("GPL");

char * whom = "default";
int howmany = 42;

MODULE_PARM_DESC(whom, "Name of the person");
MODULE_PARM_DESC(howmany, "How many times the name will be printed");

module_param(whom, charp, 0600);
module_param(howmany, int, 0600);

static int __init hello_init(void)
{

	/* pour peut être accédée par ce module */
	/*EXPORT_SYMBOL_GPL(init_uts_ns);*/

	int i;
	for(i =0; i<howmany; i++)
		pr_info(KERN_INFO "Hello, %s\n", whom);
	return 0;
}
module_init(hello_init);

static void __exit hello_exit(void)
{
	int i;
	for(i =0; i<howmany; i++)
		pr_info(KERN_INFO "Goodbye,  %s\n", whom);
}
module_exit(hello_exit);

