/*
 * Copyright (c) 2015 MediaTek Inc.
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
 */
#ifndef __MTK_USB_PHY_H__
#define __MTK_USB_PHY_H__
#include <linux/phy/phy.h>

#if IS_ENABLED(CONFIG_PHY_MTK_TPHY)
void mtk_phy_patch_pp(struct phy *phy, int port);
#else
static inline void mtk_phy_patch_pp(struct phy *phy, int port)
{
}
#endif
#endif	/* __MTK_USB_PHY_H__ */
