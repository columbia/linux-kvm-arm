/*
 * AMD 10Gb Ethernet PHY driver
 *
 * This file is available to you under your choice of the following two
 * licenses:
 *
 * License 1: GPLv2
 *
 * Copyright (c) 2014 Advanced Micro Devices, Inc.
 *
 * This file is free software; you may copy, redistribute and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or (at
 * your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * License 2: Modified BSD
 *
 * Copyright (c) 2014 Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Advanced Micro Devices, Inc. nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/unistd.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>
#include <linux/mdio.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>
#include <linux/acpi.h>

MODULE_AUTHOR("Tom Lendacky <thomas.lendacky@amd.com>");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION("0.0.0-a");
MODULE_DESCRIPTION("AMD 10GbE (amd-xgbe) PHY driver");

#define XGBE_PHY_ID	0x7996ced0
#define XGBE_PHY_MASK	0xfffffff0

#define XGBE_PHY_SERDES_RETRY		32
#define XGBE_PHY_CHANNEL_PROPERTY	"amd,serdes-channel"
#define XGBE_PHY_SPEEDSET_PROPERTY	"amd,speed-set"

#define XGBE_AN_INT_CMPLT		0x01
#define XGBE_AN_INC_LINK		0x02
#define XGBE_AN_PG_RCV			0x04

#define XNP_MCF_NULL_MESSAGE		0x001
#define XNP_ACK_PROCESSED		(1 << 12)
#define XNP_MP_FORMATTED		(1 << 13)
#define XNP_NP_EXCHANGE			(1 << 15)

#define XGBE_PHY_RATECHANGE_COUNT	500

#ifndef MDIO_PMA_10GBR_PMD_CTRL
#define MDIO_PMA_10GBR_PMD_CTRL		0x0096
#endif

#ifndef MDIO_PMA_10GBR_FEC_CTRL
#define MDIO_PMA_10GBR_FEC_CTRL		0x00ab
#endif

#ifndef MDIO_AN_XNP
#define MDIO_AN_XNP			0x0016
#endif

#ifndef MDIO_AN_INTMASK
#define MDIO_AN_INTMASK			0x8001
#endif

#ifndef MDIO_AN_INT
#define MDIO_AN_INT			0x8002
#endif

#ifndef MDIO_AN_KR_CTRL
#define MDIO_AN_KR_CTRL			0x8003
#endif

#ifndef MDIO_CTRL1_SPEED1G
#define MDIO_CTRL1_SPEED1G		(MDIO_CTRL1_SPEED10G & ~BMCR_SPEED100)
#endif

#ifndef MDIO_KR_CTRL_PDETECT
#define MDIO_KR_CTRL_PDETECT		0x01
#endif

/* SerDes integration register offsets */
#define SIR0_KR_RT_1			0x002c
#define SIR0_STATUS			0x0040
#define SIR1_SPEED			0x0000

/* SerDes integration register entry bit positions and sizes */
#define SIR0_KR_RT_1_RESET_INDEX	11
#define SIR0_KR_RT_1_RESET_WIDTH	1
#define SIR0_STATUS_RX_READY_INDEX	0
#define SIR0_STATUS_RX_READY_WIDTH	1
#define SIR0_STATUS_TX_READY_INDEX	8
#define SIR0_STATUS_TX_READY_WIDTH	1
#define SIR1_SPEED_DATARATE_INDEX	4
#define SIR1_SPEED_DATARATE_WIDTH	2
#define SIR1_SPEED_PI_SPD_SEL_INDEX	12
#define SIR1_SPEED_PI_SPD_SEL_WIDTH	4
#define SIR1_SPEED_PLLSEL_INDEX		3
#define SIR1_SPEED_PLLSEL_WIDTH		1
#define SIR1_SPEED_RATECHANGE_INDEX	6
#define SIR1_SPEED_RATECHANGE_WIDTH	1
#define SIR1_SPEED_TXAMP_INDEX		8
#define SIR1_SPEED_TXAMP_WIDTH		4
#define SIR1_SPEED_WORDMODE_INDEX	0
#define SIR1_SPEED_WORDMODE_WIDTH	3

#define SPEED_10000_CDR			0x7
#define SPEED_10000_PLL			0x1
#define SPEED_10000_RATE		0x0
#define SPEED_10000_TXAMP		0xa
#define SPEED_10000_WORD		0x7

#define SPEED_2500_CDR			0x2
#define SPEED_2500_PLL			0x0
#define SPEED_2500_RATE			0x1
#define SPEED_2500_TXAMP		0xf
#define SPEED_2500_WORD			0x1

#define SPEED_1000_CDR			0x2
#define SPEED_1000_PLL			0x0
#define SPEED_1000_RATE			0x3
#define SPEED_1000_TXAMP		0xf
#define SPEED_1000_WORD			0x1

/* SerDes RxTx register offsets */
#define RXTX_REG20			0x0050
#define RXTX_REG114			0x01c8

/* SerDes RxTx register entry bit positions and sizes */
#define RXTX_REG20_BLWC_ENA_INDEX	2
#define RXTX_REG20_BLWC_ENA_WIDTH	1
#define RXTX_REG114_PQ_REG_INDEX	9
#define RXTX_REG114_PQ_REG_WIDTH	7

#define RXTX_10000_BLWC			0
#define RXTX_10000_PQ			0x1e

#define RXTX_2500_BLWC			1
#define RXTX_2500_PQ			0xa

#define RXTX_1000_BLWC			1
#define RXTX_1000_PQ			0xa

#define GET_BITS(_var, _index, _width)					\
	(((_var) >> (_index)) & ((0x1 << (_width)) - 1))

#define SET_BITS(_var, _index, _width, _val)				\
do {									\
	(_var) &= ~(((0x1 << (_width)) - 1) << (_index));		\
	(_var) |= (((_val) & ((0x1 << (_width)) - 1)) << (_index));	\
} while (0)

#define XCMU_IOREAD(_priv, _reg)					\
	ioread16((_priv)->cmu_regs + _reg)

#define XCMU_IOWRITE(_priv, _reg, _val)					\
	iowrite16((_val), (_priv)->cmu_regs + _reg)

/* Macros for reading or writing SerDes RxTx registers
 *  The ioread macros will get bit fields or full values using the
 *  register definitions formed using the input names
 *
 *  The iowrite macros will set bit fields or full values using the
 *  register definitions formed using the input names
 */
#define XRXTX_IOREAD(_priv, _reg)					\
	ioread16((_priv)->rxtx_regs + _reg)

