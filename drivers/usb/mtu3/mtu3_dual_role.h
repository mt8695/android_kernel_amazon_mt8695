/*
 * mtu3_dr.h - dual role switch and host glue layer header
 *
 * Copyright (C) 2016 MediaTek Inc.
 *
 * Author: Chunfeng Yun <chunfeng.yun@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MTU3_DUAL_ROLE_H_
#define _MTU3_DUAL_ROLE_H_
#include <linux/usb/class-dual-role.h>
#include "mtu3.h"

#if IS_ENABLED(CONFIG_USB_MTU3_DUAL_ROLE)
#if IS_ENABLED(CONFIG_DUAL_ROLE_USB_INTF)
int mtu3_usb_dual_role_init(struct mtu3 *mtu3);
int mtu3_dual_role_set_none(struct mtu3 *mtu3);
int mtu3_dual_role_set_host(struct mtu3 *mtu3);
int mtu3_dual_role_set_device(struct mtu3 *mtu3);
#else
static inline int mtu3_usb_dual_role_init(struct mtu3 *mtu3)
{
	return 0;
}
static inline int mtu3_dual_role_set_none(struct mtu3 *mtu3)
{
	return 0;
}
static inline int mtu3_dual_role_set_host(struct mtu3 *mtu3)
{
	return 0;
}
static inline int mtu3_dual_role_set_device(struct mtu3 *mtu3)
{
	return 0;
}
#endif
#endif

#endif	/* _MTU3_DUAL_ROLE_H_ */
