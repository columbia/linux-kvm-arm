/*
 * Copyright (C) 2012,2013 - ARM Ltd
 * Author: Marc Zyngier <marc.zyngier@arm.com>
 *
 * Derived from arch/arm/kvm/handle_exit.c:
 * Copyright (C) 2012 - Virtual Open Systems and Columbia University
 * Author: Christoffer Dall <c.dall@virtualopensystems.com>
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

#include <linux/kvm.h>
#include <linux/kvm_host.h>
#include <asm/kvm_emulate.h>
#include <asm/kvm_coproc.h>
#include <asm/kvm_mmu.h>
#include <asm/kvm_psci.h>

typedef int (*exit_handle_fn)(struct kvm_vcpu *, struct kvm_run *);

#ifdef CONFIG_KVM_ARM_WS_PROFILE
static const char* wsstat_names[WSSTAT_NR] = {
	[WSSTAT_VGIC_LIST_REG] = "vgic_list_regs",
	[WSSTAT_VGIC_HCR_INT] = "vgic_hcr_int",
	[WSSTAT_VGIC_OTHER] = "vgic_other",
	[WSSTAT_TIMER] = "timer",
	[WSSTAT_HOST_GPREGS] = "host_gpregs",
	[WSSTAT_GUEST_GPREGS] = "guest_gpregs",
	[WSSTAT_HOST_FPREGS] = "host_fpregs",
	[WSSTAT_GUEST_FPREGS] = "guest_fpregs",
	[WSSTAT_HOST_SYSREGS] = "host_sysregs",
	[WSSTAT_GUEST_SYSREGS] = "guest_sysregs",
	[WSSTAT_HOST_DEBUGREGS] = "host_debugregs",
	[WSSTAT_GUEST_DEBUGREGS] = "guest_debugregs",
	[WSSTAT_VMCONFIG] = "vmconfig",

};

static void print_kvm_vcpu_stats(struct kvm_vcpu *vcpu)
{
	int i;
	
	for (i = 0; i < WSSTAT_NR; i++) {
		trace_printk("%s_save_cc: %lu\n",
			     wsstat_names[i], vcpu->stat.ws_stat_save[i]);
		trace_printk("%s_restore_cc: %lu\n",
			     wsstat_names[i], vcpu->stat.ws_stat_restore[i]);
	}
}
#endif

static volatile bool kvm_vmswitch_ping_sent = false;
static DECLARE_WAIT_QUEUE_HEAD(vmswitch_queue);

/* Used for counting vm switch time - hackish and racy I know */
static unsigned long cc_before;

#define HVC_NOOP		0x4b000000
#define HVC_CCNT_ENABLE		0x4b000001
#define HVC_VMSWITCH_SEND	0x4b000010
#define HVC_VMSWITCH_RCV	0x4b000020
#define HVC_VMSWITCH_DONE	0x4b000030
static int handle_hvc(struct kvm_vcpu *vcpu, struct kvm_run *run)
{
	int ret;

	/*
	 * Enable cycle counter for Xen - we choose to be compatible but rely
	 * on running measurement guests under perf on the KVM host.
	 */
	if (*vcpu_reg(vcpu, 0) == HVC_CCNT_ENABLE) {
		return 1;
	}

	/* NOOP hvc call to measure hypercall turn-around time */
	if (*vcpu_reg(vcpu, 0) == HVC_NOOP) {
#ifdef CONFIG_KVM_ARM_WS_PROFILE
		print_kvm_vcpu_stats(vcpu);
#endif
		return 1;
	}

	/* Measure VM switching time */
	if (*vcpu_reg(vcpu, 0) == HVC_VMSWITCH_SEND) {
		/* Report error to guest if we have no waiting receiver */
		if (!waitqueue_active(&vmswitch_queue)) {
			*vcpu_reg(vcpu, 0) = -EAGAIN;
			return 1;
		}

		cc_before = *vcpu_reg(vcpu, 1);
		kvm_vmswitch_ping_sent = true;
		smp_mb();
		wake_up(&vmswitch_queue);
		wait_event_interruptible(vmswitch_queue, !kvm_vmswitch_ping_sent);
		*vcpu_reg(vcpu, 0) = 0;
		return 1;
	}

	/* Measure VM switching time */
	if (*vcpu_reg(vcpu, 0) == HVC_VMSWITCH_RCV) {
		/* Assume we have one other VM running */
		wait_event_interruptible(vmswitch_queue, kvm_vmswitch_ping_sent);
		kvm_vmswitch_ping_sent = false;
		*vcpu_reg(vcpu, 0) = cc_before;
		return 1;
	}

	if (*vcpu_reg(vcpu, 0) == HVC_VMSWITCH_DONE) {
		/* Assume we have one other VM running */
		wake_up_all(&vmswitch_queue);
		return 1;
	}

	ret = kvm_psci_call(vcpu);
	if (ret < 0) {
		kvm_err("injecting undefined exception on hvc call with reg0: 0x%lx\n",
			*vcpu_reg(vcpu, 0));
		kvm_inject_undefined(vcpu);
		return 1;
	}

	return ret;
}

