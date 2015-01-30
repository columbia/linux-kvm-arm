#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <asm/io.h>

static noinline void __noop(void)
{
}

extern void *gic_data_dist_base_ex(void);
extern void *gic_data_cpu_base_ex(void);

#define GICC_EOIR		0x00000010
SYSCALL_DEFINE0(unit_test)
{
	u32 cc_before=11;
	u32 cc_after=11;
	u32 val = 0;
	void *vgic_dist_addr;
	void *vgic_cpu_addr;

	printk("WE ARE HERE\n");
	asm volatile(" mrc p15, 0, %0, c9, c13, 0\n" : "=r" (cc_before) ::);
	asm volatile("mov r0, #0x4b000000");
/*	asm volatile("hvc #0"); */
	asm volatile(" mrc p15, 0, %0, c9, c13, 0\n" : "=r" (cc_after) ::);

	printk("columbia unit_test hvc: %u\n", cc_after - cc_before);

	asm volatile(" mrc p15, 0, %0, c9, c13, 0\n" : "=r" (cc_before) ::);
	__noop();
	asm volatile(" mrc p15, 0, %0, c9, c13, 0\n" : "=r" (cc_after) ::);

	printk("columbia unit_test noop: %u\n", cc_after - cc_before);


	vgic_dist_addr = gic_data_dist_base_ex();
	asm volatile(" mrc p15, 0, %0, c9, c13, 0\n" : "=r" (cc_before) ::);
	val = readl(vgic_dist_addr + 0x8); //Distributor Implementer Identification Register
	asm volatile(" mrc p15, 0, %0, c9, c13, 0\n" : "=r" (cc_after) ::);

	printk("columbia unit_test kernel I/O: %u\n", cc_after - cc_before);
	printk("columbia dist_addr is : 0x%x\n", vgic_dist_addr);
	printk("columbia Register value: 0x%x\n", val);

	val = 1023;
	vgic_cpu_addr = gic_data_cpu_base_ex();
	asm volatile(" mrc p15, 0, %0, c9, c13, 0\n" : "=r" (cc_before) ::);
	writel(val, vgic_cpu_addr + GICC_EOIR);
	asm volatile(" mrc p15, 0, %0, c9, c13, 0\n" : "=r" (cc_after) ::);
	printk("columbia unit_test EOI: %u\n", cc_after - cc_before);

	return (cc_after-cc_before);
}
