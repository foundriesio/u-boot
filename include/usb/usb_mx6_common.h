// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2019 NXP
 *
 */

#ifndef __USB_MX6_COMMON_H__
#define __USB_MX6_COMMON_H__
#include <usb/ehci-ci.h>

#if defined(CONFIG_MX6) || defined(CONFIG_MX7ULP) || defined(CONFIG_IMXRT) || defined(CONFIG_IMX8) || defined(CONFIG_IMX8ULP)
static const ulong phy_bases[] = {
	USB_PHY0_BASE_ADDR,
#if defined(USB_PHY1_BASE_ADDR)
	USB_PHY1_BASE_ADDR,
#endif
};
#endif

struct ehci_mx6_phy_data {
	void __iomem *phy_addr;
	void __iomem *misc_addr;
	void __iomem *anatop_addr;
};

void ehci_mx6_phy_init(struct usb_ehci *ehci, struct ehci_mx6_phy_data *phy_data, int index);
#endif /* __USB_MX6_COMMON_H__ */
