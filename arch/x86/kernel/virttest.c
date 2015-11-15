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

static ssize_t iolat_test(struct file *file, const char __user *buffer,
		size_t count, loff_t *pos)
{
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
	int ret;
	
	proc_create("iolat", 0, NULL, &virttest_iolat_fops);
	pr_info("virt-tests successfully initialized\n");
	return 0;
}

__initcall(virt_test_init);
