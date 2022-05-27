// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 Foundries.io Ltd
 */

#include <common.h>
#include <command.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <asm/io.h>
#include <spi.h>
#include <spi_flash.h>

#define SYS_CHECKSUM_OFFSET		(0x3U)
#define PERSISTENT_REG_SIZE		(0x20000U)
#define ENV_VAR_PREFIX			"zynqmp."

/* The below enums denote persistent registers in Qspi Flash */
struct sys_persistent_state {
	char last_booted_img;
	char requested_boot_img;
	char img_b_bootable;
	char img_a_bootable;
};

struct sys_boot_img_info {
	char idstr[4U];
	unsigned int ver;
	unsigned int len;
	unsigned int checksum;
	struct sys_persistent_state persistent_state;
	unsigned int boot_img_a_offset;
	unsigned int boot_img_b_offset;
	unsigned int recovery_img_offset;
} __packed;

enum sys_boot_img_id {
	SYS_BOOT_IMG_A_ID = 0,
	SYS_BOOT_IMG_B_ID = 1,
};

static struct sys_boot_img_info boot_img_info __attribute__ ((aligned(4U)));

static unsigned int calculate_checksum(void)
{
	unsigned int idx;
	unsigned int checksum = 0U;
	unsigned int *data = (unsigned int *)&boot_img_info;
	unsigned int boot_img_info_size = sizeof(boot_img_info) / 4U;

	for (idx = 0U; idx < SYS_CHECKSUM_OFFSET; idx++)
		checksum += data[idx];

	for (idx = SYS_CHECKSUM_OFFSET + 1U; idx < boot_img_info_size; idx++)
		checksum += data[idx];

	return (0xFFFFFFFFU - checksum);
}

static int validate_boot_img_info(void)
{
	int ret = -1;
	unsigned int checksum = boot_img_info.checksum;

	if ((boot_img_info.idstr[0U] == 'A') &&
	    (boot_img_info.idstr[1U] == 'B') &&
		(boot_img_info.idstr[2U] == 'U') &&
		(boot_img_info.idstr[3U] == 'M')) {
		boot_img_info.checksum = calculate_checksum();
		if (checksum == boot_img_info.checksum)
			ret = 0;
	}

	return ret;
}

static int init_spi_flash(struct spi_flash **flash)
{
	unsigned int bus = CONFIG_SF_DEFAULT_BUS;
	unsigned int cs = CONFIG_SF_DEFAULT_CS;
	/* In DM mode, defaults speed and mode will be taken from DT */
	unsigned int speed = CONFIG_SF_DEFAULT_SPEED;
	unsigned int mode = CONFIG_SF_DEFAULT_MODE;

	int ret;
#if CONFIG_IS_ENABLED(DM_SPI_FLASH)
	struct udevice *new, *bus_dev;
#else
	struct spi_flash *new;
#endif

#if CONFIG_IS_ENABLED(DM_SPI_FLASH)
	/* Remove the old device, otherwise probe will just be a nop */
	ret = spi_find_bus_and_cs(bus, cs, &bus_dev, &new);
	if (!ret)
		device_remove(new, DM_REMOVE_NORMAL);

	*flash = NULL;
	ret = spi_flash_probe_bus_cs(bus, cs, speed, mode, &new);
	if (ret) {
		printf("Failed to initialize SPI flash at %u:%u (error %d)\n",
		       bus, cs, ret);
		return ret;
	}

	*flash = dev_get_uclass_priv(new);
#else
	if (*flash)
		spi_flash_free(*flash);

	new = spi_flash_probe(bus, cs, speed, mode);
	*flash = new;
	if (!new) {
		printf("Failed to initialize SPI flash at %u:%u\n", bus, cs);
		return ret;
	}
#endif
	return ret;
}

static int read_persistent_info(struct spi_flash *flash)
{
	int ret;

	ret = spi_flash_read(flash, CONFIG_ZYNQMP_IMGSEL_PERSISTENT_REGISTER,
			     sizeof(boot_img_info), &boot_img_info);
	if (ret) {
		printf("Check read failed (error = %d)\n", ret);
		return ret;
	}

	ret = validate_boot_img_info();
	if (ret) {
		printf("Persistent registers are corrupted\n");
		return ret;
	}

	return ret;
}

