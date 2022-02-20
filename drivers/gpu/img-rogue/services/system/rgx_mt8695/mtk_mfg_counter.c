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

#include <linux/spinlock.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include "servicesext.h"
#include "rgxdevice.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <asm-generic/io.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/trace_events.h>
#include <linux/fs.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/hrtimer.h>

#include "mtk_mfg.h"
#include "mtk_mfg_counter.h"
#include "mtk_gpu_misc.h"

#define MFG_COUNTER_DEFAULT_PERIOD 1000 /* us */
#define MFG_COUNTER_DEFAULT_HIST_SIZE 4000

enum  {
	MFG_COUNTER_STATE_NOT_READY,
	MFG_COUNTER_STATE_NO_DATA,
	MFG_COUNTER_STATE_TRACING,
	MFG_COUNTER_STATE_FINISHED,
};
enum {
	MFG_COUNTER_CONTINUE,
	MFG_COUNTER_VALUE,
};

/*
 * TBD:We may need another spinlock for some query from
 * non-sleep context..
 */
/* static DEFINE_SPINLOCK(counter_lock); */
static DEFINE_MUTEX(trace_lock);

static void __iomem *rgx_base;

#define MFG_RGX_WRITE32(addr, value)	  writel(value, rgx_base + (addr))
#define MFG_RGX_READ32(addr)			  readl(rgx_base + (addr))

#ifdef writeq
#define MFG_RGX_WRITE64(addr, value)	writeq(value, rgx_base + (addr))
#define MFG_RGX_READ64(addr)		readq(rgx_base + (addr))
#else
/* Little endian support only */
#define MFG_RGX_READ64(addr) ((u64) (((u64)(readl(rgx_base + (addr))) << 32) | readl(rgx_base + (addr))))
#define MFG_RGX_WRITE64(addr, value) \
do { \
	writel((u32)((value) & 0xffffffff), rgx_base + (addr));  \
	writel((u32)((u64)((value) >> 32) & 0xffffffff), rgx_base + (addr) + 4); \
} while (0)
#endif

static int power_state;

/*
 * Read func will be in trace_lock.
 */
typedef void (*mfg_counter_update_pfn)(struct mfg_counter *counter);

static struct mfg_counter {
	const char *name;
	u32 indirect_inst;	/*0, 1, 2 ... */
	u32 indirect_offset;
	u32 offset;
	mfg_counter_update_pfn update;
	u64 value;
	u64 prev_value;
	int type;
};

static void rgx_base_update32(struct mfg_counter *counter)
{
	u64 val;

	if (power_state > 0) {
		if (counter->indirect_offset)
			MFG_RGX_WRITE32(
				counter->indirect_offset,
				counter->indirect_inst);

		val = MFG_RGX_READ32(counter->offset);
		if (val < counter->prev_value) {
			pr_err(">> overflowed...!!! offset %d, val: %llx, prev %llx\n", val, counter->prev_value);
			val += 0xFFFFFFFFULL;
		}
		counter->prev_value = counter->value;
		counter->value = val;
	} else {
		counter->prev_value = 0;
		counter->value = 0;
	}
}

static void gpu_freq_update(struct mfg_counter *counter)
{
	counter->prev_value = counter->value;
	counter->value = mtk_mfg_get_snap_freq();
}

static void gpu_loading_update(struct mfg_counter *counter)
{
	counter->prev_value = counter->value;
	counter->value = (u64)mtk_gpu_misc_get_util(NULL, NULL, NULL, NULL);
}

static void gpu_timestamp_update(struct mfg_counter *counter)
{
	counter->prev_value = counter->value;
	counter->value = (u64)ktime_get_ns();
}

struct mfg_counter mfg_counters[] = {
	{
		"TA_OR_3D",
		0, 0, 0x6038,
		rgx_base_update32,
		0, 0, MFG_COUNTER_CONTINUE
	},
	{
		"TPU_TEXELS",
		0, 0x8008, 0x17b0,
		rgx_base_update32,
		0, 0,
		MFG_COUNTER_CONTINUE
	},
	{
		"USC_PWR_INSTR",
		0, 0, 0x4600,
		rgx_base_update32,
		0, 0,
		MFG_COUNTER_CONTINUE
	},
	{
		"USC_PWR_NUM_FLOAT",
		0, 0, 0x4620,
		rgx_base_update32,
		0, 0,
		MFG_COUNTER_CONTINUE
	},
	{
		"USC_PWR_NUM_F16",
		0, 0, 0x4658,
		rgx_base_update32,
		0, 0,
		MFG_COUNTER_CONTINUE
	},
	{
		"PBE_PWR",
		0, 0, 0x1590,
		rgx_base_update32,
		0, 0,
		MFG_COUNTER_CONTINUE
	},
	{
		"gpu_freq",
		0, 0, 0,
		gpu_freq_update,
		0, 0,
		MFG_COUNTER_VALUE
	},
	{
		"gpu_loading",
		0, 0, 0,
		gpu_loading_update,
		0, 0,
		MFG_COUNTER_VALUE
	},
	{
		"timestamp",
		0, 0, 0,
		gpu_timestamp_update,
		0, 0,
		MFG_COUNTER_CONTINUE
	},
};

