/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2018 NXP
 */

#include <asm/arch/sci/sci.h>
#include <asm/mach-imx/sys_proto.h>
#include <asm/arch/power-domain.h>
#include <dm/platdata.h>
#include <dm/device-internal.h>
#include <dm/device.h>
#include <power-domain.h>
#include <linux/types.h>

struct pass_over_info_t {
	u16 barker;
	u16 len;
	u32 g_bt_cfg_shadow;
	u32 card_address_mode;
	u32 bad_block_count_met;
	u32 g_ap_mu;
};

#if defined(CONFIG_IMX8QM)
#define FUSE_IMG_SET_OFF_WORD 464
#elif defined(CONFIG_IMX8QXP) || defined (CONFIG_IMX8DXL)
#define FUSE_IMG_SET_OFF_WORD 720
#endif

extern unsigned long rom_pointer[];
void build_info(void);
enum boot_device get_boot_device(void);
int boot_mode_getprisec(void);
int boot_mode_is_closed(void);
int ahab_close(void);
void boot_mode_enable_secondary(bool enable);
int print_bootinfo(void);
int sc_pm_setup_uart(sc_rsrc_t uart_rsrc, sc_pm_clock_rate_t clk_rate);
int imx8_power_domain_lookup_name(const char *name,
				  struct power_domain *power_domain);
void imx8_power_off_pd_devices(const char *permanent_on_devices[], int size);
bool m4_parts_booted(void);