#define XRXTX_IOREAD_BITS(_priv, _reg, _field)				\
	GET_BITS(XRXTX_IOREAD((_priv), _reg),				\
		 _reg##_##_field##_INDEX,				\
		 _reg##_##_field##_WIDTH)

#define XRXTX_IOWRITE(_priv, _reg, _val)				\
	iowrite16((_val), (_priv)->rxtx_regs + _reg)

#define XRXTX_IOWRITE_BITS(_priv, _reg, _field, _val)			\
do {									\
	u16 reg_val = XRXTX_IOREAD((_priv), _reg);			\
	SET_BITS(reg_val,						\
		 _reg##_##_field##_INDEX,				\
		 _reg##_##_field##_WIDTH, (_val));			\
	XRXTX_IOWRITE((_priv), _reg, reg_val);				\
} while (0)

/* SerDes CMU register offsets */
#define CMU_REG15			0x003c
#define CMU_REG16			0x0040

/* SerDes CMU register entry bit positions and sizes */
#define CMU_REG16_TX_RATE_CHANGE_BASE	15
#define CMU_REG16_RX_RATE_CHANGE_BASE	14
#define CMU_REG16_RATE_CHANGE_DECR	2

/* SerDes RxTx register offsets */
#define RXTX_REG2			0x0008
#define RXTX_REG3			0x000c
#define RXTX_REG5			0x0014
#define RXTX_REG6			0x0018
#define RXTX_REG53			0x00d4
#define RXTX_REG115			0x01cc
#define RXTX_REG142			0x0238

/* SerDes RxTx register entry bit positions and sizes */
#define RXTX_REG2_RESETB_INDEX			15
#define RXTX_REG2_RESETB_WIDTH			1
#define RXTX_REG3_TX_DATA_RATE_INDEX		14
#define RXTX_REG3_TX_DATA_RATE_WIDTH		2
#define RXTX_REG3_TX_WORD_MODE_INDEX		11
#define RXTX_REG3_TX_WORD_MODE_WIDTH		3
#define RXTX_REG5_TXAMP_CNTL_INDEX		7
#define RXTX_REG5_TXAMP_CNTL_WIDTH		4
#define RXTX_REG6_RX_DATA_RATE_INDEX		9
#define RXTX_REG6_RX_DATA_RATE_WIDTH		2
#define RXTX_REG6_RX_WORD_MODE_INDEX		11
#define RXTX_REG6_RX_WORD_MODE_WIDTH		3
#define RXTX_REG20_BLWC_ENA_INDEX		2
#define RXTX_REG20_BLWC_ENA_WIDTH		1
#define RXTX_REG53_RX_PLLSELECT_INDEX		15
#define RXTX_REG53_RX_PLLSELECT_WIDTH		1
#define RXTX_REG53_TX_PLLSELECT_INDEX		14
#define RXTX_REG53_TX_PLLSELECT_WIDTH		1
#define RXTX_REG53_PI_SPD_SEL_CDR_INDEX		10
#define RXTX_REG53_PI_SPD_SEL_CDR_WIDTH		4
#define RXTX_REG114_PQ_REG_INDEX		9
#define RXTX_REG114_PQ_REG_WIDTH		7
#define RXTX_REG115_FORCE_LAT_CAL_START_INDEX	2
#define RXTX_REG115_FORCE_LAT_CAL_START_WIDTH	1
#define RXTX_REG115_FORCE_SUM_CAL_START_INDEX	1
#define RXTX_REG115_FORCE_SUM_CAL_START_WIDTH	1
#define RXTX_REG142_SUM_CALIB_DONE_INDEX	15
#define RXTX_REG142_SUM_CALIB_DONE_WIDTH	1
#define RXTX_REG142_SUM_CALIB_ERR_INDEX		14
#define RXTX_REG142_SUM_CALIB_ERR_WIDTH		1
#define RXTX_REG142_LAT_CALIB_DONE_INDEX	11
#define RXTX_REG142_LAT_CALIB_DONE_WIDTH	1

#define RXTX_FULL_RATE				0x0
#define RXTX_HALF_RATE				0x1
#define RXTX_FIFTH_RATE				0x3
#define RXTX_66BIT_WORD				0x7
#define RXTX_10BIT_WORD				0x1
#define RXTX_10G_TX_AMP				0xa
#define RXTX_1G_TX_AMP				0xf
#define RXTX_10G_CDR				0x7
#define RXTX_1G_CDR				0x2
#define RXTX_10G_PLL				0x1
#define RXTX_1G_PLL				0x0
#define RXTX_10G_PQ				0x1e
#define RXTX_1G_PQ				0xa

DEFINE_SPINLOCK(cmu_lock);

enum amd_xgbe_phy_an {
	AMD_XGBE_AN_READY = 0,
	AMD_XGBE_AN_START,
	AMD_XGBE_AN_EVENT,
	AMD_XGBE_AN_PAGE_RECEIVED,
	AMD_XGBE_AN_INCOMPAT_LINK,
	AMD_XGBE_AN_COMPLETE,
	AMD_XGBE_AN_NO_LINK,
	AMD_XGBE_AN_EXIT,
	AMD_XGBE_AN_ERROR,
};

enum amd_xgbe_phy_rx {
	AMD_XGBE_RX_READY = 0,
	AMD_XGBE_RX_BPA,
	AMD_XGBE_RX_XNP,
	AMD_XGBE_RX_COMPLETE,
};

enum amd_xgbe_phy_mode {
	AMD_XGBE_MODE_KR,
	AMD_XGBE_MODE_KX,
};

enum amd_xgbe_phy_speedset {
	AMD_XGBE_PHY_SPEEDSET_1000_10000 = 0,
	AMD_XGBE_PHY_SPEEDSET_2500_10000,
};

struct amd_xgbe_phy_priv {
	struct platform_device *pdev;
	struct acpi_device *adev;
	struct device *dev;

	struct phy_device *phydev;

	/* SerDes related mmio resources */
	struct resource *rxtx_res;
	struct resource *cmu_res;

	/* SerDes related mmio registers */
	void __iomem *rxtx_regs;	/* SerDes Rx/Tx CSRs */
	void __iomem *cmu_regs;		/* SerDes CMU CSRs */

	unsigned int serdes_channel;
	unsigned int speed_set;

	/* Maintain link status for re-starting auto-negotiation */
	unsigned int link;

	/* Auto-negotiation state machine support */
	struct mutex an_mutex;
	enum amd_xgbe_phy_an an_result;
	enum amd_xgbe_phy_an an_state;
	enum amd_xgbe_phy_rx kr_state;
	enum amd_xgbe_phy_rx kx_state;
	struct work_struct an_work;
	struct workqueue_struct *an_workqueue;
	unsigned int parallel_detect;
};

static int amd_xgbe_an_enable_kr_training(struct phy_device *phydev)
{
	int ret;

	ret = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_PMD_CTRL);
	if (ret < 0)
		return ret;

	ret |= 0x02;
	phy_write_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_PMD_CTRL, ret);

	return 0;
}

static int amd_xgbe_an_disable_kr_training(struct phy_device *phydev)
{
	int ret;

	ret = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_PMD_CTRL);
	if (ret < 0)
		return ret;

	ret &= ~0x02;
	phy_write_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_PMD_CTRL, ret);

	return 0;
}

