/*
 * Based on the x86 implementation.
 *
 * Copyright (C) 2012 ARM Ltd.
 * Author: Marc Zyngier <marc.zyngier@arm.com>
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
 */

#include <linux/perf_event.h>
#include <linux/kvm_host.h>

#include <asm/kvm_emulate.h>
#include <asm/stacktrace.h>

#define PAR_PA_SHIFT	12
#define PAR_PA_MASK	(((1ULL << 36) - 1) << PAR_PA_SHIFT)

static int read_guest_va(struct kvm_vcpu *vcpu,
			 unsigned long gva,
			 unsigned long *data)
{
	phys_addr_t gpa;
	int ret;

	/* 32-bit mode not supported by __kvm_gva_to_gpa yet */
	BUG_ON(vcpu_mode_is_32bit(vcpu));

	gpa = kvm_call_hyp(__kvm_gva_to_gpa, vcpu->kvm, vcpu, gva);

	if (gpa & 1)
		return -EFAULT;

	gpa &= PAR_PA_MASK;
	gpa |= gva & ((1 << 12) - 1);
	ret = kvm_read_guest(vcpu->kvm, gpa, data, sizeof(unsigned long));
	return ret;
}

/*
 * AArch64 PCS assigns the frame pointer to x29.
 *
 * A simple function prologue looks like this:
 * 	sub	sp, sp, #0x10
 *   	stp	x29, x30, [sp]
 *	mov	x29, sp
 *
 * A simple function epilogue looks like this:
 *	mov	sp, x29
 *	ldp	x29, x30, [sp]
 *	add	sp, sp, #0x10
 *
 * We assume that the guest being profiled follows the aarch64 PCS.
 */
static int notrace kvm_unwind_frame(struct kvm_vcpu *vcpu,
				    struct stackframe *frame)
{
	unsigned long high, low;
	unsigned long fp = frame->fp;
	int ret;

	trace_printk("KVM unwind at: 0x%lx\n", fp);

	low  = frame->sp;
	high = ALIGN(low, THREAD_SIZE);

	if (fp < low || fp > high - 0x18 || fp & 0xf)
		return -EINVAL;

	frame->sp = fp + 0x10;

	ret = read_guest_va(vcpu, fp, &frame->fp);
	if (ret)
		return -EFAULT;

	ret = read_guest_va(vcpu, fp + 8, &frame->pc);
	if (ret)
		return -EFAULT;

	/*
	 * -4 here because we care about the PC at time of bl,
	 * not where the return will go.
	 */
	frame->pc -= 4;

	trace_printk("KVM new frame:\n  fp: 0x%lx\n  pc: %lx\n", frame->fp, frame->pc);

	return 0;
}

/*
 * Gets called by walk_stackframe() for every stackframe. This will be called
 * whist unwinding the stackframe and is like a subroutine return so we use
 * the PC.
 */
static void callchain_trace(struct stackframe *frame, void *data)
{
	struct perf_callchain_entry *entry = data;
	perf_callchain_store(entry, frame->pc);
}

static void notrace kvm_walk_stackframe(struct kvm_vcpu *vcpu,
					struct stackframe *frame, void *data)
{
	while (1) {
		int ret;

		callchain_trace(frame, data);

		ret = kvm_unwind_frame(vcpu, frame);
		if (ret < 0)
			break;
	}
}

static void kvm_callchain_kernel(struct perf_callchain_entry *entry)
{
	struct kvm_vcpu *vcpu;
	struct stackframe frame;

	vcpu = kvm_arm_get_running_vcpu();
	if (WARN_ON(!vcpu))
		return;

	/* 32-bit guests not supported yet */
	if (vcpu_mode_is_32bit(vcpu))
		return;

	frame.fp = *vcpu_reg(vcpu, 29);
	frame.sp = vcpu_gp_regs(vcpu)->sp_el1;
	frame.pc = *vcpu_pc(vcpu);

	kvm_walk_stackframe(vcpu, &frame, entry);
}

static int kvm_is_in_guest(void)
{
        return kvm_arm_get_running_vcpu() != NULL;
}

static int kvm_is_user_mode(void)
{
	struct kvm_vcpu *vcpu;

	vcpu = kvm_arm_get_running_vcpu();

	if (vcpu)
		return !vcpu_mode_priv(vcpu);

	return 0;
}

static unsigned long kvm_get_guest_ip(void)
{
	struct kvm_vcpu *vcpu;

	vcpu = kvm_arm_get_running_vcpu();

	if (vcpu)
		return *vcpu_pc(vcpu);

	return 0;
}

static struct perf_guest_info_callbacks kvm_guest_cbs = {
	.is_in_guest	= kvm_is_in_guest,
	.is_user_mode	= kvm_is_user_mode,
	.get_guest_ip	= kvm_get_guest_ip,
	.callchain_kernel = kvm_callchain_kernel,
};

int kvm_perf_init(void)
{
	return perf_register_guest_info_callbacks(&kvm_guest_cbs);
}

int kvm_perf_teardown(void)
{
	return perf_unregister_guest_info_callbacks(&kvm_guest_cbs);
}
