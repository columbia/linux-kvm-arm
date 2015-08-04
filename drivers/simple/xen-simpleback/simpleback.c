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
/*
static void simpleif_notify_work(struct xen_simpleif *simpleif)
{
	int notify;
	struct simpleif_response resp;
	struct simpleif_back_ring* br = &simpleif->simple_back_ring;
	resp.operation = 3;
	memcpy(RING_GET_RESPONSE(br, br->rsp_prod_pvt), &resp, sizeof(resp));
	br->rsp_prod_pvt++;
	RING_PUSH_RESPONSES_AND_CHECK_NOTIFY(br, notify);
	//if (notify)
}
*/

static __always_inline volatile unsigned long read_cc(void)
{
	unsigned long cc;
#ifdef CONFIG_ARM64
	isb();
	//asm volatile("mrs %0, PMCCNTR_EL0" : "=r" (cc) ::);
	asm volatile("mrs %0, CNTPCT_EL0" : "=r" (cc) ::);
	isb();
#else
	asm volatile("mrc p15, 0, %[reg], c9, c13, 0": [reg] "=r" (cc));
#endif
	return cc;
}


irqreturn_t xen_simpleif_be_int(int irq, void *dev_id)
{
	struct xen_simpleif *simpleif=dev_id;
	/* Enable this to measure domU to dom0 */
	/*
	static unsigned long cc = 0;
	cc = read_cc();
	trace_printk("jintack simple backend at\t%ld\n", cc);
	*/

	/* Enable this to measure dom0 to domU */
	/*
	static unsigned long cc = 0;
	trace_printk("jintack simple backend previously at\t%ld\n", cc);
	cc=1000;
	cc = read_cc();
	*/

	notify_remote_via_irq(simpleif->irq);
//	HYPERVISOR_sched_op(SCHEDOP_yield, NULL);
	return IRQ_HANDLED;
}

module_init(xen_simpleif_init);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS("xen-backend:vsimple");
