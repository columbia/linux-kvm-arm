#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/kvm_host.h>
#include <linux/virt_test.h>
#include <linux/proc_fs.h>
#include <asm/io.h>

static void *mmio_read_user_addr;
static void *vgic_dist_addr;
static void *vgic_cpu_addr;

extern void *gic_data_dist_base_ex(void);
extern void *gic_data_cpu_base_ex(void);

static bool count_cycles = true;

volatile int cpu1_ipi_ack;

#ifndef CONFIG_ARM64
__asm__(".arch_extension	virt");
#endif


#define GOAL (1ULL << 28)
#define ARR_SIZE(_x) ((int)(sizeof(_x) / sizeof(_x[0])))
#define for_each_test(_iter, _tests, _tmp) \
	for (_tmp = 0, _iter = _tests; \
	     _tmp < ARR_SIZE(_tests); \
	     _tmp++, _iter++)

#define CYCLE_COUNT(c1, c2) \
	(((c1) > (c2) || ((c1) == (c2) && count_cycles)) ? 0 : (c2) - (c1))

#define PROCFS_MAX_SIZE 128
#define MAX_MSG_LEN 512

extern void smp_send_virttest(int cpu);

static noinline void __noop(void)
{
}

static volatile ccount_t read_cc(void)
{
	ccount_t cc;
	isb();
#ifdef CONFIG_ARM64
	asm volatile("mrs %0, PMCCNTR_EL0" : "=r" (cc) ::);
#else
	asm volatile("mrc p15, 0, %[reg], c9, c13, 0": [reg] "=r" (cc));
#endif
	isb();
	return cc;
}

#define GICC_EOIR		0x00000010

void send_and_wait_ipi(void)
{
	int cpu;
	unsigned long timeout = 1U << 28;

	cpu1_ipi_ack = 0;
	for_each_online_cpu(cpu) {
		if (cpu == smp_processor_id())
			continue;
		else {
			smp_send_virttest(cpu);
			break;
		}
	}
	while (!cpu1_ipi_ack && timeout--);

	if (!cpu1_ipi_ack)
		pr_warn("ipi received failed\n");

	return;
}

static ccount_t ipi_test(void)
{
	ccount_t ret, cc_before, cc_after;
	unsigned long flags;

	local_irq_save(flags);
	cc_before = read_cc();
	send_and_wait_ipi();
	cc_after = read_cc();
	local_irq_restore(flags);
	ret = CYCLE_COUNT(cc_before, cc_after);

	return ret;

}

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
	val = readl(vgic_dist_addr + 0x8); /* GICD_IIDR */
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

static ccount_t trap_out_test(void)
{
	unsigned long flags;
	ccount_t trap_out = 0;
	ccount_t before_hvc= 0, soh = 0, after_hvc = 0;
#ifndef CONFIG_ARM64
	ccount_t cc0 = 0, cc1 = 0, cc2 = 0;
#endif
	ccount_t eoh = 0; /* end of hyp */

	before_hvc = 0, soh = 0, after_hvc = 0;
	local_irq_save(flags);
#ifdef CONFIG_ARM64
	asm volatile(
			"mov x0, #0x10000\n\t"
			"mrs x3 , PMCCNTR_EL0\n\t"
			"hvc #0\n\t"
			"mrs x2 , PMCCNTR_EL0\n\t"
			"mov %[before_hvc], x3\n\t"
			"mov %[soh], x1\n\t"
			"mov %[eoh], x4\n\t"
			"mov %[after_hvc], x2\n\t":
			[before_hvc] "=r" (before_hvc),
			[soh] "=r" (soh),
			[after_hvc] "=r" (after_hvc),
			[eoh] "=r" (eoh): :
			"x0", "x1", "x2", "x3", "x4");
#else
	asm volatile(
			"mov r0, #0x4c000000\n\t"
			"mrc p15, 0, r3, c9, c13, 0\n\t"
			"hvc #0\n\t"
			"mrc p15, 0, r2, c9, c13, 0\n\t"
			"mov %[cc0], r3\n\t"
			"mov %[cc1], r1\n\t"
			"mov %[cc2], r2\n\t":
			[cc0] "=r" (cc0),
			[cc1] "=r" (cc1),
			[cc2] "=r" (cc2): :
			"r0", "r1", "r2", "r3");
#endif
	local_irq_restore(flags);
	trap_out = after_hvc - soh;

	return trap_out;
}


