/* drivers/gpu/vithar/kbase/src/platform/mali_kbase_dvfs.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T604 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file mali_kbase_dvfs.c
 * DVFS
 */

#include <osk/mali_osk.h>
#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_kbase_uku.h>
#include <kbase/src/common/mali_kbase_mem.h>
#include <kbase/src/common/mali_midg_regmap.h>
#include <kbase/src/linux/mali_kbase_mem_linux.h>
#include <uk/mali_ukk.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/miscdevice.h>
#include <linux/list.h>
#include <linux/semaphore.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>

#include <mach/map.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <mach/regs-clock.h>
#include <asm/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <kbase/src/platform/mali_kbase_dvfs.h>
#include <kbase/src/common/mali_kbase_gator.h>
#ifdef CONFIG_EXYNOS5_CPUFREQ
#include <mach/cpufreq.h>
#endif

#ifdef MALI_DVFS_ASV_ENABLE
#include <mach/asv.h>
#define MALI_DVFS_ASV_GROUP_SPECIAL_NUM 10
#define MALI_DVFS_ASV_GROUP_NUM 12
#endif

#ifdef CONFIG_REGULATOR
static struct regulator *g3d_regulator=NULL;
#ifdef CONFIG_VITHAR_HWVER_R0P0
static int mali_gpu_vol = 1250000; /* 1.25V @ 533 MHz */
#else
static int mali_gpu_vol = 1050000; /* 1.05V @ 266 MHz */
#endif
#endif

/***********************************************************/
/*  This table and variable are using the check time share of GPU Clock  */
/***********************************************************/
static unsigned long long prev_time = 0;

mali_time_in_state time_in_state[MALI_DVFS_STEP]=
{
#if (MALI_DVFS_STEP == 7)
	{100, 0},
	{160, 0},
	{266, 0},
	{350, 0},
	{400, 0},
	{450, 0},
	{533, 0}
#else
#error no table
#endif
};

typedef struct _mali_dvfs_info{
	unsigned int voltage;
	unsigned int clock;
	int min_threshold;
	int	max_threshold;
}mali_dvfs_info;

static mali_dvfs_info mali_dvfs_infotbl[MALI_DVFS_STEP]=
{
#if (MALI_DVFS_STEP == 7)
	{912500, 100, 0, 85},
	{925000, 160, 48, 85},
	{1025000, 266, 51, 87},
	{1075000, 350, 70, 80},
	{1125000, 400, 70, 80},
	{1150000, 450, 76, 99},
	{1250000, 533, 99, 100}
#else
#error no table
#endif
};

#ifdef CONFIG_VITHAR_DVFS

typedef struct _mali_dvfs_status_type{
	kbase_device *kbdev;
	int step;
	int utilisation;
	int up_keepcnt;
	int down_keepcnt;
	uint noutilcnt;
#ifdef CONFIG_VITHAR_FREQ_LOCK
	int upper_lock;
	int under_lock;
#endif
#ifdef MALI_DVFS_ASV_ENABLE
	int asv_need_update;
	int asv_group;
#endif
}mali_dvfs_status;

static struct workqueue_struct *mali_dvfs_wq = 0;
int mali_dvfs_control=0;
osk_spinlock mali_dvfs_spinlock;

#ifdef MALI_DVFS_ASV_ENABLE
#if (MALI_DVFS_STEP != 7)
#error DVFS_ASV support only 6steps
#endif

