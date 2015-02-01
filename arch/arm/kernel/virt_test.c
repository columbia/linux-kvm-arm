#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/kvm_host.h>
#include <asm/io.h>
#include <asm/virt_test.h>

static void *mmio_read_user_addr;
static void *vgic_dist_addr;
static void *vgic_cpu_addr;

extern void *gic_data_dist_base_ex(void);
extern void *gic_data_cpu_base_ex(void);

static bool count_cycles = true;

#define GOAL (1ULL << 28)
#define ARR_SIZE(_x) ((int)(sizeof(_x) / sizeof(_x[0])))
#define for_each_test(_iter, _tests, _tmp) \
        for (_tmp = 0, _iter = _tests; \
             _tmp < ARR_SIZE(_tests); \
             _tmp++, _iter++)

#define CYCLE_COUNT(c1, c2) \
        (((c1) > (c2) || ((c1) == (c2) && count_cycles)) ? 0 : (c2) - (c1))

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

static unsigned long hvc_test(void)
{
	unsigned long ret, cc_before, cc_after;

	cc_before = read_cc();
	kvm_call_hyp((void*)0x4b000000);
	cc_after = read_cc();
	ret = CYCLE_COUNT(cc_before, cc_after);
	return ret;
}

static unsigned long noop_test(void)
{
	unsigned long ret, cc_before, cc_after;

	cc_before = read_cc();
	__noop();
	cc_after = read_cc();
	ret = CYCLE_COUNT(cc_before, cc_after);
	return ret;
}

static unsigned long mmio_user(void)
{
	unsigned long ret, cc_before, cc_after;
	u32 val;

	cc_before = read_cc();
	val = readl(mmio_read_user_addr + 0x8); // MMIO USER
	cc_after = read_cc();
	ret = CYCLE_COUNT(cc_before, cc_after);
	return ret;
}


static unsigned long mmio_kernel(void)
{
	unsigned long ret, cc_before, cc_after;
	u32 val;

	cc_before = read_cc();
	val = readl(vgic_dist_addr + 0x8); //Distributor Implementer Identification Register
	cc_after = read_cc();
	ret = CYCLE_COUNT(cc_before, cc_after);
	return ret;
}

static unsigned long eoi_test(void)
{
	unsigned long ret, cc_before, cc_after;
	u32 val;

	val = 1023;
	cc_before = read_cc();
	writel(val, vgic_cpu_addr + GICC_EOIR);
	cc_after = read_cc();
	ret = CYCLE_COUNT(cc_before, cc_after);
	return ret;
}

struct virt_test available_tests[] = {
	{ "hvc",                hvc_test,	true },
	{ "noop_guest",         noop_test,	true },
	{ "mmio_read_user",     mmio_user,	true },
	{ "mmio_read_vgic",     mmio_kernel,	true },
	{ "ipi",                NULL,		false },
	{ "eoi",                eoi_test,	true },
};

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

static void loop_test(struct virt_test *test)
{
	unsigned long i, iterations = 32;
	unsigned long sample, cycles;

	/* Reset count for each test*/
	kvm_call_hyp((void*)0x4b000001);
	do {
		iterations *= 2;
		cycles = 0;

		for (i = 0; i < iterations; i++) {
			sample = test->test_fn();
			if (sample == 0 && count_cycles) {
				/* If something went wrong or we had an
				 * overflow, don't count that sample */
				iterations--;
				i--;
				pr_warn("cycle count overflow: %lu\n", sample);
				continue;
			}
			cycles += sample;
		}

	} while (cycles < GOAL && count_cycles);

	//debug("%s exit %d cycles over %d iterations = %d\n",
	//       test->name, cycles, iterations, cycles / iterations);
	printk("columbia %s\t%lu\n",
	       test->name, cycles / iterations);
}

SYSCALL_DEFINE0(unit_test)
{
	struct virt_test *test;
	unsigned long ret, i;

	i = 0;
	ret = init_mmio_test();
	if (ret < 0)
		goto out;

	for_each_test(test, available_tests, i) {
                if (!test->run)
                        continue;
		loop_test(test);
	}
out:
	return ret;
}
