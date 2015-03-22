#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/freezer.h>
#include <linux/bitmap.h>

#include <xen/events.h>
#include <xen/page.h>
#include <xen/xen.h>
#include <asm/xen/hypervisor.h>
#include <asm/xen/hypercall.h>
#include <xen/balloon.h>
#include "common.h"

static int __init xen_simpleif_init(void)
{
        int rc = 0;

        if (!xen_domain())
                return -ENODEV;

        rc = xen_simpleif_interface_init();
        if (rc)
                goto failed_init;

        rc = xen_simpleif_xenbus_init();
        if (rc)
                goto failed_init;

 failed_init:
        return rc;
}

module_init(xen_simpleif_init);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS("xen-backend:vsimple");
