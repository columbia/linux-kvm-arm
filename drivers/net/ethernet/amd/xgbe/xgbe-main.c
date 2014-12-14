/*
 * AMD 10Gb Ethernet driver
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
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *     The Synopsys DWC ETHER XGMAC Software Driver and documentation
 *     (hereinafter "Software") is an unsupported proprietary work of Synopsys,
 *     Inc. unless otherwise expressly agreed to in writing between Synopsys
 *     and you.
 *
 *     The Software IS NOT an item of Licensed Software or Licensed Product
 *     under any End User Software License Agreement or Agreement for Licensed
 *     Product with Synopsys or any supplement thereto.  Permission is hereby
 *     granted, free of charge, to any person obtaining a copy of this software
 *     annotated with this license and the Software, to deal in the Software
 *     without restriction, including without limitation the rights to use,
 *     copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 *     of the Software, and to permit persons to whom the Software is furnished
 *     to do so, subject to the following conditions:
 *
 *     The above copyright notice and this permission notice shall be included
 *     in all copies or substantial portions of the Software.
 *
 *     THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS"
 *     BASIS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *     TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *     PARTICULAR PURPOSE ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS
 *     BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *     CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *     SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *     INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *     CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *     ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *     THE POSSIBILITY OF SUCH DAMAGE.
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *     The Synopsys DWC ETHER XGMAC Software Driver and documentation
 *     (hereinafter "Software") is an unsupported proprietary work of Synopsys,
 *     Inc. unless otherwise expressly agreed to in writing between Synopsys
 *     and you.
 *
 *     The Software IS NOT an item of Licensed Software or Licensed Product
 *     under any End User Software License Agreement or Agreement for Licensed
 *     Product with Synopsys or any supplement thereto.  Permission is hereby
 *     granted, free of charge, to any person obtaining a copy of this software
 *     annotated with this license and the Software, to deal in the Software
 *     without restriction, including without limitation the rights to use,
 *     copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 *     of the Software, and to permit persons to whom the Software is furnished
 *     to do so, subject to the following conditions:
 *
 *     The above copyright notice and this permission notice shall be included
 *     in all copies or substantial portions of the Software.
 *
 *     THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS"
 *     BASIS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *     TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *     PARTICULAR PURPOSE ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS
 *     BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *     CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *     SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *     INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *     CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *     ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *     THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/clk.h>
#include <linux/acpi.h>

#include "xgbe.h"
#include "xgbe-common.h"

MODULE_AUTHOR("Tom Lendacky <thomas.lendacky@amd.com>");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(XGBE_DRV_VERSION);
MODULE_DESCRIPTION(XGBE_DRV_DESC);

unsigned int speed = 0;
module_param(speed, uint, 0444);
MODULE_PARM_DESC(speed, " Select operating speed (1=1GbE, 2=2.5GbE, 10=10GbE, any other value implies auto-negotiation");

static void xgbe_default_config(struct xgbe_prv_data *pdata)
{
	DBGPR("-->xgbe_default_config\n");

	pdata->pblx8 = DMA_PBL_X8_ENABLE;
	pdata->tx_sf_mode = MTL_TSF_ENABLE;
	pdata->tx_threshold = MTL_TX_THRESHOLD_64;
	pdata->tx_pbl = DMA_PBL_16;
	pdata->tx_osp_mode = DMA_OSP_ENABLE;
	pdata->rx_sf_mode = MTL_RSF_DISABLE;
	pdata->rx_threshold = MTL_RX_THRESHOLD_64;
	pdata->rx_pbl = DMA_PBL_16;
	pdata->pause_autoneg = 1;
	pdata->tx_pause = 1;
	pdata->rx_pause = 1;
	pdata->power_down = 0;

	if (speed == 10) {
		pdata->default_autoneg = AUTONEG_DISABLE;
		pdata->default_speed = SPEED_10000;
	} else if (speed == 2) {
		pdata->default_autoneg = AUTONEG_DISABLE;
		pdata->default_speed = SPEED_2500;
	} else if (speed == 1) {
		pdata->default_autoneg = AUTONEG_DISABLE;
		pdata->default_speed = SPEED_1000;
	} else {
		pdata->default_autoneg = AUTONEG_ENABLE;
		pdata->default_speed = SPEED_10000;
	}

	DBGPR("<--xgbe_default_config\n");
}

static void xgbe_init_all_fptrs(struct xgbe_prv_data *pdata)
{
	xgbe_init_function_ptrs_dev(&pdata->hw_if);
	xgbe_init_function_ptrs_desc(&pdata->desc_if);
}

static int xgbe_map_resources(struct xgbe_prv_data *pdata)
{
	struct platform_device *pdev = pdata->pdev;
	struct device *dev = pdata->dev;
	struct resource *res;

	/* Obtain the mmio areas for the device */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pdata->xgmac_regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(pdata->xgmac_regs)) {
		dev_err(dev, "xgmac ioremap failed\n");
		return PTR_ERR(pdata->xgmac_regs);
	}
	DBGPR("  xgmac_regs = %p\n", pdata->xgmac_regs);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	pdata->xpcs_regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(pdata->xpcs_regs)) {
		dev_err(dev, "xpcs ioremap failed\n");
		return PTR_ERR(pdata->xpcs_regs);
	}
	DBGPR("  xpcs_regs  = %p\n", pdata->xpcs_regs);

	return 0;
}

