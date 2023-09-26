// SPDX-License-Identifier: GPL-2.0+

#include <common.h>
#include <dm.h>
#include <init.h>
#include <sysinfo.h>
#include <asm/global_data.h>
#include <linux/libfdt.h>
#include <linux/compiler.h>

#define COMPATIBLE "lmp,bootloader"

int __weak get_boot_firmware_info(void)
{
	char *version;
	int node;
	int ret = 0;

	node = fdt_node_offset_by_compatible(gd->fdt_blob, -1, COMPATIBLE);
	if (node < 0) {
		printf("Can't find node with compatible = \"" COMPATIBLE
		       "\"\n");
		ret = -FDT_ERR_NOTFOUND;
		goto out;
	}

	version = fdt_getprop(gd->fdt_blob, node, "bootfirmware-version", NULL);
	if (version) {
		printf("Boot firmware version: %s\n", version);
#if CONFIG_IS_ENABLED(ENV_SUPPORT)
		env_set("dt_bootfirmware_version", version);
#endif
	} else {
		ret = -FDT_ERR_NOTFOUND;
	}

out:
	/* we should stop booting, if boot firmware version is not found */
	if (CONFIG_IS_ENABLED(BOOTFIRMWARE_INFO_STRICT) && ret != 0) {
		printf("Fail to read boot firmware info from DTB, "
		       "abort boot as BOOTFIRMWARE_INFO_STRICT is set\n");
		return ret;
	}

	return 0;
}
