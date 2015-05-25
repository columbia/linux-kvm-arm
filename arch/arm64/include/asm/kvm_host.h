/*
 * Copyright (C) 2012,2013 - ARM Ltd
 * Author: Marc Zyngier <marc.zyngier@arm.com>
 *
 * Derived from arch/arm/include/asm/kvm_host.h:
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

#ifndef __ARM64_KVM_HOST_H__
#define __ARM64_KVM_HOST_H__

#include <linux/types.h>
#include <linux/kvm_types.h>
#include <asm/kvm.h>
#include <asm/kvm_asm.h>
#include <asm/kvm_mmio.h>

#define __KVM_HAVE_ARCH_INTC_INITIALIZED

#if defined(CONFIG_KVM_ARM_MAX_VCPUS)
#define KVM_MAX_VCPUS CONFIG_KVM_ARM_MAX_VCPUS
#else
#define KVM_MAX_VCPUS 0
#endif

#define KVM_USER_MEM_SLOTS 32
#define KVM_PRIVATE_MEM_SLOTS 4
#define KVM_COALESCED_MMIO_PAGE_OFFSET 1

#include <kvm/arm_vgic.h>
#include <kvm/arm_arch_timer.h>

#define KVM_VCPU_MAX_FEATURES 3

int __attribute_const__ kvm_target_cpu(void);
int kvm_reset_vcpu(struct kvm_vcpu *vcpu);
int kvm_arch_dev_ioctl_check_extension(long ext);

struct kvm_arch {
	/* The VMID generation used for the virt. memory system */
	u64    vmid_gen;
	u32    vmid;

	/* 1-level 2nd stage table and lock */
	spinlock_t pgd_lock;
	pgd_t *pgd;

	/* VTTBR value associated with above pgd and vmid */
	u64    vttbr;

	/* The maximum number of vCPUs depends on the used GIC model */
	int max_vcpus;

	/* Interrupt controller */
	struct vgic_dist	vgic;

	/* Timer */
	struct arch_timer_kvm	timer;
};

#define KVM_NR_MEM_OBJS     40

/*
 * We don't want allocation failures within the mmu code, so we preallocate
 * enough memory for a single page fault in a cache.
 */
struct kvm_mmu_memory_cache {
	int nobjs;
	void *objects[KVM_NR_MEM_OBJS];
};

struct kvm_vcpu_fault_info {
	u32 esr_el2;		/* Hyp Syndrom Register */
	u64 far_el2;		/* Hyp Fault Address Register */
	u64 hpfar_el2;		/* Hyp IPA Fault Address Register */
};

struct kvm_cpu_context {
	struct kvm_regs	gp_regs;
	union {
		u64 sys_regs[NR_SYS_REGS];
		u32 copro[NR_COPRO_REGS];
	};
};

typedef struct kvm_cpu_context kvm_cpu_context_t;

struct kvm_vcpu_arch {
	struct kvm_cpu_context ctxt;

	/* HYP configuration */
	u64 hcr_el2;

	/* Exception Information */
	struct kvm_vcpu_fault_info fault;

	/* Debug state */
	u64 debug_flags;

	/* Pointer to host CPU context */
	kvm_cpu_context_t *host_cpu_context;

	/* VGIC state */
	struct vgic_cpu vgic_cpu;
	struct arch_timer_cpu timer_cpu;

	/*
	 * Anything that is not used directly from assembly code goes
	 * here.
	 */

	/* Don't run the guest */
	bool pause;

	/* IO related fields */
	struct kvm_decode mmio_decode;

	/* Interrupt related fields */
	u64 irq_lines;		/* IRQ and FIQ levels */

	/* Cache some mmu pages needed inside spinlock regions */
	struct kvm_mmu_memory_cache mmu_page_cache;

	/* Target CPU and feature flags */
	int target;
	DECLARE_BITMAP(features, KVM_VCPU_MAX_FEATURES);

	/* Detect first run of a vcpu */
	bool has_run_once;
};

#define vcpu_gp_regs(v)		(&(v)->arch.ctxt.gp_regs)
#define vcpu_sys_reg(v,r)	((v)->arch.ctxt.sys_regs[(r)])
/*
 * CP14 and CP15 live in the same array, as they are backed by the
 * same system registers.
 */