#ifdef CONFIG_ACPI
static int xgbe_acpi_support(struct xgbe_prv_data *pdata)
{
	struct acpi_device *adev = pdata->adev;
	struct device *dev = pdata->dev;
	const union acpi_object *property;
	acpi_status status;
	u64 cca;
	unsigned int i;
	int ret;

	/* Map the memory resources */
	ret = xgbe_map_resources(pdata);
	if (ret)
		return ret;

	/* Obtain the system clock setting */
	ret = acpi_dev_get_property(adev, XGBE_ACPI_DMA_FREQ, ACPI_TYPE_INTEGER,
				    &property);
	if (ret) {
		dev_err(dev, "unable to obtain %s acpi property\n",
			XGBE_ACPI_DMA_FREQ);
		return ret;
	}
	pdata->sysclk_rate = property->integer.value;

	/* Obtain the PTP clock setting */
	ret = acpi_dev_get_property(adev, XGBE_ACPI_PTP_FREQ, ACPI_TYPE_INTEGER,
				    &property);
	if (ret) {
		dev_err(dev, "unable to obtain %s acpi property\n",
			XGBE_ACPI_PTP_FREQ);
		return ret;
	}
	pdata->ptpclk_rate = property->integer.value;

	/* Retrieve the MAC address */
	ret = acpi_dev_get_property_array(adev, XGBE_ACPI_MAC_ADDR,
					  ACPI_TYPE_INTEGER, &property);
	if (ret) {
		dev_err(dev, "unable to obtain %s acpi property\n",
			XGBE_ACPI_MAC_ADDR);
		return ret;
	}
	if (property->package.count != 6) {
		dev_err(dev, "invalid %s acpi property\n",
			XGBE_ACPI_MAC_ADDR);
		return -EINVAL;
	}
	for (i = 0; i < property->package.count; i++) {
		union acpi_object *obj = &property->package.elements[i];

		pdata->mac_addr[i] = (u8)obj->integer.value;
	}
	if (!is_valid_ether_addr(pdata->mac_addr)) {
		dev_err(dev, "invalid %s acpi property\n",
			XGBE_ACPI_MAC_ADDR);
		return -EINVAL;
	}

	/* Retrieve the PHY mode - it must be "xgmii" */
	ret = acpi_dev_get_property(adev, XGBE_ACPI_PHY_MODE, ACPI_TYPE_STRING,
				    &property);
	if (ret) {
		dev_err(dev, "unable to obtain %s acpi property\n",
			XGBE_ACPI_PHY_MODE);
		return ret;
	}
	if (strcmp(property->string.pointer,
		   phy_modes(PHY_INTERFACE_MODE_XGMII))) {
		dev_err(dev, "invalid %s acpi property\n",
			XGBE_ACPI_PHY_MODE);
		return -EINVAL;
	}
	pdata->phy_mode = PHY_INTERFACE_MODE_XGMII;

#ifndef METHOD_NAME__CCA
#define METHOD_NAME__CCA "_CCA"
#endif
	/* Set the device cache coherency values */
	if (acpi_has_method(adev->handle, METHOD_NAME__CCA)) {
		status = acpi_evaluate_integer(adev->handle, METHOD_NAME__CCA,
					       NULL, &cca);
		if (ACPI_FAILURE(status)) {
			dev_err(dev, "error obtaining acpi _CCA method\n");
			return -EINVAL;
		}
	} else {
		dev_err(dev, "acpi method %s is required\n",
			METHOD_NAME__CCA);
		return -EINVAL;
	}

	if (cca) {
		pdata->axdomain = XGBE_DMA_OS_AXDOMAIN;
		pdata->arcache = XGBE_DMA_OS_ARCACHE;
		pdata->awcache = XGBE_DMA_OS_AWCACHE;
	} else {
		pdata->axdomain = XGBE_DMA_SYS_AXDOMAIN;
		pdata->arcache = XGBE_DMA_SYS_ARCACHE;
		pdata->awcache = XGBE_DMA_SYS_AWCACHE;
	}

	return 0;
}
#else   /* CONFIG_ACPI */
static int xgbe_acpi_support(struct xgbe_prv_data *pdata)
{
	return -EINVAL;
}
#endif  /* CONFIG_ACPI */