static void print_persistent_info(void)
{
	printf("Image A: ");
	if (boot_img_info.persistent_state.img_a_bootable == 0U) {
		printf("Non Bootable\n");
		env_set(ENV_VAR_PREFIX "image_a_bootable", "0");
	} else {
		printf("Bootable\n");
		env_set(ENV_VAR_PREFIX "image_a_bootable", "1");
	}

	printf("Image B: ");
	if (boot_img_info.persistent_state.img_b_bootable == 0U) {
		printf("Non Bootable\n");
		env_set(ENV_VAR_PREFIX "image_b_bootable", "0");
	} else {
		printf("Bootable\n");
		env_set(ENV_VAR_PREFIX "image_b_bootable", "1");
	}

	printf("Requested Boot Image: ");
	if (boot_img_info.persistent_state.requested_boot_img ==
		(char)SYS_BOOT_IMG_A_ID) {
		printf("Image A\n");
		env_set(ENV_VAR_PREFIX "requested_boot", "0");
	} else {
		printf("Image B\n");
		env_set(ENV_VAR_PREFIX "requested_boot", "1");
	}

	printf("Last Booted Image: ");
	if (boot_img_info.persistent_state.last_booted_img ==
		(char)SYS_BOOT_IMG_A_ID) {
		printf("Image A\n");
		env_set(ENV_VAR_PREFIX "last_booted", "0");
	} else {
		printf("Image B\n");
		env_set(ENV_VAR_PREFIX "last_booted", "1");
	}
}

static int update_persistent_info(struct spi_flash *flash, u32 slot_a,
				  u32 slot_b, u32 requested_slot)
{
	int ret;

	boot_img_info.persistent_state.img_a_bootable = slot_a;
	boot_img_info.persistent_state.img_b_bootable = slot_b;
	boot_img_info.persistent_state.requested_boot_img = requested_slot;
	boot_img_info.checksum = calculate_checksum();

	ret = spi_flash_erase(flash, CONFIG_ZYNQMP_IMGSEL_PERSISTENT_REGISTER,
			      flash->sector_size);
	if (ret) {
		printf("Erase operation to SPI flash failed\n");
		return ret;
	}

	ret = spi_flash_write(flash, CONFIG_ZYNQMP_IMGSEL_PERSISTENT_REGISTER,
			      sizeof(boot_img_info), &boot_img_info);
	if (ret)
		printf("Write operation to SPI flash failed\n");

	return ret;
}

static int do_bootslot(struct cmd_tbl *cmdtp, int flag,
		       int argc, char * const argv[])
{
	u32 slot_a, slot_b, requested_slot;
	struct spi_flash *flash;
	int ret;

	if (argc != 4 && argc != 1)
		return CMD_RET_USAGE;

	ret = init_spi_flash(&flash);
	if (ret) {
		printf("Can't init spi flash\n");
		return CMD_RET_FAILURE;
	}

	ret = read_persistent_info(flash);
	if (ret) {
		printf("Can't read spi flash\n");
		ret = CMD_RET_FAILURE;
		goto free_spi;
	}

	/* If we just want to retrieve current persistent data info */
	if (argc == 1) {
		print_persistent_info();
		ret = CMD_RET_SUCCESS;
		goto free_spi;
	}

	slot_a = simple_strtoul(argv[1], NULL, 10);
	slot_b = simple_strtoul(argv[2], NULL, 10);
	requested_slot = simple_strtoul(argv[3], NULL, 10);

	if ((slot_a != 0 && slot_a != 1) ||
	    (slot_b != 0 && slot_b != 1) ||
	    (requested_slot != 0 && requested_slot != 1)) {
		ret = CMD_RET_USAGE;
		goto free_spi;
	}

	ret = update_persistent_info(flash, slot_a, slot_b, requested_slot);
	if (ret) {
		printf("Update of persistent registers failed\n");
		ret = CMD_RET_FAILURE;
		goto free_spi;
	}

	print_persistent_info();

free_spi:
	spi_flash_free(flash);

	return ret;
}

U_BOOT_CMD(
	bootslot, CONFIG_SYS_MAXARGS, 1, do_bootslot,
	"Get/Set current boot slot on QSPI media",
	"[no param]                   - only print persistent register values\n"
	"bootslot slot_a slot_b requested_slot - update persistent register with new values:\n"
	"                                        \"slot_a\" is bootable, supported values [0,1]\n"
	"                                        \"slot_b\" is bootable, supported values [0,1]\n"
	"                                        \"requested_slot\", supported values [0,1]\n"
	);
