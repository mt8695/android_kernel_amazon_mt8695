/*
 * Copyright (c) 2016-2017 MediaTek Inc.
 * Author: Yong Wu <yong.wu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef MTK_IMGRESZ_PRIV_H
#define MTK_IMGRESZ_PRIV_H

#define DRV_VERIFY_SUPPORT	1

#include "imgresz.h"

#define loginfo(lvl, string, args...) do {\
		if (imgresz_log_level & lvl)\
			pr_info("[ImgResz]"string, ##args);\
		} while (0)
#define logwarn(string, args...) pr_info("[ImgResz]"string, ##args)

enum IMGRESZ_CHIP_VER {
	IMGRESZ_CURR_CHIP_VER_BASE = 0,
	IMGRESZ_CURR_CHIP_VER_8697,
	IMGRESZ_CURR_CHIP_VER_8695,
	IMGRESZ_CURR_CHIP_VER_MAX
};

enum IMGRESZ_RESAMPLE_METHOD_T {
	IMGRESZ_RESAMPLE_METHOD_M_TAP,/* multi-tap */
	IMGRESZ_RESAMPLE_METHOD_4_TAP,
	IMGRESZ_RESAMPLE_METHOD_8_TAP
};

enum IMGRESZ_BUF_MAIN_FORMAT_T {
	IMGRESZ_BUF_MAIN_FORMAT_Y_C,
	IMGRESZ_BUF_MAIN_FORMAT_Y_CB_CR,
	IMGRESZ_BUF_MAIN_FORMAT_INDEX,
	IMGRESZ_BUF_MAIN_FORMAT_ARGB,
	IMGRESZ_BUF_MAIN_FORMAT_AYUV,
};

enum IMGRESZ_YUV_FORMAT_T {
	IMGRESZ_YUV_FORMAT_420,
	IMGRESZ_YUV_FORMAT_422,
	IMGRESZ_YUV_FORMAT_444
};

enum IMGRESZ_ARGB_FORMAT_T {
	IMGRESZ_ARGB_FORMAT_0565,
	IMGRESZ_ARGB_FORMAT_1555,
	IMGRESZ_ARGB_FORMAT_4444,
	IMGRESZ_ARGB_FORMAT_8888,
};

struct imgresz_buf_format {
	enum IMGRESZ_BUF_MAIN_FORMAT_T mainformat;
	enum IMGRESZ_YUV_FORMAT_T yuv_format;
	enum IMGRESZ_ARGB_FORMAT_T argb_format;

	unsigned int h_sample[3];
	unsigned int v_sample[3];
	bool block;
	bool progressive;
	bool top_field;
	bool bit10;
	bool jump_10bit;
};

struct imgresz_hal_info {
	unsigned int h8_factor_y;
	unsigned int h8_offset_y;
	unsigned int h8_factor_cb;
	unsigned int h8_offset_cb;
	unsigned int h8_factor_cr;
	unsigned int h8_offset_cr;
	unsigned int hsa_factor_y;
	unsigned int hsa_offset_y;
	unsigned int hsa_factor_cb;
	unsigned int hsa_offset_cb;
	unsigned int hsa_factor_cr;
	unsigned int hsa_offset_cr;
	unsigned int v4_factor_y;
	unsigned int v4_factor_cb;
	unsigned int v4_factor_cr;
	unsigned int v4_offset_y;
	unsigned int v4_offset_cb;
	unsigned int v4_offset_cr;
	unsigned int vm_factor_y;
	unsigned int vm_factor_cb;
	unsigned int vm_factor_cr;
	unsigned int vm_offset_y;
	unsigned int vm_offset_cb;
	unsigned int vm_offset_cr;
	bool vm_scale_up_y;
	bool vm_scale_up_cb;
	bool vm_scale_up_cr;
};

struct imgresz_partition_info {
	unsigned int src_x_offset;
	unsigned int src_x_offset_c;
	unsigned int src_w;
	unsigned int src_w_y;
	unsigned int src_w_c;
	unsigned int src_w_cr;
	unsigned int src_h;
	unsigned int src_h_y;
	unsigned int src_h_c;
	unsigned int src_h_cr;
	unsigned int dst_x_offset;
	unsigned int dst_w;
};

struct imgresz_scale_data {
	enum imgresz_ticket_fun_type	funtype;
	enum imgresz_scale_mode		scale_mode;

	enum IMGRESZ_RESAMPLE_METHOD_T	h_method;
	enum IMGRESZ_RESAMPLE_METHOD_T	v_method;
	struct imgresz_src_buf_info	src_buf;
	struct imgresz_dst_buf_info	dst_buf;
	struct imgresz_buf_format	src_format;
	struct imgresz_buf_format	dst_format;
	struct imgresz_rm_info		rm_info;
	struct imgresz_jpg_info		jpg_info;
	struct imgresz_partial_buf_info partial_info;
	struct imgresz_hal_info     hal_info;
	struct imgresz_partition_info  partition;

	bool				ufo_v_partition;
	bool				ufo_h_partition;
	bool				ufo_page0;
	bool				one_phase;
	bool				outstanding;
};

#endif
