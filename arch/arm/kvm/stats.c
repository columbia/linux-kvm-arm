/*
 * Copyright (C) 2015 - Columbia University
 * Author: Christoffer Dall <cdall@cs.columbia.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *
 * This code tries to profile the overhead introduced by KVM spent outside of
 * the VM.  We make several attempts to separately report the time spent by
 * the system outside the VM, but not by KVM, because the thread is being
 * scheduled off the CPU or because time is spent processing interrupts.
 *
 * Each measurement observation is like a period of a cosine curve, where Y=1
 * is host EL1, Y=0 is EL2, and Y=-1 is the guest.  In that way, and
 * observation is measurements of real time between each local minima (time
 * outside the guest):
 *
 *  Host:  ---                      ---------------
 *            |                    |               |
 *            |                    |               |
 *   EL2:      ----           -----                 -----
 *                 |         |                           |
 *                 |         |                           |
 * Guest:           ---------                             -----------
 *                          ^                             ^
 *             Start of observation                     End of observation
 */

#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/kvm_host.h>
#include <linux/vmalloc.h>
#include <linux/kvm.h>
#include <linux/sched.h>
#include <linux/seq_file.h>

#include <asm/uaccess.h>
#include <asm/ptrace.h>
#include <asm/virt.h>
#include <asm/kvm_arm.h>
#include <asm/kvm_asm.h>
#include <asm/kvm_mmu.h>
#include <asm/kvm_emulate.h>
#include <asm/kvm_coproc.h>
#include <asm/kvm_psci.h>

#include <clocksource/arm_arch_timer.h>

static unsigned long arch_timer_rate;

void kvm_arch_sched_in(struct kvm_vcpu *vcpu, int cpu)
{
	struct kvm_exit_data *new = vcpu->stat.exit_stats.new_edata;
	unsigned long now;

	now = kvm_arm_read_pcounter();
	new->sched_out_time += (now - new->sched_out_start);
	new->sched_out_start = 0;
}

void kvm_arch_sched_out(struct kvm_vcpu *vcpu)
{
	struct kvm_exit_data *new = vcpu->stat.exit_stats.new_edata;
	new->sched_out_start = kvm_arm_read_pcounter();
	new->sched_out_nr++;
}

void kvm_arch_notify_irq_start(void)
{
	struct kvm_vcpu *vcpu = kvm_arm_get_running_vcpu();
	struct kvm_exit_data *new;

	if (!vcpu)
		return;

	new = vcpu->stat.exit_stats.new_edata;
	new->el1_irq_start = kvm_arm_read_pcounter();
	new->el1_irq_nr++;
}

void kvm_arch_notify_irq_end(void)
{
	struct kvm_vcpu *vcpu = kvm_arm_get_running_vcpu();
	struct kvm_exit_data *new;
	unsigned long now;

	if (!vcpu)
		return;

	new = vcpu->stat.exit_stats.new_edata;
	now = kvm_arm_read_pcounter();
	new->el1_irq_time += (now - new->el1_irq_start);

	if (!vcpu->stat.exit_stats.in_handle_exit)
		new->el1_irq_no_handle_exit_time += (now - new->el1_irq_start);
}

void kvm_trap_stat_set_exit_reason(struct kvm_vcpu *vcpu,
				   int exit_reason)
{
	struct kvm_exit_data *new = vcpu->stat.exit_stats.new_edata;
	new->trap_reason = exit_reason;
}

void kvm_stat_new_loop(struct kvm_vcpu *vcpu)
{
	struct kvm_exit_data *new = vcpu->stat.exit_stats.new_edata;
	new->entry_el1_loop_start = kvm_arm_read_pcounter();
}

void kvm_stat_enter_guest(struct kvm_vcpu *vcpu)
{
	struct kvm_exit_data *new = vcpu->stat.exit_stats.new_edata;
	new->entry_el1 = kvm_arm_read_pcounter();
}

void kvm_stat_exit_guest(struct kvm_vcpu *vcpu)
{
	struct kvm_exit_data *new = vcpu->stat.exit_stats.new_edata;
	new->exit_el1 = kvm_arm_read_pcounter();
}

