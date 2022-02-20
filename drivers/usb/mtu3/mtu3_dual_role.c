/*
 * Copyright (C) 2018 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb/class-dual-role.h>
#include "mtu3.h"

static enum dual_role_property mtu3_dual_role_props[] = {
	DUAL_ROLE_PROP_MODE,
	DUAL_ROLE_PROP_PR,
	DUAL_ROLE_PROP_DR,
	/* DUAL_ROLE_PROP_VCONN_SUPPLY, */
};

static int mtu3_dual_role_get_prop(struct dual_role_phy_instance *dual_role,
			enum dual_role_property prop, unsigned int *val)
{
	struct ssusb_mtk *ssusb = dev_get_drvdata(dual_role->dev.parent);
	struct mtu3 *mtu3 = ssusb->u3d;
	int result = 0;
	int mode, pr, dr;

	dev_dbg(mtu3->dev,
		"prop:%d, pull: D%s, active:%d, is_host:%s dual_role_mode(%i):%s\n",
			prop, mtu3->softconnect ? "+" : "-", mtu3->is_active,
			ssusb->is_host ? "Host" : "Device", ssusb->dual_role_mode,
			 (ssusb->dual_role_mode == MTU3_DR_FORCE_HOST)   ? "HOST"
			:(ssusb->dual_role_mode == MTU3_DR_FORCE_DEVICE) ? "DEVICE"
			: "NONE" );

	if (mtu3->is_active) {
		if (ssusb->is_host) {
			mode = DUAL_ROLE_PROP_MODE_DFP;
			pr   = DUAL_ROLE_PROP_PR_SRC;
			dr   = DUAL_ROLE_PROP_DR_HOST;
		} else {
			mode = DUAL_ROLE_PROP_MODE_UFP;
			pr   = DUAL_ROLE_PROP_PR_SNK;
			dr   = DUAL_ROLE_PROP_DR_DEVICE;
		}
	} else {
		mode = DUAL_ROLE_PROP_MODE_NONE;
		pr   = DUAL_ROLE_PROP_PR_NONE;
		dr   = DUAL_ROLE_PROP_DR_NONE;
	}

	switch (prop) {
	case DUAL_ROLE_PROP_MODE:
		*val = mode;
		break;
	case DUAL_ROLE_PROP_PR:
		*val = pr;
		break;
	case DUAL_ROLE_PROP_DR:
		*val = dr;
		break;
	case DUAL_ROLE_PROP_VCONN_SUPPLY:
		*val = DUAL_ROLE_PROP_VCONN_SUPPLY_NO;
		break;
	default:
		result = -EINVAL;
		dev_warn(mtu3->dev, "Invalid argument prop:(0~4): %i\n", prop);
		break;
	}

	return result;
}

static int mtu3_dual_role_prop_is_writeable(
	struct dual_role_phy_instance *dual_role, enum dual_role_property prop)
{
	/* not support writeable */
	return -EACCES;
}

static int mtu3_dual_role_set_prop(struct dual_role_phy_instance *dual_role,
			enum dual_role_property prop, const unsigned int *val)
{
	/* do nothing */
	return -EPERM;
}

static int mtu3_usb_dual_role_changed(struct mtu3 *mtu3)
{
	struct ssusb_mtk *ssusb = mtu3->ssusb;
	int result = -ENODEV;

	if (ssusb->dr_usb) {
		result = 0;
		dual_role_instance_changed(ssusb->dr_usb);
	}

	return result;
}
int mtu3_dual_role_set_none(struct mtu3 *mtu3)
{
	struct ssusb_mtk *ssusb = mtu3->ssusb;
	int result = 0;

	if (!ssusb)
		return -ENODEV;


	ssusb->dual_role_mode = MTU3_DR_FORCE_NONE;
	result = mtu3_usb_dual_role_changed(mtu3);
	dev_dbg(mtu3->dev, "MTU3_DR_FORCE_NONE uevent is sent\n");

	return result;
}
int mtu3_dual_role_set_host(struct mtu3 *mtu3)
{
	struct ssusb_mtk *ssusb = mtu3->ssusb;
	int result = 0;

	if (!ssusb)
		return -ENODEV;

	if (ssusb->dual_role_mode != MTU3_DR_FORCE_HOST) {
		ssusb->dual_role_mode = MTU3_DR_FORCE_HOST;
		result = mtu3_usb_dual_role_changed(mtu3);
		dev_dbg(mtu3->dev, "MTU3_DR_FORCE_HOST uevent is sent\n");
	} else
		dev_warn(mtu3->dev, "Warning: It was MTU3_DR_FORCE_HOST\n");

	return result;
}
int mtu3_dual_role_set_device(struct mtu3 *mtu3)
{
	struct ssusb_mtk *ssusb = mtu3->ssusb;
	int result = 0;

	if (!ssusb)
		return -ENODEV;

	if (ssusb->dual_role_mode != MTU3_DR_FORCE_DEVICE) {
		ssusb->dual_role_mode = MTU3_DR_FORCE_DEVICE;
		result = mtu3_usb_dual_role_changed(mtu3);
		dev_dbg(mtu3->dev, "MTU3_DR_FORCE_DEVICE uevent is sent\n");
	} else
		dev_warn(mtu3->dev, "Warning: It was MTU3_DR_FORCE_DEVICE\n");

	return result;
}

int mtu3_usb_dual_role_init(struct mtu3 *mtu3)
{
	int result = 0;
	struct dual_role_phy_desc *dual_desc;
	struct ssusb_mtk *ssusb = mtu3->ssusb;

	dual_desc = devm_kzalloc(mtu3->dev, sizeof(*dual_desc), GFP_KERNEL);

	if (!dual_desc){
		dev_err(mtu3->dev, "%s fail to devm_kzalloc(...)\n", __func__);
		result = -ENOMEM;
	} else {
		dual_desc->name            = "dual-role-usb20";
		dual_desc->supported_modes = DUAL_ROLE_SUPPORTED_MODES_DFP_AND_UFP;
		dual_desc->properties      = mtu3_dual_role_props;
		dual_desc->num_properties  = ARRAY_SIZE(mtu3_dual_role_props);
		dual_desc->get_property    = mtu3_dual_role_get_prop;
		dual_desc->set_property    = mtu3_dual_role_set_prop;
		dual_desc->property_is_writeable = mtu3_dual_role_prop_is_writeable;

		ssusb->dr_usb = devm_dual_role_instance_register(mtu3->dev, dual_desc);
		if (IS_ERR(ssusb->dr_usb)) {
			dev_err(ssusb->dev, "fail to register dual role usb\n");
			result = -EINVAL;
		}
	}

	return result;
}
