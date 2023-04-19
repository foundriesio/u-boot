/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2022 NXP
 */

#ifndef __ARCH_IMX9_SYS_PROTO_H
#define __ARCH_NMX9_SYS_PROTO_H

#include <asm/mach-imx/sys_proto.h>

void soc_power_init(void);
bool m33_is_rom_kicked(void);
int m33_prepare(void);
bool is_usb_boot(void);
int boot_mode_getprisec(void);
int boot_mode_is_closed(void);
void boot_mode_enable_secondary(bool enable);
#endif
