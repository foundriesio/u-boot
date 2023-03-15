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

	node = fdt_node_offset_by_compatible(gd->fdt_blob, -1, COMPATIBLE);
	if (node < 0) {
		printf("Can't find node with compatible = \"" COMPATIBLE
		       "\"\n");
		return -FDT_ERR_NOTFOUND;
	}

	version = fdt_getprop(gd->fdt_blob, node, "bootfirmware-version", NULL);
	if (version) {
		printf("Boot firmware version: %s\n", version);
		env_set("dt_bootfirmware_version", version);
	} else {
		return -FDT_ERR_NOTFOUND;
	}

	return 0;
}