#if (MALI_DVFS_STEP == 7)
static const unsigned int mali_dvfs_asv_vol_tbl_special[MALI_DVFS_ASV_GROUP_SPECIAL_NUM][MALI_DVFS_STEP]=
{
	/*  100Mh   160Mh     266Mh   350Mh		400Mh   450Mh   533Mh*/
	{/*Group 1*/
		912500, 925000, 1025000, 1075000, 1100000, 1150000, 1225000,
	},
	{/*Group 2*/
		900000, 900000, 1000000, 1037500, 1087500, 1125000, 1200000,
	},
	{/*Group 3*/
		912500, 925000, 1025000, 1037500, 1100000, 1150000, 1225000,
	},
	{/*Group 4*/
		900000, 900000, 1000000, 1025000, 1087500, 1125000, 1200000,
	},
	{/*Group 5*/
		912500, 925000, 1000000, 1000000, 1125000, 1150000, 1250000,
	},
	{/*Group 6*/
		900000, 912500, 987500, 987500, 1112500, 1150000, 1237500,
	},
	{/*Group 7*/
		900000, 900000, 975000, 987500, 1100000, 1137500, 1225000,
	},
	{/*Group 8*/
		900000, 900000, 975000, 987500, 1100000, 1137500, 1225000,
	},
	{/*Group 9*/
		887500, 900000, 962500, 975000, 1087500, 1125000, 1212500,
	},
	{/*Group 10*/
		887500, 900000, 962500, 962500, 1087500, 1125000, 1212500,
	},
};
static const unsigned int mali_dvfs_asv_vol_tbl[MALI_DVFS_ASV_GROUP_NUM][MALI_DVFS_STEP]=
{
	/*  100Mh	160Mh	   266Mh	350Mh, 	400Mh	450Mh	533Mh*/
	{/*Group 0*/
		925000, 925000, 1025000, 1075000, 1125000, 1150000, 1200000,
	},
	{/*Group 1*/
		900000, 900000, 1000000, 1037500, 1087500, 1137500, 1187500,
	},
	{/*Group 2*/
		900000, 900000, 950000, 1037500, 1075000, 1125000, 1187500,
	},
	{/*Group 3*/
		900000, 900000, 950000, 1037500, 1075000, 1125000, 1187500,
	},
	{/*Group 4*/
		900000, 900000, 937500, 1025000, 1075000, 1112500, 1175000,
	},
	{/*Group 5*/
		900000, 900000, 937500, 1000000, 1050000, 1100000, 1150000,
	},
	{/*Group 6*/
		900000, 900000, 925000, 987500, 1037500, 1087500, 1137500,
	},
	{/*Group 7*/
		900000, 900000, 912500, 987500, 1025000, 1075000, 1125000,
	},
	{/*Group 8*/
		900000, 900000, 912500, 987500, 1012500, 1075000, 1125000,
	},
	{/*Group 9*/
		900000, 900000, 900000, 975000, 1012500, 1050000, 1125000,
	},
	{/*Group 10*/
		875000, 900000, 900000, 962500, 1000000, 1050000, 1112500,
	},
	{/*Group 11*/
		875000, 900000, 900000, 962500, 1000000, 1050000, 1112500,
	},
};
#endif
#endif

/*dvfs status*/
static mali_dvfs_status mali_dvfs_status_current;
#ifdef MALI_DVFS_ASV_ENABLE
#if (MALI_DVFS_STEP == 7)
static const unsigned int mali_dvfs_vol_default[MALI_DVFS_STEP]=
	{ 925000, 925000, 1025000, 1075000, 1125000, 1150000, 1200000};
#else
#error
#endif