#ifdef CONFIG_OF
static int xgbe_of_support(struct xgbe_prv_data *pdata)
{
	struct device *dev = pdata->dev;
	const u8 *mac_addr;
	int ret;

	/* Map the memory resources */
	ret = xgbe_map_resources(pdata);
	if (ret)
		return ret;

	/* Obtain the system clock setting */
	pdata->sysclk = devm_clk_get(dev, XGBE_DMA_CLOCK);
	if (IS_ERR(pdata->sysclk)) {
		dev_err(dev, "dma devm_clk_get failed\n");
		return PTR_ERR(pdata->sysclk);
	}
	pdata->sysclk_rate = clk_get_rate(pdata->sysclk);

	/* Obtain the PTP clock setting */
	pdata->ptpclk = devm_clk_get(dev, XGBE_PTP_CLOCK);
	if (IS_ERR(pdata->ptpclk)) {
		dev_err(dev, "ptp devm_clk_get failed\n");
		return PTR_ERR(pdata->ptpclk);
	}
	pdata->ptpclk_rate = clk_get_rate(pdata->ptpclk);

	/* Retrieve the MAC address */
	mac_addr = of_get_mac_address(dev->of_node);
	if (!mac_addr) {
		dev_err(dev, "invalid mac address for this device\n");
		return -EINVAL;
	}
	memcpy(pdata->mac_addr, mac_addr, ETH_ALEN);

	/* Retrieve the PHY mode - it must be "xgmii" */
	pdata->phy_mode = of_get_phy_mode(dev->of_node);
	if (pdata->phy_mode != PHY_INTERFACE_MODE_XGMII) {
		dev_err(dev, "invalid phy-mode specified for this device\n");
		return -EINVAL;
	}

	/* Set the device cache coherency values */
	if (of_property_read_bool(dev->of_node, "dma-coherent")) {
		pdata->axdomain = XGBE_DMA_OS_AXDOMAIN;
		pdata->arcache = XGBE_DMA_OS_ARCACHE;
		pdata->awcache = XGBE_DMA_OS_AWCACHE;
	} else {
		pdata->axdomain = XGBE_DMA_SYS_AXDOMAIN;
		pdata->arcache = XGBE_DMA_SYS_ARCACHE;
		pdata->awcache = XGBE_DMA_SYS_AWCACHE;
	}

	return 0;
}
#else   /* CONFIG_OF */
static int xgbe_of_support(struct xgbe_prv_data *pdata)
{
	return -EINVAL;
}
#endif  /*CONFIG_OF */

