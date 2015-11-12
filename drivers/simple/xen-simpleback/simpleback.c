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
static __always_inline volatile unsigned long read_cc_before(void)
{
        unsigned long cc;
#ifdef CONFIG_ARM64
        isb();
        asm volatile("mrs %0, PMCCNTR_EL0" : "=r" (cc) ::);
        isb();
#elif defined(CONFIG_ARM)
        asm volatile("mrc p15, 0, %[reg], c9, c13, 0": [reg] "=r" (cc));
#elif defined(CONFIG_X86_64)
        asm volatile ("CPUID\n\t"::: "%rax", "%rbx", "%rcx", "%rdx");
        asm volatile ( "RDTSC\n\t"
                        "shl $0x20, %%rdx\n\t"
                        "or %%rax, %%rdx\n\t"
                        "mov %%rdx, %0\n\t"
                        : "=r" (cc)
                        :: "%rax", "%rbx", "%rcx", "%rdx");
#endif
        return cc;
}

static __always_inline volatile unsigned long read_cc_after(void)
{
        unsigned long cc;
#ifdef CONFIG_ARM64
        isb();
        asm volatile("mrs %0, PMCCNTR_EL0" : "=r" (cc) ::);
        isb();
#elif defined(CONFIG_ARM)
        asm volatile("mrc p15, 0, %[reg], c9, c13, 0": [reg] "=r" (cc));
#elif defined(CONFIG_X86_64)
        asm volatile (
                        "mov %%cr0, %%rax\n\t"
                        "mov %%rax, %%cr0\n\t"
                        "RDTSC\n\t"
                        "shl $0x20, %%rdx\n\t"
                        "or %%rax, %%rdx\n\t"
                        "mov %%rdx, %0\n\t"
                        : "=r" (cc)
                        :: "%rax", "%rdx");
#endif
        return cc;
}

irqreturn_t xen_simpleif_be_int(int irq, void *dev_id)
{
        struct xen_simpleif *simpleif=dev_id;
        static unsigned long cc = 0;
        cc = read_cc_after();
	if (simpleif->irq != 0)
        	notify_remote_via_irq(simpleif->irq);
	/* we may need this if we run dom0 and domu on the same core */
        /* HYPERVISOR_sched_op(SCHEDOP_yield, NULL); */
        trace_printk("jintack simple backend at\t%ld\n", cc);
        return IRQ_HANDLED;
}

module_init(xen_simpleif_init);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_ALIAS("xen-backend:vsimple");