static int mali_dvfs_update_asv(int group)
{
	int i;

	if (exynos_lot_id && group == 0) return 1;

	if (group == -1) {
		for (i=0; i<MALI_DVFS_STEP; i++)
		{
			mali_dvfs_infotbl[i].voltage = mali_dvfs_vol_default[i];
		}
		printk("mali_dvfs_update_asv use default table\n");
		return 1;
	}
	if (group > MALI_DVFS_ASV_GROUP_NUM) {
		OSK_PRINT_ERROR(OSK_BASE_PM, "invalid asv group (%d)\n", group);
		return 1;
	}
	for (i=0; i<MALI_DVFS_STEP; i++)
	{
		if (exynos_lot_id)
			mali_dvfs_infotbl[i].voltage = mali_dvfs_asv_vol_tbl_special[group-1][i];
		else
			mali_dvfs_infotbl[i].voltage = mali_dvfs_asv_vol_tbl[group][i];
	}
	return 0;
}
#endif
#ifdef CONFIG_EXYNOS5_CPUFREQ
void mali_dvfs_set_cpulock(int freq)
{
	static int _freq = -1;
	unsigned int level;

	if (_freq == freq)
		return;

	if (_freq!=-1)
		exynos_cpufreq_lock_free(DVFS_LOCK_ID_USER);

	switch(freq)
	{
		case 1000: case 900: case 800: case 700:
		case 600: case 500: case 400: case 300: case 200:
			if (exynos_cpufreq_get_level(freq * 1000, &level)) {
				printk("failed to get cpufreq level for %dMHz\n", freq);
				return;
			}

			if (exynos_cpufreq_lock(DVFS_LOCK_ID_USER, level)) {
				printk("failed to cpufreq lock for L%d\n", level);
				return;
			}

			printk("cpufreq locked on <%d>%dMHz\n", level, freq);
			break;
		case 0:
			//just free
			break;
		default:
			return;
	}

	_freq = freq;

	return;
}
#endif
static void mali_dvfs_event_proc(struct work_struct *w)
{
	mali_dvfs_status dvfs_status;
	int allow_up_cnt = 2;
	int allow_down_cnt = 10;

	osk_spinlock_lock(&mali_dvfs_spinlock);
	dvfs_status = mali_dvfs_status_current;
	osk_spinlock_unlock(&mali_dvfs_spinlock);

#ifdef MALI_DVFS_ASV_ENABLE
	if (dvfs_status.asv_need_update==2) {
		mali_dvfs_update_asv(-1);
		dvfs_status.asv_need_update=0;
	} else if (dvfs_status.asv_group!=(exynos_result_of_asv&0xf)) {
		if (mali_dvfs_update_asv(exynos_result_of_asv&0xf)==0) {
			dvfs_status.asv_group = (exynos_result_of_asv&0xf);
			dvfs_status.asv_need_update=0;
		}
	}
#endif

#if MALI_DVFS_START_MAX_STEP
	/*If no input is keeping for longtime, first step will be max step. */
	if (dvfs_status.noutilcnt > 20 && dvfs_status.utilisation > 0) {
		dvfs_status.step=kbase_platform_dvfs_get_level(266);
	}
#endif
	if (dvfs_status.utilisation == 100) {
		allow_up_cnt = 1;
	} else if (mali_dvfs_infotbl[dvfs_status.step].clock < 266) {
		allow_up_cnt = 2;
		//mali_dvfs_set_cpulock(0);
	} else if (mali_dvfs_infotbl[dvfs_status.step].clock == 266){
		allow_up_cnt = 10;
		//mali_dvfs_set_cpulock(400);
	} else {
		allow_up_cnt = 3;
		allow_down_cnt = 2;
	}

	if (dvfs_status.utilisation > mali_dvfs_infotbl[dvfs_status.step].max_threshold)
	{
		dvfs_status.up_keepcnt++;
		dvfs_status.down_keepcnt=0;
		if (dvfs_status.up_keepcnt > allow_up_cnt)
		{
			dvfs_status.step++;
			dvfs_status.up_keepcnt=0;
			OSK_ASSERT(dvfs_status.step < MALI_DVFS_STEP);
		}
	}else if ((dvfs_status.step>0) &&
			(dvfs_status.utilisation < mali_dvfs_infotbl[dvfs_status.step].min_threshold)) {
		dvfs_status.down_keepcnt++;
		dvfs_status.up_keepcnt=0;
		if (dvfs_status.down_keepcnt > allow_down_cnt)
		{
			OSK_ASSERT(dvfs_status.step > 0);
			dvfs_status.step--;
			dvfs_status.down_keepcnt=0;
		}
	}else{
		dvfs_status.up_keepcnt=0;
		dvfs_status.down_keepcnt=0;
	}

#ifdef CONFIG_VITHAR_FREQ_LOCK
	if ((dvfs_status.upper_lock >= 0)&&(dvfs_status.step > dvfs_status.upper_lock)) {
		dvfs_status.step = dvfs_status.upper_lock;
		if ((dvfs_status.under_lock > 0)&&(dvfs_status.under_lock > dvfs_status.upper_lock)) {
			dvfs_status.under_lock = dvfs_status.upper_lock;
		}
	}
	if (dvfs_status.under_lock > 0) {
		if (dvfs_status.step < dvfs_status.under_lock)
			dvfs_status.step = dvfs_status.under_lock;
	}
#endif

	if (mali_dvfs_control==1)
		kbase_platform_dvfs_set_level(dvfs_status.kbdev, dvfs_status.step);

#if MALI_GATOR_SUPPORT
	kbase_trace_mali_timeline_event(GATOR_MAKE_EVENT(ACTIVITY_DVFS_CHANGED, ACTIVITY_DVFS) |((unsigned int)clk_get_rate(dvfs_status.kbdev->sclk_g3d)/1000000));
	kbase_trace_mali_timeline_event(GATOR_MAKE_EVENT(ACTIVITY_DVFS_UTILISATION_CHANGED, ACTIVITY_DVFS_UTILISATION) | dvfs_status.utilisation);
#endif

#if MALI_DVFS_START_MAX_STEP
	if (dvfs_status.utilisation == 0) {
		dvfs_status.noutilcnt++;
	} else {
		dvfs_status.noutilcnt=0;
	}
#endif

#if MALI_DVFS_DEBUG
	printk("[mali_dvfs] utilisation: %d step: %d[%d,%d] cnt: %d/%d\n",
			dvfs_status.utilisation, dvfs_status.step,
			mali_dvfs_infotbl[dvfs_status.step].min_threshold,
			mali_dvfs_infotbl[dvfs_status.step].max_threshold,
			dvfs_status.up_keepcnt,dvfs_status.down_keepcnt);
#endif

	osk_spinlock_lock(&mali_dvfs_spinlock);
	mali_dvfs_status_current=dvfs_status;
	osk_spinlock_unlock(&mali_dvfs_spinlock);

}