static int xgbe_probe(struct platform_device *pdev)
{
	struct xgbe_prv_data *pdata;
	struct xgbe_hw_if *hw_if;
	struct xgbe_desc_if *desc_if;
	struct net_device *netdev;
	struct device *dev = &pdev->dev;
	int ret;

	DBGPR("--> xgbe_probe\n");

	netdev = alloc_etherdev_mq(sizeof(struct xgbe_prv_data),
				   XGBE_MAX_DMA_CHANNELS);
	if (!netdev) {
		dev_err(dev, "alloc_etherdev failed\n");
		ret = -ENOMEM;
		goto err_alloc;
	}
	SET_NETDEV_DEV(netdev, dev);
	pdata = netdev_priv(netdev);
	pdata->netdev = netdev;
	pdata->pdev = pdev;
	pdata->adev = ACPI_COMPANION(dev);
	pdata->dev = dev;
	platform_set_drvdata(pdev, netdev);

	spin_lock_init(&pdata->lock);
	mutex_init(&pdata->xpcs_mutex);
	spin_lock_init(&pdata->tstamp_lock);

	/* Set and validate the number of descriptors for a ring */
	BUILD_BUG_ON_NOT_POWER_OF_2(XGBE_TX_DESC_CNT);
	pdata->tx_desc_count = XGBE_TX_DESC_CNT;
	if (pdata->tx_desc_count & (pdata->tx_desc_count - 1)) {
		dev_err(dev, "tx descriptor count (%d) is not valid\n",
			pdata->tx_desc_count);
		ret = -EINVAL;
		goto err_io;
	}
	BUILD_BUG_ON_NOT_POWER_OF_2(XGBE_RX_DESC_CNT);
	pdata->rx_desc_count = XGBE_RX_DESC_CNT;
	if (pdata->rx_desc_count & (pdata->rx_desc_count - 1)) {
		dev_err(dev, "rx descriptor count (%d) is not valid\n",
			pdata->rx_desc_count);
		ret = -EINVAL;
		goto err_io;
	}

	/* Obtain device settings */
	if (pdata->adev && !acpi_disabled)
		ret = xgbe_acpi_support(pdata);
	else
		ret = xgbe_of_support(pdata);
	if (ret)
		goto err_io;

	/* Set the DMA mask */
	if (!dev->dma_mask)
		dev->dma_mask = &dev->coherent_dma_mask;
	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(40));
	if (ret) {
		dev_err(dev, "dma_set_mask_and_coherent failed\n");
		goto err_io;
	}

	/* Get the device interrupt */
	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		dev_err(dev, "platform_get_irq failed\n");
		goto err_io;
	}

	netdev->irq = ret;
	netdev->base_addr = (unsigned long)pdata->xgmac_regs;
	memcpy(netdev->dev_addr, pdata->mac_addr, netdev->addr_len);

	/* Set all the function pointers */
	xgbe_init_all_fptrs(pdata);
	hw_if = &pdata->hw_if;
	desc_if = &pdata->desc_if;

	/* Issue software reset to device */
	hw_if->exit(pdata);

	/* Populate the hardware features */
	xgbe_get_all_hw_features(pdata);

	/* Set default configuration data */
	xgbe_default_config(pdata);

	/* Calculate the number of Tx and Rx rings to be created
	 *  -Tx (DMA) Channels map 1-to-1 to Tx Queues so set
	 *   the number of Tx queues to the number of Tx channels
	 *   enabled
	 *  -Rx (DMA) Channels do not map 1-to-1 so use the actual
	 *   number of Rx queues
	 */
	pdata->tx_ring_count = min_t(unsigned int, num_online_cpus(),
				     pdata->hw_feat.tx_ch_cnt);
	pdata->tx_q_count = pdata->tx_ring_count;
	ret = netif_set_real_num_tx_queues(netdev, pdata->tx_ring_count);
	if (ret) {
		dev_err(dev, "error setting real tx queue count\n");
		goto err_io;
	}

	pdata->rx_ring_count = min_t(unsigned int,
				     netif_get_num_default_rss_queues(),
				     pdata->hw_feat.rx_ch_cnt);
	pdata->rx_q_count = pdata->hw_feat.rx_q_cnt;
	ret = netif_set_real_num_rx_queues(netdev, pdata->rx_ring_count);
	if (ret) {
		dev_err(dev, "error setting real rx queue count\n");
		goto err_io;
	}

	/* Prepare to regsiter with MDIO */
	pdata->mii_bus_id = kasprintf(GFP_KERNEL, "%s", pdev->name);
	if (!pdata->mii_bus_id) {
		dev_err(dev, "failed to allocate mii bus id\n");
		ret = -ENOMEM;
		goto err_io;
	}
	ret = xgbe_mdio_register(pdata);
	if (ret)
		goto err_bus_id;

	/* Set device operations */
	netdev->netdev_ops = xgbe_get_netdev_ops();
	netdev->ethtool_ops = xgbe_get_ethtool_ops();
#ifdef CONFIG_AMD_XGBE_DCB
	netdev->dcbnl_ops = xgbe_get_dcbnl_ops();
