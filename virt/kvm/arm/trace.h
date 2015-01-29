#if !defined(_TRACE_KVM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_KVM_H

#include <linux/tracepoint.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM kvm

/*
 * Tracepoints for arch_timer
 */
TRACE_EVENT(kvm_timer_inject_irq,
	TP_PROTO(unsigned long vcpu_id, __u32 irq),
	TP_ARGS(vcpu_id, irq),

	TP_STRUCT__entry(
		__field(	unsigned long,	vcpu_id	)
		__field(	__u32,		irq		)
	),

	TP_fast_assign(
		__entry->vcpu_id	= vcpu_id;
		__entry->irq		= irq;
	),

	TP_printk("VCPU: %ld, IRQ %d", __entry->vcpu_id, __entry->irq)
);

/*
 * Tracepoints for VGIC
 */
TRACE_EVENT(vgic_update_irq_pending,
	TP_PROTO(int cpuid, unsigned int irq_num, bool level),
	TP_ARGS(cpuid, irq_num, level),

	TP_STRUCT__entry(
		__field(	  int,  cpuid		)
		__field( unsigned int,	irq_num	)
		__field(	 bool,	level		)
	),

	TP_fast_assign(
		__entry->cpuid		= cpuid;
		__entry->irq_num	= irq_num;
		__entry->level		= level;
	),

	TP_printk("Inject IRQ%d (%s) to CPU %d", __entry->irq_num,
		__entry->level?"true":"false", __entry->cpuid)
);

TRACE_EVENT(vgic_process_maintence,
	TP_PROTO(struct vgic_lr vlr),
	TP_ARGS(vlr),

	TP_STRUCT__entry(
		__field(u16, irq)
		__field(u8, source)
		__field(u8, state)
	),

	TP_fast_assign(
		__entry->irq	= vlr.irq;
		__entry->source = vlr.source;
		__entry->state	= vlr.state;
	),

	TP_printk("irq:%d, source:%d, state=0x%x", __entry->irq,
		__entry->source, __entry->state)
);

TRACE_EVENT(vgic_update_state,
	TP_PROTO(unsigned long bm),
	TP_ARGS(bm),

	TP_STRUCT__entry(
		__field(unsigned long, bm)
	),

	TP_fast_assign(
		__entry->bm	= bm;
	),

	TP_printk("bitmap: %lu", __entry->bm)
);

TRACE_EVENT(vgic_dispatch_sgi,
	TP_PROTO(int irq, int source, u8 target_cpus),
	TP_ARGS(irq, source, target_cpus),

	TP_STRUCT__entry(
		__field(int, irq)
		__field(int, source)
		__field(u8, target_cpus)
	),

	TP_fast_assign(
		__entry->irq		= irq;
		__entry->source		= source;
		__entry->target_cpus	= target_cpus;
	),

	TP_printk("IRQ: %d, source: %d, target_cpus: 0x%x\n",
		  __entry->irq, __entry->source, __entry->target_cpus)
);


#endif /* _TRACE_KVM_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../../virt/kvm/arm
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace

/* This part must be outside protection */
#include <trace/define_trace.h>