#define vcpu_cp14(v,r)		((v)->arch.ctxt.copro[(r)])
#define vcpu_cp15(v,r)		((v)->arch.ctxt.copro[(r)])

#ifdef CONFIG_CPU_BIG_ENDIAN
#define vcpu_cp15_64_high(v,r)	vcpu_cp15((v),(r))
#define vcpu_cp15_64_low(v,r)	vcpu_cp15((v),(r) + 1)
#else
#define vcpu_cp15_64_high(v,r)	vcpu_cp15((v),(r) + 1)
#define vcpu_cp15_64_low(v,r)	vcpu_cp15((v),(r))
#endif

struct kvm_vm_stat {
	u32 remote_tlb_flush;
};

/* Various defines for the exit trap framework - BEGIN */

enum trap_reason {
	TRAP_HVC = 0,
	TRAP_WFE,
	TRAP_WFI,
	TRAP_IO_KERNEL,
	TRAP_IO_USER,
	TRAP_IRQ,
	TRAP_MAX /* For array sizing */
};

static inline unsigned long kvm_arm_read_pcounter(void)
{
	unsigned long val;

	asm volatile(
		"isb\n"
		"mrs %0, CNTPCT_EL0\n"
		"isb\n"
		: [reg] "=r" (val));
	return val;
}

extern void kvm_init_trap_stats(void);
extern void kvm_vcpu_init_trap_stats(struct kvm_vcpu *vcpu);
extern void kvm_trap_stat_set_exit_reason(struct kvm_vcpu *vcpu,
					  int exit_reason);
extern void kvm_stat_new_loop(struct kvm_vcpu *vcpu);
extern void kvm_stat_enter_guest(struct kvm_vcpu *vcpu);
extern void kvm_stat_exit_guest(struct kvm_vcpu *vcpu);
extern void kvm_stat_handle_exit_begin(struct kvm_vcpu *vcpu);
extern void kvm_stat_handle_exit_end(struct kvm_vcpu *vcpu);

struct kvm_exit_data {
	enum trap_reason trap_reason;

	/* Chronological data points for an exit */

	unsigned long exit_el2;		/* just trapped from guest */
	unsigned long exit_el1;		/* just trapped from el2 */

	/* Adjustments for IRQs happening */
	unsigned long el1_irq_start;	/* holding variable, DNR */
	unsigned long el1_irq_time;
	unsigned long el1_irq_no_handle_exit_time; /* to calc switch time */
	unsigned long el1_irq_nr;

	/* Adjustments for scheding Overhead */
	unsigned long sched_out_start;	/* holding variable, DNR */
	unsigned long sched_out_time;
	unsigned long sched_out_nr;

	unsigned long exit_el1_handle_exit;  /* reached handle_exit */

	unsigned long entry_el1_loop_start;  /* start of run loop */
	unsigned long entry_el1;	     /* about to trap to el2 */
	unsigned long entry_el2;	     /* about to trap to guest */
};

struct kvm_exit_stats {
	unsigned long trap_exit_time[TRAP_MAX]; /* time spent */
	unsigned long trap_exit_nr[TRAP_MAX];   /* number of times seen */
	unsigned long trap_exit_time_in_kvm[TRAP_MAX]; /* time - (irq+sched) */
	unsigned long total_el2;
	unsigned long total_guest;
	unsigned long switch_time; /* exit -> handle_exit -> loop -> entry */
	unsigned long switch_time_in_kvm; /* less irq + sched */

	unsigned long total_irq_time;
	unsigned long total_irq_nr;

	unsigned long total_sched_out_time;
	unsigned long total_sched_out_nr;

	unsigned long reset_time;

	/*
	 * We want to measure the full time spent outside of the VCPU and
	 * attribute this to the individual exit reasons, but we don't have
	 * the full information before we exited, and re-entered the CPU and
	 * we cannot report the data in the middle of EL2.
	 *
	 * Therefore, we keep two versions of the exit data around and store
	 * data points on the new_edata pointer and switch new with prev when
	 * we have all the observations and are about to enter the guest,
	 * leaving us with a fresh new copy for new_edata.
	 */
	struct kvm_exit_data edata1;
	struct kvm_exit_data edata2;
	struct kvm_exit_data *new_edata;
	struct kvm_exit_data *prev_edata;

