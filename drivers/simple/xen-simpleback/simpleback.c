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


/*
 * Notification from the guest OS.
 */
static void simpleif_notify_work(struct xen_simpleif *simpleif)
{
	/* TODO: Read request? */
	int notify;
	struct simpleif_response resp;
	struct simpleif_back_ring* br = &simpleif->simple_back_ring;
	resp.operation = 3;
	memcpy(RING_GET_RESPONSE(br, br->rsp_prod_pvt), &resp, sizeof(resp));
	br->rsp_prod_pvt++;
	RING_PUSH_RESPONSES_AND_CHECK_NOTIFY(br, notify);
	//if (notify)
		notify_remote_via_irq(simpleif->irq);
}

irqreturn_t xen_simpleif_be_int(int irq, void *dev_id)
{
	printk("jintack simple back got interrupt!\n");
	simpleif_notify_work(dev_id);
	printk("jintack simple back push response!\n");
	return IRQ_HANDLED;
}

module_init(xen_simpleif_init);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS("xen-backend:vsimple");
