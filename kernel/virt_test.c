#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/kvm_host.h>
#include <asm/io.h>
#include <linux/virt_test.h>

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

static ccount_t read_cc(void)
{
        ccount_t cc;
#ifdef CONFIG_ARM64
	asm volatile("mrs %0, PMCCNTR_EL0" : "=r" (cc) ::);
#else
        asm volatile("mrc p15, 0, %[reg], c9, c13, 0": [reg] "=r" (cc));
#endif
        return cc;
}

#define GICC_EOIR		0x00000010

static ccount_t hvc_test(void)
{
	ccount_t ret, cc_before, cc_after;
	unsigned long flags;

	local_irq_save(flags);
	cc_before = read_cc();
	kvm_call_hyp((void*)0x4b000000);
	cc_after = read_cc();
	local_irq_restore(flags);
	ret = CYCLE_COUNT(cc_before, cc_after);

	return ret;
}

static ccount_t noop_test(void)
{
	ccount_t ret, cc_before, cc_after;
	unsigned long flags;

	local_irq_save(flags);
	cc_before = read_cc();
	__noop();
	cc_after = read_cc();
	local_irq_restore(flags);
	ret = CYCLE_COUNT(cc_before, cc_after);

	return ret;
}

static ccount_t mmio_user(void)
{
	ccount_t ret, cc_before, cc_after;
	u32 val;

	cc_before = read_cc();
	val = readl(mmio_read_user_addr + 0x8); // MMIO USER
	cc_after = read_cc();
	ret = CYCLE_COUNT(cc_before, cc_after);
	return ret;
}


static ccount_t mmio_kernel(void)
{
	ccount_t ret, cc_before, cc_after;
	u32 val;
	unsigned long flags;

	local_irq_save(flags);
	cc_before = read_cc();
	val = readl(vgic_dist_addr + 0x8); //Distributor Implementer Identification Register
	cc_after = read_cc();
	local_irq_restore(flags);
	ret = CYCLE_COUNT(cc_before, cc_after);

	return ret;
}

static ccount_t eoi_test(void)
{
	ccount_t ret, cc_before, cc_after;
	u32 val;

	unsigned long flags;
	val = 1023;
	local_irq_save(flags);
	cc_before = read_cc();
	writel(val, vgic_cpu_addr + GICC_EOIR);
	cc_after = read_cc();
	local_irq_restore(flags);
	ret = CYCLE_COUNT(cc_before, cc_after);

	return ret;
}

struct virt_test available_tests[] = {
	{ "hvc",                hvc_test,	true},
	{ "mmio_read_user",     mmio_user,	true},
	{ "mmio_read_vgic",     mmio_kernel,	true},
	{ "ipi",                NULL,		false},
	{ "eoi",                eoi_test,	true},
	{ "noop_guest",         noop_test,	true},
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
	ccount_t sample, cycles;
	ccount_t min = 0, max = 0;

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
				/* sample is always 0 here. why print? */
				/* pr_warn("cycle count overflow: %lu\n", sample); */
				continue;
			}
			cycles += sample;

			if (min == 0 || min > sample)
				min = sample;
			if (max < sample)
				max = sample;
		}

	} while (cycles < GOAL && count_cycles);

	//debug("%s exit %d cycles over %d iterations = %d\n",
	//       test->name, cycles, iterations, cycles / iterations);
	printk("columbia %s\t%lu\t%lu, min: %lu, max: %lu\n",
	       test->name, (unsigned long)(cycles / iterations), iterations, (unsigned long) min, (unsigned long) max);
}

static void run_test_once(struct virt_test *test)
{
	ccount_t sample;
	sample = test->test_fn();
	printk("columbia once %s\t%lu\n", test->name, (unsigned long)sample);
}

asmlinkage long sys_kvm_counter(int action)
{
	printk("action %d\n");
	if (action == 1)
		kvm_call_hyp((void*)0x4b000002);
	else if (action == 2)
		kvm_call_hyp((void*)0x4b000003);
	else if (action == 3)
		kvm_call_hyp((void*)0x4b000004);
	return 0;
		
}

SYSCALL_DEFINE0(unit_test)
{
	struct virt_test *test;
	unsigned long ret, i;
	/* unsigned int run_once = 0; */

	i = 0;
	ret = init_mmio_test();
	if (ret < 0)
		goto out;

	for_each_test(test, available_tests, i) {
		/* Reset count for each test*/
		kvm_call_hyp((void*)0x4b000001);

                if (!test->run)
                        continue;
		loop_test(test);
		run_test_once(test);
	}
out:
	return ret;
}