static DECLARE_WORK(mali_dvfs_work, mali_dvfs_event_proc);

int kbase_platform_dvfs_event(struct kbase_device *kbdev, u32 utilisation)
{
	osk_spinlock_lock(&mali_dvfs_spinlock);
	mali_dvfs_status_current.utilisation = utilisation;
	osk_spinlock_unlock(&mali_dvfs_spinlock);
	queue_work_on(0, mali_dvfs_wq, &mali_dvfs_work);

	/*add error handle here*/
	return MALI_TRUE;
}

int kbase_platform_dvfs_get_utilisation(void)
{
	int utilisation = 0;

	osk_spinlock_lock(&mali_dvfs_spinlock);
	utilisation = mali_dvfs_status_current.utilisation;
	osk_spinlock_unlock(&mali_dvfs_spinlock);

	return utilisation;
}

int kbase_platform_dvfs_get_control_status(void)
{
	return mali_dvfs_control;
}

int kbase_platform_dvfs_set_control_status(int onoff)
{
	switch(onoff)
	{
	case 0:case 1:
		mali_dvfs_control = onoff;
		printk("mali_dvfs_control is changed to %d\n", mali_dvfs_control);
		return MALI_TRUE;
	}
	return MALI_FALSE;
}

int kbase_platform_dvfs_init(struct device *dev)
{
	struct kbase_device *kbdev;
	kbdev = dev_get_drvdata(dev);

	/*default status
	  add here with the right function to get initilization value.
	 */
	if (!mali_dvfs_wq)
		mali_dvfs_wq = create_singlethread_workqueue("mali_dvfs");

	osk_spinlock_init(&mali_dvfs_spinlock,OSK_LOCK_ORDER_PM_METRICS);

	/*add a error handling here*/
	osk_spinlock_lock(&mali_dvfs_spinlock);
	mali_dvfs_status_current.kbdev = kbdev;
	mali_dvfs_status_current.utilisation = 100;
	mali_dvfs_status_current.step = MALI_DVFS_STEP-1;
#ifdef CONFIG_VITHAR_FREQ_LOCK
	mali_dvfs_status_current.upper_lock = -1;
	mali_dvfs_status_current.under_lock = -1;
#endif
#ifdef MALI_DVFS_ASV_ENABLE
	mali_dvfs_status_current.asv_need_update=1;
	mali_dvfs_status_current.asv_group=-1;
#endif
	mali_dvfs_control=1;
	osk_spinlock_unlock(&mali_dvfs_spinlock);

	// this operation is set the external variable for time_in_state sysfile.
	// this variable "prev_time" is Using the time share of GPU clock( mali_kbase_platform.c ).
	if( prev_time == 0 )
		prev_time = get_jiffies_64();

	return MALI_TRUE;
}