void kvm_stat_handle_exit_begin(struct kvm_vcpu *vcpu)
{
	struct kvm_exit_data *new = vcpu->stat.exit_stats.new_edata;
	vcpu->stat.exit_stats.in_handle_exit = true;
	new->exit_el1_handle_exit = kvm_arm_read_pcounter();
}

/*
 * This is the main function we run when we have a complete observation and
 * are done with all the critical path exit stuff.
 */
static void update_exit_stats(struct kvm_vcpu *vcpu)
{
	struct kvm_exit_stats *estats = &vcpu->stat.exit_stats;
	struct kvm_exit_data *prev = estats->prev_edata;
	struct kvm_exit_data *new = estats->new_edata;
	unsigned long time, switch_time;

	/* Check if we have a full prev observation (not first run) */
	if (!prev->exit_el2)
		goto out;

	time = prev->entry_el2 - prev->exit_el2;

	estats->trap_exit_time[prev->trap_reason] += time;
	estats->trap_exit_nr[prev->trap_reason]++;

	time = time - (prev->sched_out_time + prev->el1_irq_time);
	estats->trap_exit_time_in_kvm[prev->trap_reason] += time;

	estats->total_irq_time += prev->el1_irq_time;
	estats->total_irq_nr += prev->el1_irq_nr;
	estats->total_sched_out_time += prev->sched_out_time;
	estats->total_sched_out_nr += prev->sched_out_nr;

	estats->total_el2 += (prev->exit_el1 - prev->exit_el2) +
			     (prev->entry_el2 - prev->entry_el1);

	estats->total_guest += new->exit_el2 - prev->entry_el2;

	switch_time =
		(prev->exit_el1_handle_exit - prev->exit_el2) +
		(prev->entry_el2 - prev->entry_el1_loop_start);

	estats->switch_time += switch_time;
	estats->switch_time_in_kvm +=
		switch_time - prev->el1_irq_no_handle_exit_time;

out:
	/* We are done with prev, let's flush it for the next run */
	memset(prev, 0, sizeof(*prev));
}

void kvm_stat_handle_exit_end(struct kvm_vcpu *vcpu)
{
	update_exit_stats(vcpu);
	vcpu->stat.exit_stats.in_handle_exit = false;
}

void kvm_vcpu_init_trap_stats(struct kvm_vcpu *vcpu)
{
	struct kvm_exit_stats *estats = &vcpu->stat.exit_stats;
	memset(estats, 0, sizeof(*estats));
	estats->new_edata = &estats->edata1;
	estats->prev_edata = &estats->edata2;
	estats->reset_time = kvm_arm_read_pcounter();
}

#define cutoff(x) x = max(x, now)
void kvm_vcpu_reset_trap_stats(struct kvm_vcpu *vcpu)
{
	struct kvm_exit_stats *estats = &vcpu->stat.exit_stats;
	struct kvm_exit_data *prev = estats->prev_edata;
	struct kvm_exit_data *new = estats->new_edata;
	unsigned long now = kvm_arm_read_pcounter();

	memset(estats->trap_exit_time, 0, sizeof(unsigned long) * TRAP_MAX);
	memset(estats->trap_exit_nr, 0, sizeof(unsigned long) * TRAP_MAX);
	memset(estats->trap_exit_time_in_kvm, 0, sizeof(unsigned long) * TRAP_MAX);
	estats->total_el2 = 0;
	estats->total_guest = 0;
	estats->switch_time = 0;
	estats->switch_time_in_kvm = 0;
	estats->total_irq_time = 0;
	estats->total_irq_nr = 0;
	estats->total_sched_out_time = 0;
	estats->total_sched_out_nr = 0;

	memset(prev, 0, sizeof(*prev));

	/* Adjust any data from current observation before the reset time */
	cutoff(new->exit_el2);
	cutoff(new->exit_el1);
	cutoff(new->el1_irq_start);
	new->el1_irq_time = 0;
	new->el1_irq_no_handle_exit_time = 0;
	new->el1_irq_nr = 0;
	cutoff(new->sched_out_start);
	new->sched_out_time = 0;
	new->sched_out_nr = 0;
	cutoff(new->exit_el1_handle_exit);
	cutoff(new->entry_el1_loop_start);
	cutoff(new->entry_el1);
	cutoff(new->entry_el2);

	estats->reset_time = now;
}