	bool in_handle_exit;
};

/* Various defines for the exit trap framework - ENDIT */

struct kvm_vcpu_stat {
	u32 halt_successful_poll;
	u32 halt_wakeup;

	struct kvm_exit_stats exit_stats;
};

int kvm_vcpu_preferred_target(struct kvm_vcpu_init *init);
unsigned long kvm_arm_num_regs(struct kvm_vcpu *vcpu);
int kvm_arm_copy_reg_indices(struct kvm_vcpu *vcpu, u64 __user *indices);
int kvm_arm_get_reg(struct kvm_vcpu *vcpu, const struct kvm_one_reg *reg);
int kvm_arm_set_reg(struct kvm_vcpu *vcpu, const struct kvm_one_reg *reg);

#define KVM_ARCH_WANT_MMU_NOTIFIER
int kvm_unmap_hva(struct kvm *kvm, unsigned long hva);
int kvm_unmap_hva_range(struct kvm *kvm,
			unsigned long start, unsigned long end);
void kvm_set_spte_hva(struct kvm *kvm, unsigned long hva, pte_t pte);
int kvm_age_hva(struct kvm *kvm, unsigned long start, unsigned long end);
int kvm_test_age_hva(struct kvm *kvm, unsigned long hva);

/* We do not have shadow page tables, hence the empty hooks */
static inline void kvm_arch_mmu_notifier_invalidate_page(struct kvm *kvm,
							 unsigned long address)
{
}

struct kvm_vcpu *kvm_arm_get_running_vcpu(void);
struct kvm_vcpu * __percpu *kvm_get_running_vcpus(void);

u64 kvm_call_hyp(void *hypfn, ...);
void force_vm_exit(const cpumask_t *mask);
void kvm_mmu_wp_memory_region(struct kvm *kvm, int slot);

int handle_exit(struct kvm_vcpu *vcpu, struct kvm_run *run,
		int exception_index);

int kvm_perf_init(void);
int kvm_perf_teardown(void);

struct kvm_vcpu *kvm_mpidr_to_vcpu(struct kvm *kvm, unsigned long mpidr);

static inline void __cpu_init_hyp_mode(phys_addr_t boot_pgd_ptr,
				       phys_addr_t pgd_ptr,
				       unsigned long hyp_stack_ptr,
				       unsigned long vector_ptr)
{
	/*
	 * Call initialization code, and switch to the full blown
	 * HYP code.
	 */
	kvm_call_hyp((void *)boot_pgd_ptr, pgd_ptr,
		     hyp_stack_ptr, vector_ptr);
}

struct vgic_sr_vectors {
	void	*save_vgic;
	void	*restore_vgic;
};

static inline void vgic_arch_setup(const struct vgic_params *vgic)
{
	extern struct vgic_sr_vectors __vgic_sr_vectors;

	switch(vgic->type)
	{
	case VGIC_V2:
		__vgic_sr_vectors.save_vgic	= __save_vgic_v2_state;
		__vgic_sr_vectors.restore_vgic	= __restore_vgic_v2_state;
		break;

#ifdef CONFIG_ARM_GIC_V3
	case VGIC_V3:
		__vgic_sr_vectors.save_vgic	= __save_vgic_v3_state;
		__vgic_sr_vectors.restore_vgic	= __restore_vgic_v3_state;
		break;
#endif

	default:
		BUG();
	}
}

static inline void kvm_arch_hardware_disable(void) {}
static inline void kvm_arch_hardware_unsetup(void) {}
static inline void kvm_arch_sync_events(struct kvm *kvm) {}
static inline void kvm_arch_vcpu_uninit(struct kvm_vcpu *vcpu) {}

extern void kvm_arch_sched_in(struct kvm_vcpu *vcpu, int cpu);
extern void kvm_arch_sched_out(struct kvm_vcpu *vcpu);

#endif /* __ARM64_KVM_HOST_H__ */