void kbase_platform_dvfs_term(void)
{
	if (mali_dvfs_wq)
		destroy_workqueue(mali_dvfs_wq);

	mali_dvfs_wq = NULL;
}
#endif

int mali_get_dvfs_upper_locked_freq(void)
{
	unsigned int locked_level = -1;

#ifdef CONFIG_VITHAR_FREQ_LOCK
	osk_spinlock_lock(&mali_dvfs_spinlock);
	locked_level = mali_dvfs_infotbl[mali_dvfs_status_current.upper_lock].clock;
	osk_spinlock_unlock(&mali_dvfs_spinlock);
#endif
	return locked_level;
}

int mali_get_dvfs_under_locked_freq(void)
{
	unsigned int locked_level = -1;

#ifdef CONFIG_VITHAR_FREQ_LOCK
	osk_spinlock_lock(&mali_dvfs_spinlock);
	locked_level = mali_dvfs_infotbl[mali_dvfs_status_current.under_lock].clock;
	osk_spinlock_unlock(&mali_dvfs_spinlock);
#endif

	return locked_level;
}


int mali_dvfs_freq_lock(int level)
{
#ifdef CONFIG_VITHAR_FREQ_LOCK
	osk_spinlock_lock(&mali_dvfs_spinlock);
	mali_dvfs_status_current.upper_lock = level;
	osk_spinlock_unlock(&mali_dvfs_spinlock);
#endif
	return 0;
}
void mali_dvfs_freq_unlock(void)
{
#ifdef CONFIG_VITHAR_FREQ_LOCK
	osk_spinlock_lock(&mali_dvfs_spinlock);
	mali_dvfs_status_current.upper_lock = -1;
	osk_spinlock_unlock(&mali_dvfs_spinlock);
#endif
}

int mali_dvfs_freq_under_lock(int level)
{
#ifdef CONFIG_VITHAR_FREQ_LOCK
	osk_spinlock_lock(&mali_dvfs_spinlock);
	mali_dvfs_status_current.under_lock = level;
	osk_spinlock_unlock(&mali_dvfs_spinlock);
#endif
	return 0;
}
void mali_dvfs_freq_under_unlock(void)
{
#ifdef CONFIG_VITHAR_FREQ_LOCK
	osk_spinlock_lock(&mali_dvfs_spinlock);
	mali_dvfs_status_current.under_lock = -1;
	osk_spinlock_unlock(&mali_dvfs_spinlock);
#endif
}

int kbase_platform_regulator_init(struct device *dev)
{

#ifdef CONFIG_REGULATOR
	g3d_regulator = regulator_get(NULL, "vdd_g3d");
	if(IS_ERR(g3d_regulator))
	{
		printk("[kbase_platform_regulator_init] failed to get vithar regulator\n");
		return -1;
	}

	if(regulator_enable(g3d_regulator) != 0)
	{
		printk("[kbase_platform_regulator_init] failed to enable vithar regulator\n");
		return -1;
	}

	if(regulator_set_voltage(g3d_regulator, mali_gpu_vol, mali_gpu_vol) != 0)
	{
		printk("[kbase_platform_regulator_init] failed to set vithar operating voltage [%d]\n", mali_gpu_vol);
		return -1;
	}
#endif

	return 0;
}

