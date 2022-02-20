/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_MFG_COUNTER__
#define __MTK_MFG_COUNTER__

#include <linux/seq_file.h>

extern int mfg_counter_init(void);
extern void mfg_counter_deinit(void);

void mfg_counter_start_trace(void);
void mfg_counter_stop_trace(void);

void mfg_counter_show_trace(struct seq_file *m);
int mfg_counter_set_hist_size(int period, int len);

void mfg_counter_power_on(void);
void mfg_counter_power_off(void);
#endif