static int handle_smc(struct kvm_vcpu *vcpu, struct kvm_run *run)
{
	kvm_inject_undefined(vcpu);
	return 1;
}

/**
 * kvm_handle_wfx - handle a wait-for-interrupts or wait-for-event
 *		    instruction executed by a guest
 *
 * @vcpu:	the vcpu pointer
 *
 * WFE: Yield the CPU and come back to this vcpu when the scheduler
 * decides to.
 * WFI: Simply call kvm_vcpu_block(), which will halt execution of
 * world-switches and schedule other host processes until there is an
 * incoming IRQ or FIQ to the VM.
 */
static int kvm_handle_wfx(struct kvm_vcpu *vcpu, struct kvm_run *run)
{
	if (kvm_vcpu_get_hsr(vcpu) & ESR_EL2_EC_WFI_ISS_WFE)
		kvm_vcpu_on_spin(vcpu);
	else
		kvm_vcpu_block(vcpu);

	kvm_skip_instr(vcpu, kvm_vcpu_trap_il_is32bit(vcpu));

	return 1;
}

static exit_handle_fn arm_exit_handlers[] = {
	[ESR_EL2_EC_WFI]	= kvm_handle_wfx,
	[ESR_EL2_EC_CP15_32]	= kvm_handle_cp15_32,
	[ESR_EL2_EC_CP15_64]	= kvm_handle_cp15_64,
	[ESR_EL2_EC_CP14_MR]	= kvm_handle_cp14_32,
	[ESR_EL2_EC_CP14_LS]	= kvm_handle_cp14_load_store,
	[ESR_EL2_EC_CP14_64]	= kvm_handle_cp14_64,
	[ESR_EL2_EC_HVC32]	= handle_hvc,
	[ESR_EL2_EC_SMC32]	= handle_smc,
	[ESR_EL2_EC_HVC64]	= handle_hvc,
	[ESR_EL2_EC_SMC64]	= handle_smc,
	[ESR_EL2_EC_SYS64]	= kvm_handle_sys_reg,
	[ESR_EL2_EC_IABT]	= kvm_handle_guest_abort,
	[ESR_EL2_EC_DABT]	= kvm_handle_guest_abort,
};

static exit_handle_fn kvm_get_exit_handler(struct kvm_vcpu *vcpu)
{
	u8 hsr_ec = kvm_vcpu_trap_get_class(vcpu);

	if (hsr_ec >= ARRAY_SIZE(arm_exit_handlers) ||
	    !arm_exit_handlers[hsr_ec]) {
		kvm_err("Unknown exception class: hsr: %#08x\n",
			(unsigned int)kvm_vcpu_get_hsr(vcpu));
		BUG();
	}

	return arm_exit_handlers[hsr_ec];
}

/*
 * Return > 0 to return to guest, < 0 on error, 0 (and set exit_reason) on
 * proper exit to userspace.
 */
int handle_exit(struct kvm_vcpu *vcpu, struct kvm_run *run,
		       int exception_index)
{
	exit_handle_fn exit_handler;

	switch (exception_index) {
	case ARM_EXCEPTION_IRQ:
		return 1;
	case ARM_EXCEPTION_TRAP:
		/*
		 * See ARM ARM B1.14.1: "Hyp traps on instructions
		 * that fail their condition code check"
		 */
		if (!kvm_condition_valid(vcpu)) {
			kvm_skip_instr(vcpu, kvm_vcpu_trap_il_is32bit(vcpu));
			return 1;
		}

		exit_handler = kvm_get_exit_handler(vcpu);

		return exit_handler(vcpu, run);
	default:
		kvm_pr_unimpl("Unsupported exception type: %d",
			      exception_index);
		run->exit_reason = KVM_EXIT_INTERNAL_ERROR;
		return 0;
	}
}