#define MFG_COUNTER_SIZE (ARRAY_SIZE(mfg_counters))

struct mfg_counter_hist {
	u64 value[MFG_COUNTER_SIZE];
};

static int hist_head;
static int hist_len;
static int hist_period = MFG_COUNTER_DEFAULT_PERIOD;
static struct mfg_counter_hist *hist;

static struct task_struct *counter_thd;


static int state = MFG_COUNTER_STATE_NOT_READY;
DECLARE_WAIT_QUEUE_HEAD(trace_wq);
/*
 * require: get trace_lock
 */
static void mfg_counter_start(void)
{
	MFG_RGX_WRITE32(0x6330U, 0x1);
	MFG_RGX_WRITE32(0x6300U, 0x1);
	MFG_RGX_WRITE32(0x6300U, 0x0);
}

/*
 * require: get trace_lock
 */
static void mfg_counter_update(void)
{
	int i;

	for (i = 0; i < MFG_COUNTER_SIZE; i++)
		mfg_counters[i].update(&mfg_counters[i]);
}

/*
 * require: get trace_lock
 */
static bool mfg_counter_trace(void)
{
	int i;
	struct mfg_counter_hist *rec;

	mfg_counter_update();
	if (hist_head < hist_len) {
		rec = &hist[hist_head];
		for (i = 0; i < MFG_COUNTER_SIZE; i++) {
			if (mfg_counters[i].type == MFG_COUNTER_VALUE) {
				rec->value[i] = mfg_counters[i].value;
			} else {/* CONTINUE */
				rec->value[i] = mfg_counters[i].value - mfg_counters[i].prev_value;
			}
		}
		hist_head++;
	} else {
		pr_info("Record done %p\n", hist);
		return true;
	}
	return false;
}

static int mfg_counter_func(void *arg)
{
	while (!kthread_should_stop()) {
		wait_event_interruptible(trace_wq,
			state == MFG_COUNTER_STATE_TRACING);

		usleep_range(hist_period, hist_period + 10);

		mutex_lock(&trace_lock);
		/* Check state again to ensure recording while TRACING state*/
		if (state == MFG_COUNTER_STATE_TRACING) {
			if (mfg_counter_trace())
				state = MFG_COUNTER_STATE_FINISHED;
		}
		mutex_unlock(&trace_lock);
	}

	return 0;
}

void mfg_counter_start_trace(void)
{
	mutex_lock(&trace_lock);

	if (state == MFG_COUNTER_STATE_NOT_READY) {
		pr_err("MFG counter is not inited\n");
		goto error_out;
	}
	if (state == MFG_COUNTER_STATE_TRACING) {
		pr_err("MFG counter is tracing\n");
		goto error_out;
	}

	hist_head = 0;

	/* HW Start counter and update init value*/
	mfg_counter_start();
	mfg_counter_update();

	/* Take first??? mfg_counter_update(); */

	state  = MFG_COUNTER_STATE_TRACING;
	wake_up_interruptible(&trace_wq);

error_out:
	mutex_unlock(&trace_lock);

	return;

}

void mfg_counter_stop_trace(void)
{
	mutex_lock(&trace_lock);
	if (state != MFG_COUNTER_STATE_TRACING) {
		mutex_unlock(&trace_lock);
		pr_err("MFG counter is not tracing.\n");
		return;
	}
	state = MFG_COUNTER_STATE_FINISHED;
	wake_up_interruptible(&trace_wq);

	mutex_unlock(&trace_lock);
}