static int amd_xgbe_phy_pcs_power_cycle(struct phy_device *phydev)
{
	int ret;

	ret = phy_read_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL1);
	if (ret < 0)
		return ret;

	ret |= MDIO_CTRL1_LPOWER;
	phy_write_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL1, ret);

	usleep_range(75, 100);

	ret &= ~MDIO_CTRL1_LPOWER;
	phy_write_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL1, ret);

	return 0;
}

static void amd_xgbe_phy_serdes_start_ratechange(struct phy_device *phydev)
{
	struct amd_xgbe_phy_priv *priv = phydev->priv;
	u16 val, mask;

	/* Assert Rx and Tx ratechange in CMU_reg16 */
	val = XCMU_IOREAD(priv, CMU_REG16);

	mask = (1 << (CMU_REG16_TX_RATE_CHANGE_BASE -
		      (priv->serdes_channel * CMU_REG16_RATE_CHANGE_DECR))) |
	       (1 << (CMU_REG16_RX_RATE_CHANGE_BASE -
		      (priv->serdes_channel * CMU_REG16_RATE_CHANGE_DECR)));
	val |= mask;

	XCMU_IOWRITE(priv, CMU_REG16, val);
}

static void amd_xgbe_phy_serdes_complete_ratechange(struct phy_device *phydev)
{
	struct amd_xgbe_phy_priv *priv = phydev->priv;
	u16 val, mask;
	unsigned int wait;

	/* Release Rx and Tx ratechange for proper channel in CMU_reg16 */
	val = XCMU_IOREAD(priv, CMU_REG16);

	mask = (1 << (CMU_REG16_TX_RATE_CHANGE_BASE -
		      (priv->serdes_channel * CMU_REG16_RATE_CHANGE_DECR))) |
	       (1 << (CMU_REG16_RX_RATE_CHANGE_BASE -
		      (priv->serdes_channel * CMU_REG16_RATE_CHANGE_DECR)));
	val &= ~mask;

	XCMU_IOWRITE(priv, CMU_REG16, val);

	/* Wait for Rx and Tx ready in CMU_reg15 */
	mask = (1 << priv->serdes_channel) |
	       (1 << (priv->serdes_channel + 8));
	wait = XGBE_PHY_RATECHANGE_COUNT;
	while (wait--) {
		udelay(50);

		val = XCMU_IOREAD(priv, CMU_REG15);
		if ((val & mask) == mask)
			return;
	}

	netdev_dbg(phydev->attached_dev, "SerDes rx/tx not ready (%#hx)\n",
		   val);
}

static int amd_xgbe_phy_xgmii_mode(struct phy_device *phydev)
{
	struct amd_xgbe_phy_priv *priv = phydev->priv;
	int ret;

	/* Disable KR training */
	ret = amd_xgbe_an_disable_kr_training(phydev);
	if (ret < 0)
		return ret;

	/* Set PCS to KR/10G speed */
	ret = phy_read_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL2);
	if (ret < 0)
		return ret;

	ret &= ~MDIO_PCS_CTRL2_TYPE;
	ret |= MDIO_PCS_CTRL2_10GBR;
	phy_write_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL2, ret);

	ret = phy_read_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL1);
	if (ret < 0)
		return ret;

	ret &= ~MDIO_CTRL1_SPEEDSEL;
	ret |= MDIO_CTRL1_SPEED10G;
	phy_write_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL1, ret);

	ret = amd_xgbe_phy_pcs_power_cycle(phydev);
	if (ret < 0)
		return ret;

	/* Set SerDes to 10G speed */
	spin_lock(&cmu_lock);

	amd_xgbe_phy_serdes_start_ratechange(phydev);

	XRXTX_IOWRITE_BITS(priv, RXTX_REG3, TX_DATA_RATE, RXTX_FULL_RATE);
	XRXTX_IOWRITE_BITS(priv, RXTX_REG3, TX_WORD_MODE, RXTX_66BIT_WORD);

	XRXTX_IOWRITE_BITS(priv, RXTX_REG5, TXAMP_CNTL, RXTX_10G_TX_AMP);

	XRXTX_IOWRITE_BITS(priv, RXTX_REG6, RX_DATA_RATE, RXTX_FULL_RATE);
	XRXTX_IOWRITE_BITS(priv, RXTX_REG6, RX_WORD_MODE, RXTX_66BIT_WORD);

	XRXTX_IOWRITE_BITS(priv, RXTX_REG20, BLWC_ENA, 0);

	XRXTX_IOWRITE_BITS(priv, RXTX_REG53, RX_PLLSELECT, RXTX_10G_PLL);
	XRXTX_IOWRITE_BITS(priv, RXTX_REG53, TX_PLLSELECT, RXTX_10G_PLL);
	XRXTX_IOWRITE_BITS(priv, RXTX_REG53, PI_SPD_SEL_CDR, RXTX_10G_CDR);

	XRXTX_IOWRITE_BITS(priv, RXTX_REG114, PQ_REG, RXTX_10G_PQ);

	amd_xgbe_phy_serdes_complete_ratechange(phydev);

	spin_unlock(&cmu_lock);

	return 0;
}

static int amd_xgbe_phy_gmii_2500_mode(struct phy_device *phydev)
{
	struct amd_xgbe_phy_priv *priv = phydev->priv;
	int ret;

	/* Disable KR training */
	ret = amd_xgbe_an_disable_kr_training(phydev);
	if (ret < 0)
		return ret;

	/* Set PCS to KX/1G speed */
	ret = phy_read_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL2);
	if (ret < 0)
		return ret;

	ret &= ~MDIO_PCS_CTRL2_TYPE;
	ret |= MDIO_PCS_CTRL2_10GBX;
	phy_write_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL2, ret);

	ret = phy_read_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL1);
	if (ret < 0)
		return ret;

	ret &= ~MDIO_CTRL1_SPEEDSEL;
	ret |= MDIO_CTRL1_SPEED1G;
	phy_write_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL1, ret);

	ret = amd_xgbe_phy_pcs_power_cycle(phydev);
	if (ret < 0)
		return ret;

	/* Set SerDes to 2.5G speed */
	spin_lock(&cmu_lock);

	amd_xgbe_phy_serdes_start_ratechange(phydev);

	XRXTX_IOWRITE_BITS(priv, RXTX_REG3, TX_DATA_RATE, RXTX_HALF_RATE);
	XRXTX_IOWRITE_BITS(priv, RXTX_REG3, TX_WORD_MODE, RXTX_10BIT_WORD);

	XRXTX_IOWRITE_BITS(priv, RXTX_REG5, TXAMP_CNTL, RXTX_1G_TX_AMP);

	XRXTX_IOWRITE_BITS(priv, RXTX_REG6, RX_DATA_RATE, RXTX_HALF_RATE);
	XRXTX_IOWRITE_BITS(priv, RXTX_REG6, RX_WORD_MODE, RXTX_10BIT_WORD);

	XRXTX_IOWRITE_BITS(priv, RXTX_REG20, BLWC_ENA, 1);

	XRXTX_IOWRITE_BITS(priv, RXTX_REG53, RX_PLLSELECT, RXTX_1G_PLL);
	XRXTX_IOWRITE_BITS(priv, RXTX_REG53, TX_PLLSELECT, RXTX_1G_PLL);
	XRXTX_IOWRITE_BITS(priv, RXTX_REG53, PI_SPD_SEL_CDR, RXTX_1G_CDR);

	XRXTX_IOWRITE_BITS(priv, RXTX_REG114, PQ_REG, RXTX_1G_PQ);

	amd_xgbe_phy_serdes_complete_ratechange(phydev);

	spin_unlock(&cmu_lock);

	return 0;
}