static void reset_vm_stats(struct kvm *kvm)
{
	struct kvm_vcpu *vcpu;
	int i;

	/* TODO: This feels racy, we don't care for now */
	kvm_for_each_vcpu(i, vcpu, kvm)
		kvm_vcpu_reset_trap_stats(vcpu);
}

static void reset_all_stats(void)
{
	struct kvm *kvm;

	spin_lock(&kvm_lock);
	list_for_each_entry(kvm, &vm_list, vm_list)
		reset_vm_stats(kvm);
	spin_unlock(&kvm_lock);
}

/*
 * Users can write 'reset' into the debugfs file.
 */
static ssize_t stats_fs_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *pos)
{
	char command[64];
	int len;

	len = min(count, sizeof(command) - 1);
	if (strncpy_from_user(command, buffer, len) < 0)
		return -EFAULT;
	command[len] = '\0';

#if 0
	if (strncmp(command, "enable", 6) == 0) {
	} else if (strncmp(command, "disable", 7) == 0) {
	} else if (strncmp(command, "reset", 5) == 0) {
	}
#endif
	if (strncmp(command, "reset", 5) == 0)
		reset_all_stats();

	/* ignore the rest of the buffer, only one command at a time */
	*pos += count;
	return count;
}

static const char* trap_stat_names[TRAP_MAX] = {
	[TRAP_HVC]		= "HVC",
	[TRAP_WFE]		= "WFE",
	[TRAP_WFI]		= "WFI",
	[TRAP_IO_KERNEL]	= "IO_KERNEL",
	[TRAP_IO_USER]		= "IO_USER",
	[TRAP_IRQ]		= "IRQ"
};


#define msec(x) ((x) / (arch_timer_rate / 1000))

#define print_hdr(m, h1, h2, h3, h4) \
	seq_printf(m, "%14s  %12s  %12s  %12s\n", h1, h2, h3, h4)
#define print_rec(m, lbl, d1, d2, d3) \
	seq_printf(m, "%14s  %12lu  %12lu  %12lu\n", \
		   lbl, d1, msec(d2), msec(d3))
#define print_rec2(m, lbl, d1, d2) \
	seq_printf(m, "%14s  %12lu  %12lu  %12s\n", \
		   lbl, d1, msec(d2), "-")

static void print_estat(struct seq_file *m, unsigned long now,
			struct kvm_exit_stats *estats)
{
	int i;
	unsigned long total_exit_time = 0;
	unsigned long total_exit_nr = 0;
	unsigned long total_exit_time_in_kvm = 0;

	seq_printf(m, "--------------------------------------------------------\n");
	for (i = 0; i < TRAP_MAX; i++) {
		print_rec(m, trap_stat_names[i],
			  estats->trap_exit_nr[i],
			  estats->trap_exit_time[i],
			  estats->trap_exit_time_in_kvm[i]);

		total_exit_time += estats->trap_exit_time[i];
		total_exit_nr += estats->trap_exit_nr[i];
		total_exit_time_in_kvm += estats->trap_exit_time_in_kvm[i];
	}
	seq_printf(m, "\n");

	print_rec(m, "Total switch", 0LU,
		   estats->switch_time, estats->switch_time_in_kvm);
	print_rec(m, "Total host", total_exit_nr,
		   total_exit_time, total_exit_time_in_kvm);
	print_rec2(m, "Total EL2", 0LU, estats->total_el2);
	print_rec2(m, "Total guest", 0LU, estats->total_guest);
	seq_printf(m, "\n");
	print_rec2(m, "Sched out", estats->total_sched_out_nr,
		   estats->total_sched_out_time);
	print_rec2(m, "Host IRQ",
		   estats->total_irq_nr,
		   estats->total_irq_time);
	print_rec2(m, "Since reset", 0LU, now - estats->reset_time);
	seq_printf(m, "--------------------------------------------------------\n");

}