void mfg_counter_show_trace(struct seq_file *m)
{
	int i, j;

	mutex_lock(&trace_lock);

	if (state == MFG_COUNTER_STATE_NOT_READY) {
		pr_err("MFG counter is not inited\n");
		goto error_out;
	} else if (state == MFG_COUNTER_STATE_TRACING) {
		pr_err("MFG counter is tracing\n");
		goto error_out;
	} else if (state == MFG_COUNTER_STATE_NO_DATA) {
		pr_err("No data, please echo start/stop to counter\n");
		goto error_out;
	}

	for (i = 0; i < MFG_COUNTER_SIZE; i++)
		seq_printf(m, " %-30s \t",	mfg_counters[i].name);

	seq_puts(m, "\n");
	for (j = 0; j < hist_len; j++) {
		for (i = 0; i < MFG_COUNTER_SIZE; i++)
			seq_printf(m, " %-30llu \t", hist[j].value[i]);

		seq_puts(m, "\n");
	}
error_out:
	mutex_unlock(&trace_lock);
}


int mfg_counter_set_hist_size(int period, int len)
{
	void *tmp;

	if (period == 0)
		period = MFG_COUNTER_DEFAULT_PERIOD;

	if (len == 0)
		len = MFG_COUNTER_DEFAULT_HIST_SIZE;

	mutex_lock(&trace_lock);
	if (state == MFG_COUNTER_STATE_TRACING && state != MFG_COUNTER_STATE_NOT_READY) {
		pr_err("Cannot set hist len while tracing or uninited\n");
		mutex_unlock(&trace_lock);
		return -1;
	}
	mutex_unlock(&trace_lock);

	tmp = vzalloc(sizeof(struct mfg_counter_hist) * len);
	if (!tmp)
		return -1;

	void *to_free = tmp;

	mutex_lock(&trace_lock);
	if (state != MFG_COUNTER_STATE_TRACING && state != MFG_COUNTER_STATE_NOT_READY) {
		to_free = hist;
		hist_head = 0;
		hist_len = len;
		hist = tmp;
		hist_period = period;
	} else {
		pr_err("Cannot set hist len while tracing or uninited\n");
	}
	mutex_unlock(&trace_lock);

	vfree(to_free);

	/* == tmp while not set successfully */
	return to_free == tmp;
}

int mfg_counter_init(void)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8695-clark");
	if (node)
		rgx_base = of_iomap(node, 0);

	if (IS_ERR_OR_NULL(rgx_base)) {
		pr_err("cannot find of_node and map\n");
		goto error_out;
	}

	hist_head = 0;
	hist_period = MFG_COUNTER_DEFAULT_PERIOD;
	hist_len = MFG_COUNTER_DEFAULT_HIST_SIZE;
	hist = vzalloc(sizeof(struct mfg_counter_hist) * hist_len);
	if (!hist) {
		pr_err("Cannot alloc hist buffer %d\n", hist_len);
		goto error_out;
	}

	state = MFG_COUNTER_STATE_NO_DATA;

	counter_thd = kthread_create(mfg_counter_func, NULL, "mfg-counter-thd");
	if (IS_ERR(counter_thd)) {
		counter_thd = 0;
		pr_err("Cannot create counter_thd\n");
		goto error_out;
	}
	wake_up_process(counter_thd);

	return 0;

error_out:
	if (counter_thd)
		kthread_stop(counter_thd);

	if (rgx_base)
		iounmap(rgx_base);

	if (hist)
		vfree(hist);

	state = MFG_COUNTER_STATE_NOT_READY;
	return -1;
}

void mfg_counter_power_on(void)
{
	mutex_lock(&trace_lock);
	power_state++;
	mutex_unlock(&trace_lock);
}

void mfg_counter_power_off(void)
{
	mutex_lock(&trace_lock);

	/*
	 * Force add a trace if powered off.
	 */
	if (state == MFG_COUNTER_STATE_TRACING)
		mfg_counter_trace();

	power_state--;
	mutex_unlock(&trace_lock);
}

void mfg_counter_deinit(void)
{
	if (counter_thd)
		kthread_stop(counter_thd);

	if (rgx_base)
		iounmap(rgx_base);

	if (hist)
		vfree(hist);

}

#if 0
/* For MET ************************************
 */
int mtk_get_gpu_pmu_init(struct mtk_met_gpu_pmu_t *pmus, int pmu_size, int *ret_size)
{

}
EXPORT_SYMBOL(mtk_get_gpu_pmu_init);

int mtk_get_gpu_pmu_swapnreset(struct mtk_met_gpu_pmu_t *pmus, int pmu_size)
{

}
EXPORT_SYMBOL(mtk_get_gpu_pmu_swapnreset);
#endif

