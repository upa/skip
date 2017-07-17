
#include <linux/kernel.h>
#include <linux/module.h>

#include "skip.h"

static int __init skip_init(void)
{
	int ret;

	ret = skip_lwt_init();
	if (ret)
		return ret;
	
	pr_info("skip version (%s) is loaded\n", SKIP_VERSION);

	return 0;
}

static void __exit skip_exit(void)
{
	skip_lwt_exit();

	pr_info("skip version (%s) is unloaded\n", SKIP_VERSION);
}

module_init(skip_init);
module_exit(skip_exit);
MODULE_AUTHOR("Ryo Nakamura <upa@haeena.net>");
MODULE_LICENSE("GPL");
MODULE_VERSION(SKIP_VERSION);
