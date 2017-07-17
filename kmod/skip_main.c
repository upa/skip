
#include <linux/kernel.h>
#include <linux/module.h>

#include "skip.h"

static int __init skip_init(void)
{
	int ret;

	ret = skip_lwt_init();
	if (ret)
		return ret;
	
	ret = af_skip_init();
	if (ret) {
		pr_err("failed to init AF_SKIP '%d'\n", ret);
		goto af_skip_failed;
	}

	pr_info("skip version (%s) is loaded\n", SKIP_VERSION);

	return 0;

af_skip_failed:
	skip_lwt_exit();
	return ret;
}

static void __exit skip_exit(void)
{
	skip_lwt_exit();
	af_skip_exit();
	pr_info("skip version (%s) is unloaded\n", SKIP_VERSION);
}

module_init(skip_init);
module_exit(skip_exit);
MODULE_AUTHOR("Ryo Nakamura <upa@haeena.net>");
MODULE_LICENSE("GPL");
MODULE_VERSION(SKIP_VERSION);
