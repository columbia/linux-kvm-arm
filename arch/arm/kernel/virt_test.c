#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <asm/io.h>
#include <linux/kvm_host.h>

static void *mmio_read_user_addr;
static void *vgic_dist_addr;
static void *vgic_cpu_addr;

extern void *gic_data_dist_base_ex(void);
extern void *gic_data_cpu_base_ex(void);

static bool count_cycles = true;

#define CYCLE_COUNT(c1, c2) \
        (((c1) > (c2) || ((c1) == (c2) && count_cycles)) ? 0 : (c2) - (c1))

static int init_mmio_test(void)
{
	int ret = 0;

	vgic_dist_addr = gic_data_dist_base_ex();
	if (!vgic_dist_addr) {
		ret = -ENODEV;
		goto out;
	}

	vgic_cpu_addr = gic_data_cpu_base_ex();
	if (!vgic_cpu_addr) {
		ret = -ENODEV;
		goto out;
	}

	mmio_read_user_addr = ioremap(0x0a000000, 0x200);
	if (!mmio_read_user_addr) {
		pr_warn("ioremap failed\n");
		ret = -EFAULT;
	}
out:
	return ret;
}

static inline void init_ccount(void)
{
	kvm_call_hyp((void*)0x4b000001);
	printk("init ccount\n");
}

static noinline void __noop(void)
{
}

#ifdef CONFIG_ARM
static unsigned long read_cc(void)
{
        unsigned long cc;
        asm volatile("mrc p15, 0, %[reg], c9, c13, 0": [reg] "=r" (cc));
        return cc;
}
#endif

#define GICC_EOIR		0x00000010
SYSCALL_DEFINE0(unit_test)
{
	unsigned long ret, cc_before, cc_after;
	u32 val;

	ret = 0;
	ret = init_mmio_test();
	if (ret < 0)
		goto out;

	init_ccount();

	cc_before = read_cc();
	//asm volatile("mov r0, #0x4b000000");
	//asm volatile("hvc #0");
	kvm_call_hyp((void*)0x4b000000);
	cc_after = read_cc();

	printk("columbia unit_test hvc: %lu\n", CYCLE_COUNT(cc_before, cc_after));

	cc_before = read_cc();
	__noop();
	cc_after = read_cc();

	printk("columbia unit_test noop: %lu\n", CYCLE_COUNT(cc_before, cc_after));

	cc_before = read_cc();
	val = readl(vgic_dist_addr + 0x8); //Distributor Implementer Identification Register
	cc_after = read_cc();

	printk("columbia unit_test kernel I/O: %lu\n", CYCLE_COUNT(cc_before, cc_after));
	//printk("columbia dist_addr is : 0x%x\n", vgic_dist_addr);
	//printk("columbia Register value: 0x%x\n", val);

	//printk("columbia ioremap value: 0x%x\n", mmio_read_user_addr);
	cc_before = read_cc();
	val = readl(mmio_read_user_addr + 0x8); // MMIO USER
	cc_after = read_cc();

	printk("columbia unit_test user I/O: %lu\n", CYCLE_COUNT(cc_before, cc_after));

	val = 1023;
	cc_before = read_cc();
	writel(val, vgic_cpu_addr + GICC_EOIR);
	cc_after = read_cc();

	printk("columbia unit_test EOI: %lu\n", CYCLE_COUNT(cc_before, cc_after));

out:
	return ret;
}
