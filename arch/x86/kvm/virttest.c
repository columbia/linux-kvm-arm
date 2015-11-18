#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/cpu.h>
#include <linux/kvm_host.h>
#include <linux/proc_fs.h>
#include <asm/io.h>
#include <asm/hypervisor.h>
#include <asm/xen/hypercall.h>

extern unsigned long virttest_inject_irq(void);

#define IOLAT_OUT "IO Latnecy Out "
#define GOAL (1ULL << 2)

static unsigned long io_latency_in(void)
{
	unsigned long cc;

	cc = virttest_inject_irq();
	return cc;
}

static void loop_test(void)
{
	unsigned long i, iterations = 32;
	unsigned long sample, cycles;
	unsigned long min = 0, max = 0, avg = 0;

	do {
		iterations *= 2;
		cycles = 0;

		for (i = 0; i < iterations; i++) {
			sample = io_latency_in();
			if (sample == 0) {
				/* If something went wrong or we had an
				 * overflow, don't count that sample */
				iterations--;
				i--;
				pr_warn("cycle count overflow: %lu\n", sample);

				continue;
			}
			cycles += sample;

			if (min == 0 || min > sample)
				min = sample;
			if (max < sample)
				max = sample;
		}

	} while (cycles < GOAL);

	//debug("%s exit %d cycles over %d iterations = %d\n",
	//       test->name, cycles, iterations, cycles / iterations);
	avg = cycles / iterations;
	trace_printk("virt-test %s\t%lu\t%lu\tmin:\t%lu\tmax:\t%lu\n",
	       IOLAT_OUT, avg, iterations, min, max);

	return;
}

static void run_test_once(void)
{
	unsigned long sample;
	//sample = test->test_fn();
	sample = io_latency_in();
	trace_printk("virt-test once %s\t%lu\n", IOLAT_OUT, sample);
}

static ssize_t iolat_test(struct file *file, const char __user *buffer,
		size_t count, loff_t *pos)
{
	int ret;

	//loop_test();
	run_test_once();

	*pos += count;
	return ret ? ret : count;
}

static int iolat_show(struct seq_file *m, void *v)
{
	return 0;
}

static int iolat_open(struct inode *inode, struct file *file)
{
	return single_open(file, iolat_show, NULL);
}

static const struct file_operations virttest_iolat_fops = {
	.owner = THIS_MODULE,
	.open = iolat_open,
	.read = seq_read,
	.write = iolat_test,
};

static int __init virt_test_init(void)
{
	proc_create("iolat", 0, NULL, &virttest_iolat_fops);
	pr_info("virt-tests successfully initialized\n");
	return 0;
}

__initcall(virt_test_init);