static int amd_xgbe_phy_gmii_mode(struct phy_device *phydev)
{
	struct amd_xgbe_phy_priv *priv = phydev->priv;
	int ret;

	/* Disable KR training */
	ret = amd_xgbe_an_disable_kr_training(phydev);
	if (ret < 0)
		return ret;

	/* Set PCS to KX/1G speed */
	ret = phy_read_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL2);
	if (ret < 0)
		return ret;

	ret &= ~MDIO_PCS_CTRL2_TYPE;
	ret |= MDIO_PCS_CTRL2_10GBX;
	phy_write_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL2, ret);

	ret = phy_read_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL1);
	if (ret < 0)
		return ret;

	ret &= ~MDIO_CTRL1_SPEEDSEL;
	ret |= MDIO_CTRL1_SPEED1G;
	phy_write_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL1, ret);

	ret = amd_xgbe_phy_pcs_power_cycle(phydev);
	if (ret < 0)
		return ret;

	/* Set SerDes to 1G speed */
	spin_lock(&cmu_lock);

	amd_xgbe_phy_serdes_start_ratechange(phydev);

	XRXTX_IOWRITE_BITS(priv, RXTX_REG3, TX_DATA_RATE, RXTX_FIFTH_RATE);
	XRXTX_IOWRITE_BITS(priv, RXTX_REG3, TX_WORD_MODE, RXTX_10BIT_WORD);

	XRXTX_IOWRITE_BITS(priv, RXTX_REG5, TXAMP_CNTL, RXTX_1G_TX_AMP);

	XRXTX_IOWRITE_BITS(priv, RXTX_REG6, RX_DATA_RATE, RXTX_FIFTH_RATE);
	XRXTX_IOWRITE_BITS(priv, RXTX_REG6, RX_WORD_MODE, RXTX_10BIT_WORD);

	XRXTX_IOWRITE_BITS(priv, RXTX_REG20, BLWC_ENA, 1);

	XRXTX_IOWRITE_BITS(priv, RXTX_REG53, RX_PLLSELECT, RXTX_1G_PLL);
	XRXTX_IOWRITE_BITS(priv, RXTX_REG53, TX_PLLSELECT, RXTX_1G_PLL);
	XRXTX_IOWRITE_BITS(priv, RXTX_REG53, PI_SPD_SEL_CDR, RXTX_1G_CDR);

	XRXTX_IOWRITE_BITS(priv, RXTX_REG114, PQ_REG, RXTX_1G_PQ);

	amd_xgbe_phy_serdes_complete_ratechange(phydev);

	spin_unlock(&cmu_lock);
	return 0;
}

static int amd_xgbe_phy_cur_mode(struct phy_device *phydev,
				 enum amd_xgbe_phy_mode *mode)
{
	int ret;

	ret = phy_read_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL2);
	if (ret < 0)
		return ret;

	if ((ret & MDIO_PCS_CTRL2_TYPE) == MDIO_PCS_CTRL2_10GBR)
		*mode = AMD_XGBE_MODE_KR;
	else
		*mode = AMD_XGBE_MODE_KX;

	return 0;
}

static bool amd_xgbe_phy_in_kr_mode(struct phy_device *phydev)
{
	enum amd_xgbe_phy_mode mode;

	if (amd_xgbe_phy_cur_mode(phydev, &mode))
		return false;

	return (mode == AMD_XGBE_MODE_KR);
}

static int amd_xgbe_phy_switch_mode(struct phy_device *phydev)
{
	struct amd_xgbe_phy_priv *priv = phydev->priv;
	int ret;

	/* If we are in KR switch to KX, and vice-versa */
	if (amd_xgbe_phy_in_kr_mode(phydev)) {
		if (priv->speed_set == AMD_XGBE_PHY_SPEEDSET_1000_10000)
			ret = amd_xgbe_phy_gmii_mode(phydev);
		else
			ret = amd_xgbe_phy_gmii_2500_mode(phydev);
	} else {
		ret = amd_xgbe_phy_xgmii_mode(phydev);
	}

	return ret;
}

static int amd_xgbe_phy_set_mode(struct phy_device *phydev,
				 enum amd_xgbe_phy_mode mode)
{
	enum amd_xgbe_phy_mode cur_mode;
	int ret;

	ret = amd_xgbe_phy_cur_mode(phydev, &cur_mode);
	if (ret)
		return ret;

	if (mode != cur_mode)
		ret = amd_xgbe_phy_switch_mode(phydev);

	return ret;
}

static enum amd_xgbe_phy_an amd_xgbe_an_tx_training(struct phy_device *phydev,
						    enum amd_xgbe_phy_rx *state)
{
	int ad_reg, lp_reg, ret;

	*state = AMD_XGBE_RX_COMPLETE;

	/* If we're not in KR mode then we're done */
	if (!amd_xgbe_phy_in_kr_mode(phydev))
		return AMD_XGBE_AN_EVENT;

	/* Enable/Disable FEC */
	ad_reg = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 2);
	if (ad_reg < 0)
		return AMD_XGBE_AN_ERROR;

	lp_reg = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_LPA + 2);
	if (lp_reg < 0)
		return AMD_XGBE_AN_ERROR;

	ret = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_FEC_CTRL);
	if (ret < 0)
		return AMD_XGBE_AN_ERROR;

	if ((ad_reg & 0xc000) && (lp_reg & 0xc000))
		ret |= 0x01;
	else
		ret &= ~0x01;

	phy_write_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_FEC_CTRL, ret);

	/* Start KR training */
	ret = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_PMD_CTRL);
	if (ret < 0)
		return AMD_XGBE_AN_ERROR;

	ret |= 0x01;
	phy_write_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_PMD_CTRL, ret);

	return AMD_XGBE_AN_EVENT;
}

static enum amd_xgbe_phy_an amd_xgbe_an_tx_xnp(struct phy_device *phydev,
					       enum amd_xgbe_phy_rx *state)
{
	u16 msg;

	*state = AMD_XGBE_RX_XNP;

	msg = XNP_MCF_NULL_MESSAGE;
	msg |= XNP_MP_FORMATTED;

	phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_XNP + 2, 0);
	phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_XNP + 1, 0);
	phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_XNP, msg);

	return AMD_XGBE_AN_EVENT;
}

static enum amd_xgbe_phy_an amd_xgbe_an_rx_bpa(struct phy_device *phydev,
					       enum amd_xgbe_phy_rx *state)
{
	unsigned int link_support;
	int ret, ad_reg, lp_reg;

