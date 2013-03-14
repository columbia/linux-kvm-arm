//#define DEBUG 1

#include "guest.h"
#include "vmexit.h"
#include "guest.h"

__asm__(".arch_extension	virt");

volatile bool second_cpu_up;
volatile bool first_cpu_ack;
volatile bool ipi_ack;
volatile bool ipi_ready;


void smp_init(void)
{
	second_cpu_up = true;
}

void smp_test(int cpu)
{
	debug("core[%u] smp irq\n", cpu);
}

void smp_interrupt(void)
{
	unsigned long ack, eoi;
	int irq, cpu;

	ipi_ack = true;

	debug("core[1]: received interrupt\n");

	ack = readl(VGIC_CPU_BASE + GICC_IAR);
	irq = IAR_IRQID(ack);
	cpu = IAR_CPUID(ack);

	debug("core[1]: received interrupt %u (src_cpu %u)\n", irq, cpu);

	if (irq <= 15) {
		/* SGI */
		//assert(cpu == 0);
	} else {
		printf("unknown IRQID: %u\n", irq);
		fail();
	}

	/* EOI the interrupt */
	eoi = MK_EOIR(cpu, irq);
	writel(VGIC_CPU_BASE + GICC_EOIR, eoi);
	dsb();
	dmb();

	ipi_ready = true;
}

void smp_gic_enable(int smp_cpus, int vgic_enabled)
{
	assert(smp_cpus >= 2);

	if (!vgic_enabled)
		return;

	writel(VGIC_CPU_BASE + GICC_CTLR, 0x1); /* enable cpu interface */
	writel(VGIC_CPU_BASE + GICC_PMR, 0xff);	/* unmask all irq priorities */

	dsb();
	dmb();
	isb();

	debug("core[1]: gic ready\n");

	while (!first_cpu_ack);

	debug("core[1]: first cpu acked\n");
	ipi_ready = true;
}