static ccount_t trap_in_test(void)
{
	unsigned long flags;
	ccount_t trap_in = 0;
	ccount_t cc0 = 0, cc1 = 0, cc2 = 0;

	cc0 = 0, cc1 = 0, cc2 = 0;

	local_irq_save(flags);
#ifdef CONFIG_ARM64
	asm volatile(
			"mov x0, #0x10000\n\t"
			"mrs x3 , PMCCNTR_EL0\n\t"
			"hvc #0\n\t"
			"mrs x2 , PMCCNTR_EL0\n\t"
			"mov %[cc0], x3\n\t"
			"mov %[cc1], x1\n\t"
			"mov %[cc2], x2\n\t":
			[cc0] "=r" (cc0),
			[cc1] "=r" (cc1),
			[cc2] "=r" (cc2): :
			"x0", "x1", "x2", "x3");
#else
	asm volatile(
			"mov r0, #0x4c000000\n\t"
			"mrc p15, 0, r3, c9, c13, 0\n\t"
			"hvc #0\n\t"
			"mrc p15, 0, r2, c9, c13, 0\n\t"
			"mov %[cc0], r3\n\t"
			"mov %[cc1], r1\n\t"
			"mov %[cc2], r2\n\t":
			[cc0] "=r" (cc0),
			[cc1] "=r" (cc1),
			[cc2] "=r" (cc2): :
			"r0", "r1", "r2", "r3");
#endif
	local_irq_restore(flags);

	trap_in = cc1 - cc0;

	return trap_in;
}

struct virt_test available_tests[] = {
	{ "hvc",		hvc_test,	true},
	{ "mmio_read_user",	mmio_user,	false},
	{ "mmio_read_vgic",	mmio_kernel,	true},
	{ "eoi",		eoi_test,	true},
	{ "noop_guest",		noop_test,	true},
	{ "ipi",		ipi_test,	true},
	{ "trap-in",		trap_in_test,	false},
	{ "trap-out",		trap_out_test,	false},
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
				/* Remove Me  */
				kvm_call_hyp((void*)0x4b000001);
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
	       test->name, (unsigned long)(cycles / iterations), iterations,
	       (unsigned long) min, (unsigned long) max);
}

static void run_test_once(struct virt_test *test)
{
	ccount_t sample;
	sample = test->test_fn();
	printk("columbia once %s\t%lu\n", test->name, (unsigned long)sample);
}

static u32 arm_virt_unit_test(int op)
{
	struct virt_test *test;
	unsigned long ret, i;
	/* unsigned int run_once = 0; */
	if (op <= 0 || op >= 10) {
		kvm_err("invalid op range\n");
		ret = -EINVAL;
		goto out;
	}

	i = 0;
	printk("columbia unit-test start----------------------\n");
	ret = init_mmio_test();
	if (ret < 0)
		goto out;

	for_each_test(test, available_tests, i) {
		/* Reset count for each test */
		kvm_call_hyp((void*)0x4b000001);

		/* test gets run when op-2=index or task->run = true */
		if (op == 1 && !test->run)
			continue;
		else if (op != 1 && (op - 2) != i)
			continue;
		loop_test(test);
		run_test_once(test);
	}
out:
	printk("columbia unit-test end----------------------\n");
	return ret;
}

static ssize_t virttest_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *pos)
{
	unsigned long procfs_buffer_size;
	int ret, op;
	char procfs_buffer[PROCFS_MAX_SIZE] = {0};
	char *ptr;

	/* get buffer size */
	procfs_buffer_size = count;
	if (procfs_buffer_size > PROCFS_MAX_SIZE ) {
		procfs_buffer_size = PROCFS_MAX_SIZE;
	}

	/* write data to the buffer */
	if ( copy_from_user(procfs_buffer, buffer, procfs_buffer_size) ) {
		return -EFAULT;
	}

	ptr = procfs_buffer;
	do {
		if (*ptr == '\n') {
			*ptr = '\0';
			break;
		}
	} while (ptr++, ptr);
	op = simple_strtol(procfs_buffer, NULL, 10);

	ret = arm_virt_unit_test(op);
	if (ret < 0)
		return ret;

	ret = procfs_buffer_size;
	return ret;
}

static const char virttest_msg[MAX_MSG_LEN] =
	"Usage: echo [num] > /proc/virttest\n \
	num = 1: test all\n \
	num = 2: hvc_test\n";

static ssize_t virttest_read(struct file *file, char __user *buffer,
		size_t count, loff_t *pos)
{
	if (*pos != 0)
		return 0;
	if (copy_to_user(buffer, virttest_msg, MAX_MSG_LEN))
		return -EFAULT;
	*pos = MAX_MSG_LEN;
	return MAX_MSG_LEN;
};

static const struct file_operations virttest_proc_fops = {
	.owner = THIS_MODULE,
	.read = virttest_read,
	.write = virttest_write,
};

void inline init_virt_test(void)
{
	proc_create("virttest", 0, NULL, &virttest_proc_fops);
	return;
}