	/* Read Base Ability register 2 first */
	ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_LPA + 1);
	if (ret < 0)
		return AMD_XGBE_AN_ERROR;

	/* Check for a supported mode, otherwise restart in a different one */
	link_support = amd_xgbe_phy_in_kr_mode(phydev) ? 0x80 : 0x20;
	if (!(ret & link_support))
		return AMD_XGBE_AN_INCOMPAT_LINK;

	/* Check Extended Next Page support */
	ad_reg = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_ADVERTISE);
	if (ad_reg < 0)
		return AMD_XGBE_AN_ERROR;

	lp_reg = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_LPA);
	if (lp_reg < 0)
		return AMD_XGBE_AN_ERROR;

	return ((ad_reg & XNP_NP_EXCHANGE) || (lp_reg & XNP_NP_EXCHANGE)) ?
	       amd_xgbe_an_tx_xnp(phydev, state) :
	       amd_xgbe_an_tx_training(phydev, state);
}

static enum amd_xgbe_phy_an amd_xgbe_an_rx_xnp(struct phy_device *phydev,
					       enum amd_xgbe_phy_rx *state)
{
	int ad_reg, lp_reg;

	/* Check Extended Next Page support */
	ad_reg = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_ADVERTISE);
	if (ad_reg < 0)
		return AMD_XGBE_AN_ERROR;

	lp_reg = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_LPA);
	if (lp_reg < 0)
		return AMD_XGBE_AN_ERROR;

	return ((ad_reg & XNP_NP_EXCHANGE) || (lp_reg & XNP_NP_EXCHANGE)) ?
	       amd_xgbe_an_tx_xnp(phydev, state) :
	       amd_xgbe_an_tx_training(phydev, state);
}

static enum amd_xgbe_phy_an amd_xgbe_an_start(struct phy_device *phydev)
{
	struct amd_xgbe_phy_priv *priv = phydev->priv;
	int ret;

	/* Be sure we aren't looping trying to negotiate */
	if (amd_xgbe_phy_in_kr_mode(phydev)) {
		if (priv->kr_state != AMD_XGBE_RX_READY)
			return AMD_XGBE_AN_NO_LINK;
		priv->kr_state = AMD_XGBE_RX_BPA;
	} else {
		if (priv->kx_state != AMD_XGBE_RX_READY)
			return AMD_XGBE_AN_NO_LINK;
		priv->kx_state = AMD_XGBE_RX_BPA;
	}

	/* Set up Advertisement register 3 first */
	ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 2);
	if (ret < 0)
		return AMD_XGBE_AN_ERROR;

	if (phydev->supported & SUPPORTED_10000baseR_FEC)
		ret |= 0xc000;
	else
		ret &= ~0xc000;

	phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 2, ret);

	/* Set up Advertisement register 2 next */
	ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 1);
	if (ret < 0)
		return AMD_XGBE_AN_ERROR;

	if (phydev->supported & SUPPORTED_10000baseKR_Full)
		ret |= 0x80;
	else
		ret &= ~0x80;

	if ((phydev->supported & SUPPORTED_1000baseKX_Full) ||
	    (phydev->supported & SUPPORTED_2500baseX_Full))
		ret |= 0x20;
	else
		ret &= ~0x20;

	phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_ADVERTISE + 1, ret);

	/* Set up Advertisement register 1 last */
	ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_ADVERTISE);
	if (ret < 0)
		return AMD_XGBE_AN_ERROR;

	if (phydev->supported & SUPPORTED_Pause)
		ret |= 0x400;
	else
		ret &= ~0x400;

	if (phydev->supported & SUPPORTED_Asym_Pause)
		ret |= 0x800;
	else
		ret &= ~0x800;

	/* We don't intend to perform XNP */
	ret &= ~XNP_NP_EXCHANGE;

	phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_ADVERTISE, ret);

	/* Enable and start auto-negotiation */
	phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_INT, 0);

	ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_KR_CTRL);
	if (ret < 0)
		return AMD_XGBE_AN_ERROR;

	ret |= MDIO_KR_CTRL_PDETECT;
	phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_KR_CTRL, ret);

	ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_CTRL1);
	if (ret < 0)
		return AMD_XGBE_AN_ERROR;

	ret |= MDIO_AN_CTRL1_ENABLE;
	ret |= MDIO_AN_CTRL1_RESTART;
	phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_CTRL1, ret);

	return AMD_XGBE_AN_EVENT;
}

static enum amd_xgbe_phy_an amd_xgbe_an_event(struct phy_device *phydev)
{
	enum amd_xgbe_phy_an new_state;
	int ret;

	ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_INT);
	if (ret < 0)
		return AMD_XGBE_AN_ERROR;

	new_state = AMD_XGBE_AN_EVENT;
	if (ret & XGBE_AN_PG_RCV)
		new_state = AMD_XGBE_AN_PAGE_RECEIVED;
	else if (ret & XGBE_AN_INC_LINK)
		new_state = AMD_XGBE_AN_INCOMPAT_LINK;
	else if (ret & XGBE_AN_INT_CMPLT)
		new_state = AMD_XGBE_AN_COMPLETE;

	if (new_state != AMD_XGBE_AN_EVENT)
		phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_INT, 0);

	return new_state;
}

static enum amd_xgbe_phy_an amd_xgbe_an_page_received(struct phy_device *phydev)
{
	struct amd_xgbe_phy_priv *priv = phydev->priv;
	enum amd_xgbe_phy_rx *state;
	int ret;

	state = amd_xgbe_phy_in_kr_mode(phydev) ? &priv->kr_state
						: &priv->kx_state;

	switch (*state) {
	case AMD_XGBE_RX_BPA:
		ret = amd_xgbe_an_rx_bpa(phydev, state);
		break;

	case AMD_XGBE_RX_XNP:
		ret = amd_xgbe_an_rx_xnp(phydev, state);
		break;

	default:
		ret = AMD_XGBE_AN_ERROR;
	}

	return ret;
}

static enum amd_xgbe_phy_an amd_xgbe_an_incompat_link(struct phy_device *phydev)
{
	int ret;

	ret = amd_xgbe_phy_switch_mode(phydev);
	if (ret)
		return AMD_XGBE_AN_ERROR;

	return AMD_XGBE_AN_START;
}

