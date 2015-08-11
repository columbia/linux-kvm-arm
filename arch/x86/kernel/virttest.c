/*
 * Measure cycles to perform various operations as an x86 VM guest
 *
 * Copyright (C) 2015 Columbia University
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * This code is not meant for upstream but as a testig framework for measuring
 * performance of hypervisors, such as KVM and Xen.
 *
 */
#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/kvm_host.h>
#include <linux/proc_fs.h>
#include <asm/io.h>
#include <asm/virttest.h>

#define GOAL (1ULL << 28)
#define for_each_test(_iter, _tests, _tmp) \
	for (_tmp = 0, _iter = _tests; \
	     _tmp < ARRAY_SIZE(_tests); \
	     _tmp++, _iter++)

#define CYCLE_COUNT(c1, c2) \
	((c1) > (c2)) ? 0 : (c2) - (c1)

#define PROCFS_MAX_SIZE 128
#define MAX_MSG_LEN 512


static unsigned long noop_test(void)
{
	int ret = 1;
	return ret;
}

struct virt_test available_tests[] = {
	{ "noop",		noop_test	},
};

static void loop_test(struct virt_test *test)
{
	unsigned long i, iterations = 32;
	unsigned long sample, cycles;
	unsigned long min = 0, max = 0, avg = 0;

	do {
		iterations *= 2;
		cycles = 0;

		for (i = 0; i < iterations; i++) {
			sample = test->test_fn();
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
	       test->name, avg, iterations, min, max);
}

static void run_test_once(struct virt_test *test)
{
	unsigned long sample;
	sample = test->test_fn();
	trace_printk("virt-test once %s\t%lu\n", test->name, sample);
}

static int arm_virt_unit_test(unsigned long op, bool once)
{
	struct virt_test *test;
	int i;

	if (op > ARRAY_SIZE(available_tests))
		return -EINVAL;

	if (op > 0) {
		test = &available_tests[op - 1];
		if (once)
			run_test_once(test);
		else
			loop_test(test);
	} else {
		for_each_test(test, available_tests, i) {
			test = &available_tests[i];
			loop_test(test);
			run_test_once(test);
		}
	}

	return 0;
}

static ssize_t __virttest_write(struct file *file, const char __user *buffer,
				size_t count, loff_t *pos, bool once)
{
	int ret;
	unsigned long val;

	ret = kstrtoul_from_user(buffer, count, 10, &val);
	if (ret)
		return ret;

	ret = arm_virt_unit_test(val, once);
	if (ret)
		return ret;

	*pos += count;

	return ret ? ret : count;
}

static ssize_t virttest_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *pos)
{
	return __virttest_write(file, buffer, count, pos, false);
}

static ssize_t virttest_once_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *pos)
{
	return __virttest_write(file, buffer, count, pos, true);
}

static int virt_test_proc_show(struct seq_file *m, void *v)
{
	int i;
	struct virt_test *test;

	seq_printf(m, "Usage: echo <test_idx> > /proc/virttest\n\n");
	seq_printf(m, "Test Idx    Test Name\n");
	seq_printf(m, "---------------------\n");
	seq_printf(m, "       0    All tests\n");
	for_each_test(test, available_tests, i) {
		seq_printf(m, "     %3d    %s\n", i + 1, test->name);
	}

	return 0;
};

static int virt_test_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, virt_test_proc_show, NULL);
}

static const struct file_operations virttest_proc_fops = {
	.owner = THIS_MODULE,
	.open = virt_test_proc_open,
	.read = seq_read,
	.write = virttest_write,
};

static const struct file_operations virttest_once_proc_fops = {
	.owner = THIS_MODULE,
	.open = virt_test_proc_open,
	.read = seq_read,
	.write = virttest_once_write,
};


static int __init virt_test_init(void)
{
	proc_create("virttest", 0, NULL, &virttest_proc_fops);
	proc_create("virttest_one", 0, NULL, &virttest_once_proc_fops);
	pr_info("virt-tests successfully initialized\n");
	return 0;
}
__initcall(virt_test_init);
