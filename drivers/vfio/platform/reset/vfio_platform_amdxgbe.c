/*
 * VFIO platform driver specialized for AMD xgbe reset
 * reset code is inherited from AMD xgbe native driver
 *
 * Copyright (c) 2015 Linaro Ltd.
 *              www.linaro.org
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>

#include "vfio_platform_private.h"

#define DRIVER_VERSION  "0.1"
#define DRIVER_AUTHOR   "Eric Auger <eric.auger@linaro.org>"
#define DRIVER_DESC     "Reset support for AMD xgbe vfio platform device"

/* DMA register offsets */
#define DMA_MR                          0x3000

int vfio_platform_amdxgbe_reset(struct vfio_platform_device *vdev)
{
	struct vfio_platform_region reg = vdev->regions[0];
	u32 dma_mr_value;

	if (!reg.ioaddr) {
		reg.ioaddr =
			ioremap_nocache(reg.addr, reg.size);
		if (!reg.ioaddr)
			return -ENOMEM;
	}

	/* Issue a software reset */
	dma_mr_value = ioread32(reg.ioaddr + DMA_MR);
	dma_mr_value |= 0x1;
	iowrite32(dma_mr_value, reg.ioaddr + DMA_MR);

	return 0;
}
EXPORT_SYMBOL_GPL(vfio_platform_amdxgbe_reset);

MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