static void amd_xgbe_an_state_machine(struct work_struct *work)
{
	struct amd_xgbe_phy_priv *priv = container_of(work,
						      struct amd_xgbe_phy_priv,
						      an_work);
	struct phy_device *phydev = priv->phydev;
	enum amd_xgbe_phy_an cur_state;
	int sleep;
	unsigned int an_supported = 0;

	/* Start in KX mode */
	if (amd_xgbe_phy_set_mode(phydev, AMD_XGBE_MODE_KX))
		priv->an_state = AMD_XGBE_AN_ERROR;

	while (1) {
		mutex_lock(&priv->an_mutex);

		cur_state = priv->an_state;

		switch (priv->an_state) {
		case AMD_XGBE_AN_START:
			an_supported = 0;
			priv->parallel_detect = 0;
			priv->an_state = amd_xgbe_an_start(phydev);
			break;

		case AMD_XGBE_AN_EVENT:
			priv->an_state = amd_xgbe_an_event(phydev);
			break;

		case AMD_XGBE_AN_PAGE_RECEIVED:
			priv->an_state = amd_xgbe_an_page_received(phydev);
			an_supported++;
			break;

		case AMD_XGBE_AN_INCOMPAT_LINK:
			priv->an_state = amd_xgbe_an_incompat_link(phydev);
			break;

		case AMD_XGBE_AN_COMPLETE:
			priv->parallel_detect = an_supported ? 0 : 1;
			netdev_info(phydev->attached_dev, "%s successful\n",
				    an_supported ? "Auto negotiation"
						 : "Parallel detection");
			/* fall through */

		case AMD_XGBE_AN_NO_LINK:
		case AMD_XGBE_AN_EXIT:
			goto exit_unlock;

		default:
			priv->an_state = AMD_XGBE_AN_ERROR;
		}

		if (priv->an_state == AMD_XGBE_AN_ERROR) {
			netdev_err(phydev->attached_dev,
				   "error during auto-negotiation, state=%u\n",
				   cur_state);
			goto exit_unlock;
		}

		sleep = (priv->an_state == AMD_XGBE_AN_EVENT) ? 1 : 0;

		mutex_unlock(&priv->an_mutex);

		if (sleep)
			usleep_range(20, 50);
	}

exit_unlock:
	priv->an_result = priv->an_state;
	priv->an_state = AMD_XGBE_AN_READY;

	mutex_unlock(&priv->an_mutex);
}

static int amd_xgbe_phy_soft_reset(struct phy_device *phydev)
{
	int count, ret;

	ret = phy_read_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL1);
	if (ret < 0)
		return ret;

	ret |= MDIO_CTRL1_RESET;
	phy_write_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL1, ret);

	count = 50;
	do {
		msleep(20);
		ret = phy_read_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL1);
		if (ret < 0)
			return ret;
	} while ((ret & MDIO_CTRL1_RESET) && --count);

	if (ret & MDIO_CTRL1_RESET)
		return -ETIMEDOUT;

	/* Make sure the XPCS and SerDes are in compatible states */
	return amd_xgbe_phy_xgmii_mode(phydev);
}

static int amd_xgbe_phy_config_init(struct phy_device *phydev)
{
	struct amd_xgbe_phy_priv *priv = phydev->priv;

	/* Initialize supported features */
	phydev->supported = SUPPORTED_Autoneg;
	phydev->supported |= SUPPORTED_Pause | SUPPORTED_Asym_Pause;
	phydev->supported |= SUPPORTED_Backplane;
	phydev->supported |= SUPPORTED_10000baseKR_Full |
			     SUPPORTED_10000baseR_FEC;
	switch (priv->speed_set) {
	case AMD_XGBE_PHY_SPEEDSET_1000_10000:
		phydev->supported |= SUPPORTED_1000baseKX_Full;
		break;
	case AMD_XGBE_PHY_SPEEDSET_2500_10000:
		phydev->supported |= SUPPORTED_2500baseX_Full;
		break;
	}
	phydev->advertising = phydev->supported;

	/* Turn off and clear interrupts */
	phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_INTMASK, 0);
	phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_INT, 0);

	return 0;
}

static int amd_xgbe_phy_setup_forced(struct phy_device *phydev)
{
	int ret;

	/* Disable auto-negotiation */
	ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_CTRL1);
	if (ret < 0)
		return ret;

	ret &= ~MDIO_AN_CTRL1_ENABLE;
	phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_CTRL1, ret);

	/* Validate/Set specified speed */
	switch (phydev->speed) {
	case SPEED_10000:
		ret = amd_xgbe_phy_xgmii_mode(phydev);
		break;

	case SPEED_2500:
		ret = amd_xgbe_phy_gmii_2500_mode(phydev);
		break;

	case SPEED_1000:
		ret = amd_xgbe_phy_gmii_mode(phydev);
		break;

	default:
		ret = -EINVAL;
	}

	if (ret < 0)
		return ret;

	/* Validate duplex mode */
	if (phydev->duplex != DUPLEX_FULL)
		return -EINVAL;

	phydev->pause = 0;
	phydev->asym_pause = 0;

	return 0;
}

static int amd_xgbe_phy_config_aneg(struct phy_device *phydev)
{
	struct amd_xgbe_phy_priv *priv = phydev->priv;
	u32 mmd_mask = phydev->c45_ids.devices_in_package;

	if (phydev->autoneg != AUTONEG_ENABLE)
		return amd_xgbe_phy_setup_forced(phydev);

	/* Make sure we have the AN MMD present */
	if (!(mmd_mask & MDIO_DEVS_AN))
		return -EINVAL;

	/* Start/Restart the auto-negotiation state machine */
	mutex_lock(&priv->an_mutex);
	priv->an_result = AMD_XGBE_AN_READY;
	priv->an_state = AMD_XGBE_AN_START;
	priv->kr_state = AMD_XGBE_RX_READY;
	priv->kx_state = AMD_XGBE_RX_READY;
	mutex_unlock(&priv->an_mutex);

	queue_work(priv->an_workqueue, &priv->an_work);

	return 0;
}

static int amd_xgbe_phy_aneg_done(struct phy_device *phydev)
{
	struct amd_xgbe_phy_priv *priv = phydev->priv;
	enum amd_xgbe_phy_an state;

	mutex_lock(&priv->an_mutex);
	state = priv->an_result;
	mutex_unlock(&priv->an_mutex);

	return (state == AMD_XGBE_AN_COMPLETE);
}