int kbase_platform_regulator_disable(struct device *dev)
{
#ifdef CONFIG_REGULATOR
	if(!g3d_regulator)
	{
		printk("[kbase_platform_regulator_disable] g3d_regulator is not initialized\n");
		return -1;
	}

	if(regulator_disable(g3d_regulator) != 0)
	{
		printk("[kbase_platform_regulator_disable] failed to disable g3d regulator\n");
		return -1;
	}
#endif
	return 0;
}

int kbase_platform_regulator_enable(struct device *dev)
{
#ifdef CONFIG_REGULATOR
	if(!g3d_regulator)
	{
		printk("[kbase_platform_regulator_enable] g3d_regulator is not initialized\n");
		return -1;
	}

	if(regulator_enable(g3d_regulator) != 0)
	{
		printk("[kbase_platform_regulator_enable] failed to enable g3d regulator\n");
		return -1;
	}
#endif
	return 0;
}

int kbase_platform_get_default_voltage(struct device *dev, int *vol)
{
#ifdef CONFIG_REGULATOR
	*vol = mali_gpu_vol;
#else
	*vol = 0;
#endif
	return 0;
}

int kbase_platform_get_voltage(struct device *dev, int *vol)
{
#ifdef CONFIG_REGULATOR
	if(!g3d_regulator)
	{
		printk("[kbase_platform_get_voltage] g3d_regulator is not initialized\n");
		return -1;
	}

	*vol = regulator_get_voltage(g3d_regulator);
#else
	*vol = 0;
#endif
	return 0;
}

int kbase_platform_set_voltage(struct device *dev, int vol)
{
#ifdef CONFIG_REGULATOR
	if(!g3d_regulator)
	{
		printk("[kbase_platform_set_voltage] g3d_regulator is not initialized\n");
		return -1;
	}

	if(regulator_set_voltage(g3d_regulator, vol, vol) != 0)
	{
		printk("[kbase_platform_set_voltage] failed to set voltage\n");
		return -1;
	}
#endif
	return 0;
}

void kbase_platform_dvfs_set_clock(kbase_device *kbdev, int freq)
{
	static struct clk * mout_gpll = NULL;
	static struct clk * fin_gpll = NULL;
	static struct clk * fout_gpll = NULL;
	static int _freq = -1;
	static unsigned long gpll_rate_prev = 0;
	unsigned long gpll_rate = 0, aclk_400_rate = 0;
	unsigned long tmp = 0;

	if (!kbdev)
		panic("oops");

	if (mout_gpll==NULL) {
		mout_gpll = clk_get(kbdev->osdev.dev, "mout_gpll");
		fin_gpll = clk_get(kbdev->osdev.dev, "ext_xtal");
		fout_gpll = clk_get(kbdev->osdev.dev, "fout_gpll");
		if(IS_ERR(mout_gpll) || IS_ERR(fin_gpll) || IS_ERR(fout_gpll))
			panic("clk_get ERROR");
	}

	if(kbdev->sclk_g3d == 0)
		return;

	if (freq == _freq)
		return;

	switch(freq)
	{
		case 533:
			gpll_rate = 533000000;
			aclk_400_rate = 533000000;
			break;
		case 450:
			gpll_rate = 450000000;
			aclk_400_rate = 450000000;
			break;
		case 400:
			gpll_rate = 800000000;
			aclk_400_rate = 400000000;
			break;
		case 350:
			gpll_rate = 1400000000;
			aclk_400_rate = 350000000;
			break;
		case 266:
			gpll_rate = 800000000;
			aclk_400_rate = 267000000;
			break;
		case 160:
			gpll_rate = 800000000;
			aclk_400_rate = 160000000;
			break;
		case 100:
			gpll_rate = 800000000;
			aclk_400_rate = 100000000;
			break;
		default:
			return;
	}

	/* if changed the GPLL rate, set rate for GPLL and wait for lock time */
	if( gpll_rate != gpll_rate_prev) {
		/*for stable clock input.*/
		clk_set_rate(kbdev->sclk_g3d, 100000000);
		clk_set_parent(mout_gpll, fin_gpll);

		/*change gpll*/
		clk_set_rate( fout_gpll, gpll_rate );

		/*restore parent*/
		clk_set_parent(mout_gpll, fout_gpll);
		gpll_rate_prev = gpll_rate;
	}

	_freq = freq;
	clk_set_rate(kbdev->sclk_g3d, aclk_400_rate);

	/* Waiting for clock is stable */
	do {
		tmp = __raw_readl(EXYNOS5_CLKDIV_STAT_TOP0);
	} while (tmp & 0x1000000);
#ifdef CONFIG_VITHAR_DVFS
#if MALI_DVFS_DEBUG
	printk("aclk400 %u[%d]\n", (unsigned int)clk_get_rate(kbdev->sclk_g3d),mali_dvfs_status_current.utilisation);
	printk("dvfs_set_clock GPLL : %lu, ACLK_400 : %luMhz\n", gpll_rate, aclk_400_rate );
#endif
#endif
	return;
}