static void summarize_stats(struct kvm_exit_stats *s,
			    struct kvm_exit_stats *e,
			    unsigned long now)
{
	int i;

	for (i = 0; i < TRAP_MAX; i++) {
		s->trap_exit_time[i] += e->trap_exit_time[i];
		s->trap_exit_nr[i] += e->trap_exit_nr[i];
		s->trap_exit_time_in_kvm[i] += e->trap_exit_time_in_kvm[i];
	}
	s->total_el2 += e->total_el2;
	s->total_irq_time += e->total_irq_time;
	s->total_irq_nr += e->total_irq_nr;
	s->total_sched_out_time += e->total_sched_out_time;
	s->total_sched_out_nr += e->total_sched_out_nr;
	s->total_guest += e->total_guest;
	s->switch_time += e->switch_time;
	s->switch_time_in_kvm += e->switch_time_in_kvm;

	/* Fixup summary reset time to account for total CPU time */
	if (!s->reset_time)
		s->reset_time = e->reset_time;
	else
		s->reset_time -= now - e->reset_time;
}

static void adjust_estat_for_sleep(unsigned long now, bool add,
				   struct kvm_exit_stats *estats)
{
	struct kvm_exit_data *new = estats->new_edata;
	unsigned long sleep_time = 0;
	unsigned long extra_sched_out = 0;

	/*
	 * Handle sleeping VCPUs time keeping
	 * (the summary will never have in_handle_exit set)
	 */
	if (estats->in_handle_exit && new->trap_reason == TRAP_WFI) {
		sleep_time = now - new->exit_el2;

		if (new->sched_out_start)
			extra_sched_out = now - new->sched_out_start;
	}

	if (add) {
		estats->trap_exit_time[TRAP_WFI] += sleep_time;
		estats->total_sched_out_time += extra_sched_out;
	} else {
		estats->trap_exit_time[TRAP_WFI] -= sleep_time;
		estats->total_sched_out_time -= extra_sched_out;
	}
}

static int stats_fs_show(struct seq_file *m, void *v)
{
	struct kvm_exit_stats *estats, *summary;
	struct kvm *kvm;
	int vmid = 0;
	unsigned long now = kvm_arm_read_pcounter();
	char hdr[64];

	summary = kzalloc(sizeof(*summary), GFP_KERNEL);

	spin_lock(&kvm_lock);
	list_for_each_entry(kvm, &vm_list, vm_list) {
		struct kvm_vcpu *vcpu;
		int i;

		kvm_for_each_vcpu(i, vcpu, kvm) {
			estats = &vcpu->stat.exit_stats;

			adjust_estat_for_sleep(now, true, estats);

			snprintf(hdr, sizeof(hdr),
				 "VM %d VCPU %d", vmid, vcpu->vcpu_id);
			print_hdr(m, hdr, "Nr", "msec", "In-KVM");

			print_estat(m, now, estats);
			summarize_stats(summary, estats, now);

			adjust_estat_for_sleep(now, false, estats);

			seq_printf(m, "\n");
		}

		snprintf(hdr, sizeof(hdr),
			 "VM %d All VCPUs", vmid);
		print_hdr(m, hdr, "Nr", "msecs", "In-KVM");
		print_estat(m, now, summary);
		seq_printf(m, "\n");

		vmid++;
	}
	spin_unlock(&kvm_lock);

	kfree(summary);

	return 0;
};

static int stats_fs_open(struct inode *inode, struct file *file)
{
	return single_open(file, stats_fs_show, NULL);
}

static const struct file_operations stats_fs_fops = {
	.owner = THIS_MODULE,
	.open = stats_fs_open,
	.read = seq_read,
	.write = stats_fs_write,
};

void kvm_init_trap_stats(void)
{
	struct dentry *dentry;

	arch_timer_rate = arch_timer_get_rate();

	dentry = debugfs_create_file("exit_stats", 0666, kvm_debugfs_dir,
				     NULL, &stats_fs_fops);
	if (!dentry)
		kvm_err("error creating debugfs dentry");
	else
		kvm_info("stats module up and running (arch timer rate: %lu)",
			 arch_timer_rate);
}