static int amd_xgbe_phy_update_link(struct phy_device *phydev)
{
	struct amd_xgbe_phy_priv *priv = phydev->priv;
	enum amd_xgbe_phy_an state;
	unsigned int check_again, autoneg;
	int ret;

	/* If we're doing auto-negotiation don't report link down */
	mutex_lock(&priv->an_mutex);
	state = priv->an_state;
	mutex_unlock(&priv->an_mutex);

	if (state != AMD_XGBE_AN_READY) {
		phydev->link = 1;
		return 0;
	}

	/* Since the device can be in the wrong mode when a link is
	 * (re-)established (cable connected after the interface is
	 * up, etc.), the link status may report no link. If there
	 * is no link, try switching modes and checking the status
	 * again if auto negotiation is enabled.
	 */
	check_again = (phydev->autoneg == AUTONEG_ENABLE) ? 1 : 0;
again:
	/* Link status is latched low, so read once to clear
	 * and then read again to get current state
	 */
	ret = phy_read_mmd(phydev, MDIO_MMD_PCS, MDIO_STAT1);
	if (ret < 0)
		return ret;

	ret = phy_read_mmd(phydev, MDIO_MMD_PCS, MDIO_STAT1);
	if (ret < 0)
		return ret;

	phydev->link = (ret & MDIO_STAT1_LSTATUS) ? 1 : 0;

	if (!phydev->link) {
		if (check_again) {
			ret = amd_xgbe_phy_switch_mode(phydev);
			if (ret < 0)
				return ret;
			check_again = 0;
			goto again;
		}
	}

	autoneg = (phydev->link && !priv->link) ? 1 : 0;
	priv->link = phydev->link;
	if (autoneg) {
		/* Link is (back) up, re-start auto-negotiation */
		ret = amd_xgbe_phy_config_aneg(phydev);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int amd_xgbe_phy_read_status(struct phy_device *phydev)
{
	struct amd_xgbe_phy_priv *priv = phydev->priv;
	u32 mmd_mask = phydev->c45_ids.devices_in_package;
	int ret, ad_ret, lp_ret;

	ret = amd_xgbe_phy_update_link(phydev);
	if (ret)
		return ret;

	if ((phydev->autoneg == AUTONEG_ENABLE) &&
	    !priv->parallel_detect) {
		if (!(mmd_mask & MDIO_DEVS_AN))
			return -EINVAL;

		if (!amd_xgbe_phy_aneg_done(phydev))
			return 0;

		/* Compare Advertisement and Link Partner register 1 */
		ad_ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_ADVERTISE);
		if (ad_ret < 0)
			return ad_ret;
		lp_ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_LPA);
		if (lp_ret < 0)
			return lp_ret;

		ad_ret &= lp_ret;
		phydev->pause = (ad_ret & 0x400) ? 1 : 0;
		phydev->asym_pause = (ad_ret & 0x800) ? 1 : 0;

		/* Compare Advertisement and Link Partner register 2 */
		ad_ret = phy_read_mmd(phydev, MDIO_MMD_AN,
				      MDIO_AN_ADVERTISE + 1);
		if (ad_ret < 0)
			return ad_ret;
		lp_ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_LPA + 1);
		if (lp_ret < 0)
			return lp_ret;

		ad_ret &= lp_ret;
		if (ad_ret & 0x80) {
			phydev->speed = SPEED_10000;
			ret = amd_xgbe_phy_set_mode(phydev, AMD_XGBE_MODE_KR);
			if (ret)
				return ret;
		} else {
			switch (priv->speed_set) {
			case AMD_XGBE_PHY_SPEEDSET_1000_10000:
				phydev->speed = SPEED_1000;
				break;

			case AMD_XGBE_PHY_SPEEDSET_2500_10000:
				phydev->speed = SPEED_2500;
				break;
			}

			ret = amd_xgbe_phy_set_mode(phydev, AMD_XGBE_MODE_KX);
			if (ret)
				return ret;
		}

		phydev->duplex = DUPLEX_FULL;
	} else {
		if (amd_xgbe_phy_in_kr_mode(phydev)) {
			phydev->speed = SPEED_10000;
		} else {
			switch (priv->speed_set) {
			case AMD_XGBE_PHY_SPEEDSET_1000_10000:
				phydev->speed = SPEED_1000;
				break;

			case AMD_XGBE_PHY_SPEEDSET_2500_10000:
				phydev->speed = SPEED_2500;
				break;
			}
		}
		phydev->duplex = DUPLEX_FULL;
		phydev->pause = 0;
		phydev->asym_pause = 0;
	}

	return 0;
}

static int amd_xgbe_phy_suspend(struct phy_device *phydev)
{
	int ret;

	mutex_lock(&phydev->lock);

	ret = phy_read_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL1);
	if (ret < 0)
		goto unlock;

	ret |= MDIO_CTRL1_LPOWER;
	phy_write_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL1, ret);

	ret = 0;

unlock:
	mutex_unlock(&phydev->lock);

	return ret;
}

static int amd_xgbe_phy_resume(struct phy_device *phydev)
{
	int ret;

	mutex_lock(&phydev->lock);

	ret = phy_read_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL1);
	if (ret < 0)
		goto unlock;

	ret &= ~MDIO_CTRL1_LPOWER;
	phy_write_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL1, ret);

	ret = 0;

unlock:
	mutex_unlock(&phydev->lock);

	return ret;
}

static int amd_xgbe_phy_map_resources(struct amd_xgbe_phy_priv *priv,
				      struct platform_device *phy_pdev,
				      unsigned int phy_resnum)
{
	struct device *dev = priv->dev;
	int ret;

	/* Get the device mmio areas */
	priv->rxtx_res = platform_get_resource(phy_pdev, IORESOURCE_MEM,
					       phy_resnum++);
	priv->rxtx_regs = devm_ioremap_resource(dev, priv->rxtx_res);
	if (IS_ERR(priv->rxtx_regs)) {
		dev_err(dev, "rxtx ioremap failed\n");
		return PTR_ERR(priv->rxtx_regs);
	}

	/* All xgbe phy devices share the CMU registers so retrieve
	 * the resource and do the ioremap directly rather than
	 * the devm_ioremap_resource call
	 */
	priv->cmu_res = platform_get_resource(phy_pdev, IORESOURCE_MEM,
					      phy_resnum++);
	if (!priv->cmu_res) {
		dev_err(dev, "cmu invalid resource\n");
		ret = -EINVAL;
		goto err_rxtx;
	}
	priv->cmu_regs = devm_ioremap_nocache(dev, priv->cmu_res->start,
					      resource_size(priv->cmu_res));
	if (!priv->cmu_regs) {
		dev_err(dev, "cmu ioremap failed\n");
		ret = -ENOMEM;
		goto err_rxtx;
	}

	return 0;

err_rxtx:
	devm_iounmap(dev, priv->rxtx_regs);
	devm_release_mem_region(dev, priv->rxtx_res->start,
				resource_size(priv->rxtx_res));

	return ret;
}

static void amd_xgbe_phy_unmap_resources(struct amd_xgbe_phy_priv *priv)
{
	struct device *dev = priv->dev;

	devm_iounmap(dev, priv->cmu_regs);

	devm_iounmap(dev, priv->rxtx_regs);
	devm_release_mem_region(dev, priv->rxtx_res->start,
				resource_size(priv->rxtx_res));
}

#ifdef CONFIG_ACPI
static int amd_xgbe_phy_acpi_support(struct amd_xgbe_phy_priv *priv)
{
	struct platform_device *phy_pdev = priv->pdev;
	struct acpi_device *adev = priv->adev;
	struct device *dev = priv->dev;
	const union acpi_object *property;
	int ret;

	/* Map the memory resources */
	ret = amd_xgbe_phy_map_resources(priv, phy_pdev, 2);
	if (ret)
		return ret;

	/* Get the device serdes channel property */
	ret = acpi_dev_get_property(adev, XGBE_PHY_CHANNEL_PROPERTY,
				    ACPI_TYPE_INTEGER, &property);
	if (ret) {
		dev_err(dev, "unable to obtain %s acpi property\n",
			XGBE_PHY_CHANNEL_PROPERTY);
		goto err_resources;
	}
	priv->serdes_channel = property->integer.value;

	/* Get the device speed set property */
	ret = acpi_dev_get_property(adev, XGBE_PHY_SPEEDSET_PROPERTY,
				    ACPI_TYPE_INTEGER, &property);
	if (ret) {
		dev_err(dev, "unable to obtain %s acpi property\n",
			XGBE_PHY_SPEEDSET_PROPERTY);
		goto err_resources;
	}
	priv->speed_set = property->integer.value;

	return 0;

err_resources:
	amd_xgbe_phy_unmap_resources(priv);

	return ret;
}
#else	/* CONFIG_ACPI */
static int amd_xgbe_phy_acpi_support(struct amd_xgbe_phy_priv *priv)
{
	return -EINVAL;
}
#endif	/* CONFIG_ACPI */