static void kbase_platform_dvfs_set_vol(unsigned int vol)
{
	static int _vol = -1;

	if (_vol == vol)
		return;

	kbase_platform_set_voltage(NULL, vol);
	_vol = vol;
#if MALI_DVFS_DEBUG
	printk("dvfs_set_vol %dmV\n", vol);
#endif
	return;
}

int kbase_platform_dvfs_get_level(int freq)
{
	int i;
	for (i=0; i < MALI_DVFS_STEP;i++)
	{
		if (mali_dvfs_infotbl[i].clock == freq)
			return i;
	}
	return -1;
}

void kbase_platform_dvfs_set_level(kbase_device *kbdev, int level)
{
#if MALI_DVFS_START_MAX_STEP
	static int level_prev = MALI_DVFS_STEP-1;
#else
	static int level_prev = 0;
#endif
	unsigned long long current_time;

	if (level == level_prev)
		return;

	if (WARN_ON((level >= MALI_DVFS_STEP)||(level < 0)))
		panic("invalid level");

	if (level > level_prev) {
		kbase_platform_dvfs_set_vol(mali_dvfs_infotbl[level].voltage);
		kbase_platform_dvfs_set_clock(kbdev, mali_dvfs_infotbl[level].clock);
	}else{
		kbase_platform_dvfs_set_clock(kbdev, mali_dvfs_infotbl[level].clock);
		kbase_platform_dvfs_set_vol(mali_dvfs_infotbl[level].voltage);
	}
	// Calculate the time share of input DVFS level.
	current_time = get_jiffies_64();

	time_in_state[level_prev].time = cputime64_add(time_in_state[level_prev].time, cputime_sub(current_time, prev_time));

	prev_time = current_time;
	level_prev = level;
}

int kbase_platform_dvfs_sprint_avs_table(char *buf)
{
#ifdef MALI_DVFS_ASV_ENABLE
	int i, cnt=0;
	if (buf==NULL)
		return 0;

	cnt+=sprintf(buf,"asv group:%d exynos_lot_id:%d\n",exynos_result_of_asv&0xf, exynos_lot_id);
	for (i=MALI_DVFS_STEP-1; i >= 0; i--) {
		cnt+=sprintf(buf+cnt,"%dMhz:%d\n",
				mali_dvfs_infotbl[i].clock, mali_dvfs_infotbl[i].voltage);
	}
	return cnt;
#else
	return 0;
#endif
}

int kbase_platform_dvfs_set(int enable)
{
#ifdef MALI_DVFS_ASV_ENABLE
	osk_spinlock_lock(&mali_dvfs_spinlock);
	if (enable) {
		mali_dvfs_status_current.asv_need_update=1;
		mali_dvfs_status_current.asv_group=-1;
	}else{
		mali_dvfs_status_current.asv_need_update=2;
	}
	osk_spinlock_unlock(&mali_dvfs_spinlock);
#endif
	return 0;
}