#endif

	/* Set device features */
	netdev->hw_features = NETIF_F_SG |
			      NETIF_F_IP_CSUM |
			      NETIF_F_IPV6_CSUM |
			      NETIF_F_RXCSUM |
			      NETIF_F_TSO |
			      NETIF_F_TSO6 |
			      NETIF_F_GRO |
			      NETIF_F_HW_VLAN_CTAG_RX |
			      NETIF_F_HW_VLAN_CTAG_TX |
			      NETIF_F_HW_VLAN_CTAG_FILTER;

	netdev->vlan_features |= NETIF_F_SG |
				 NETIF_F_IP_CSUM |
				 NETIF_F_IPV6_CSUM |
				 NETIF_F_TSO |
				 NETIF_F_TSO6;

	netdev->features |= netdev->hw_features;
	pdata->netdev_features = netdev->features;

	netdev->priv_flags |= IFF_UNICAST_FLT;

	xgbe_init_rx_coalesce(pdata);
	xgbe_init_tx_coalesce(pdata);

	netif_carrier_off(netdev);
	ret = register_netdev(netdev);
	if (ret) {
		dev_err(dev, "net device registration failed\n");
		goto err_reg_netdev;
	}

	xgbe_ptp_register(pdata);

	xgbe_debugfs_init(pdata);

	netdev_notice(netdev, "net device enabled\n");

	DBGPR("<-- xgbe_probe\n");

	return 0;

err_reg_netdev:
	xgbe_mdio_unregister(pdata);

err_bus_id:
	kfree(pdata->mii_bus_id);

err_io:
	free_netdev(netdev);

err_alloc:
	dev_notice(dev, "net device not enabled\n");

	return ret;
}

static int xgbe_remove(struct platform_device *pdev)
{
	struct net_device *netdev = platform_get_drvdata(pdev);
	struct xgbe_prv_data *pdata = netdev_priv(netdev);

	DBGPR("-->xgbe_remove\n");

	xgbe_debugfs_exit(pdata);

	xgbe_ptp_unregister(pdata);

	unregister_netdev(netdev);

	xgbe_mdio_unregister(pdata);

	kfree(pdata->mii_bus_id);

	free_netdev(netdev);

	DBGPR("<--xgbe_remove\n");

	return 0;
}

#ifdef CONFIG_PM
static int xgbe_suspend(struct device *dev)
{
	struct net_device *netdev = dev_get_drvdata(dev);
	int ret;

	DBGPR("-->xgbe_suspend\n");

	if (!netif_running(netdev)) {
		DBGPR("<--xgbe_dev_suspend\n");
		return -EINVAL;
	}

	ret = xgbe_powerdown(netdev, XGMAC_DRIVER_CONTEXT);

	DBGPR("<--xgbe_suspend\n");

	return ret;
}

static int xgbe_resume(struct device *dev)
{
	struct net_device *netdev = dev_get_drvdata(dev);
	int ret;

	DBGPR("-->xgbe_resume\n");

	if (!netif_running(netdev)) {
		DBGPR("<--xgbe_dev_resume\n");
		return -EINVAL;
	}

	ret = xgbe_powerup(netdev, XGMAC_DRIVER_CONTEXT);

	DBGPR("<--xgbe_resume\n");

	return ret;
}
#endif /* CONFIG_PM */

#ifdef CONFIG_ACPI
static const struct acpi_device_id xgbe_acpi_match[] = {
	{ "AMDI8000", 0 },
	{},
};

MODULE_DEVICE_TABLE(acpi, xgbe_acpi_match);
#endif

#ifdef CONFIG_OF
static const struct of_device_id xgbe_of_match[] = {
	{ .compatible = "amd,xgbe-seattle-v0a", },
	{},
};

MODULE_DEVICE_TABLE(of, xgbe_of_match);
#endif

static SIMPLE_DEV_PM_OPS(xgbe_pm_ops, xgbe_suspend, xgbe_resume);

static struct platform_driver xgbe_driver = {
	.driver = {
		.name = "amd-xgbe",
#ifdef CONFIG_ACPI
		.acpi_match_table = xgbe_acpi_match,
#endif
#ifdef CONFIG_OF
		.of_match_table = xgbe_of_match,
#endif
		.pm = &xgbe_pm_ops,
	},
	.probe = xgbe_probe,
	.remove = xgbe_remove,
};

module_platform_driver(xgbe_driver);