#ifdef CONFIG_OF
static int amd_xgbe_phy_of_support(struct amd_xgbe_phy_priv *priv)
{
	struct platform_device *phy_pdev;
	struct device_node *bus_node;
	struct device_node *phy_node;
	struct device *dev = priv->dev;
	const __be32 *property;
	int ret;

	bus_node = priv->dev->of_node;
	phy_node = of_parse_phandle(bus_node, "phy-handle", 0);
	if (!phy_node) {
		dev_err(dev, "unable to parse phy-handle\n");
		return -EINVAL;
	}

	phy_pdev = of_find_device_by_node(phy_node);
	if (!phy_pdev) {
		dev_err(dev, "unable to obtain phy device\n");
		ret = -EINVAL;
		goto err_put;
	}

	/* Map the memory resources */
	ret = amd_xgbe_phy_map_resources(priv, phy_pdev, 0);
	if (ret)
		goto err_put;

	/* Get the device serdes channel property */
	property = of_get_property(phy_node, XGBE_PHY_CHANNEL_PROPERTY, NULL);
	if (!property) {
		dev_err(dev, "unable to obtain %s property\n",
			XGBE_PHY_CHANNEL_PROPERTY);
		ret = -EINVAL;
		goto err_resources;
	}
	priv->serdes_channel = be32_to_cpu(*property);

	/* Get the device speed set property */
	property = of_get_property(phy_node, XGBE_PHY_SPEEDSET_PROPERTY, NULL);
	if (property)
		priv->speed_set = be32_to_cpu(*property);

	of_node_put(phy_node);

	return 0;

err_resources:
	amd_xgbe_phy_unmap_resources(priv);

err_put:
	of_node_put(phy_node);

	return ret;
}
#else	/* CONFIG_OF */
static int amd_xgbe_phy_of_support(struct amd_xgbe_phy_priv *priv)
{
	return -EINVAL;
}
#endif	/* CONFIG_OF */

static int amd_xgbe_phy_probe(struct phy_device *phydev)
{
	struct amd_xgbe_phy_priv *priv;
	struct device *dev;
	char *wq_name;
	int ret;

	if (!phydev->bus || !phydev->bus->parent)
		return -EINVAL;

	dev = phydev->bus->parent;

	wq_name = kasprintf(GFP_KERNEL, "%s-amd-xgbe-phy", phydev->bus->name);
	if (!wq_name)
		return -ENOMEM;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto err_name;
	}

	priv->pdev = to_platform_device(dev);
	priv->adev = ACPI_COMPANION(dev);
	priv->dev = dev;
	priv->phydev = phydev;

	if (priv->adev && !acpi_disabled)
		ret = amd_xgbe_phy_acpi_support(priv);
	else
		ret = amd_xgbe_phy_of_support(priv);
	if (ret)
		goto err_priv;

	switch (priv->speed_set) {
	case AMD_XGBE_PHY_SPEEDSET_1000_10000:
	case AMD_XGBE_PHY_SPEEDSET_2500_10000:
		break;
	default:
		dev_err(dev, "invalid amd,speed-set property\n");
		ret = -EINVAL;
		goto err_resources;
	}

	priv->link = 1;

	mutex_init(&priv->an_mutex);
	INIT_WORK(&priv->an_work, amd_xgbe_an_state_machine);
	priv->an_workqueue = create_singlethread_workqueue(wq_name);
	if (!priv->an_workqueue) {
		ret = -ENOMEM;
		goto err_resources;
	}

	phydev->priv = priv;

	kfree(wq_name);

	return 0;

err_resources:
	amd_xgbe_phy_unmap_resources(priv);

err_priv:
	devm_kfree(dev, priv);

err_name:
	kfree(wq_name);

	return ret;
}

static void amd_xgbe_phy_remove(struct phy_device *phydev)
{
	struct amd_xgbe_phy_priv *priv = phydev->priv;
	struct device *dev = priv->dev;

	/* Stop any in process auto-negotiation */
	mutex_lock(&priv->an_mutex);
	priv->an_state = AMD_XGBE_AN_EXIT;
	mutex_unlock(&priv->an_mutex);

	flush_workqueue(priv->an_workqueue);
	destroy_workqueue(priv->an_workqueue);

	amd_xgbe_phy_unmap_resources(priv);

	devm_kfree(dev, priv);
}

static int amd_xgbe_match_phy_device(struct phy_device *phydev)
{
	return phydev->c45_ids.device_ids[MDIO_MMD_PCS] == XGBE_PHY_ID;
}

static struct phy_driver amd_xgbe_phy_driver[] = {
	{
		.phy_id			= XGBE_PHY_ID,
		.phy_id_mask		= XGBE_PHY_MASK,
		.name			= "AMD XGBE PHY",
		.features		= 0,
		.probe			= amd_xgbe_phy_probe,
		.remove			= amd_xgbe_phy_remove,
		.soft_reset		= amd_xgbe_phy_soft_reset,
		.config_init		= amd_xgbe_phy_config_init,
		.suspend		= amd_xgbe_phy_suspend,
		.resume			= amd_xgbe_phy_resume,
		.config_aneg		= amd_xgbe_phy_config_aneg,
		.aneg_done		= amd_xgbe_phy_aneg_done,
		.read_status		= amd_xgbe_phy_read_status,
		.match_phy_device	= amd_xgbe_match_phy_device,
		.driver			= {
			.owner = THIS_MODULE,
		},
	},
};

static int __init amd_xgbe_phy_init(void)
{
	return phy_drivers_register(amd_xgbe_phy_driver,
				    ARRAY_SIZE(amd_xgbe_phy_driver));
}

static void __exit amd_xgbe_phy_exit(void)
{
	phy_drivers_unregister(amd_xgbe_phy_driver,
			       ARRAY_SIZE(amd_xgbe_phy_driver));
}

module_init(amd_xgbe_phy_init);
module_exit(amd_xgbe_phy_exit);

static struct mdio_device_id __maybe_unused amd_xgbe_phy_ids[] = {
	{ XGBE_PHY_ID, XGBE_PHY_MASK },
	{ }
};
MODULE_DEVICE_TABLE(mdio, amd_xgbe_phy_ids);
