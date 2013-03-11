#include "guest.h"
#include "vfp_test.h"

#define FPEXC_EN		(1 << 30)

static int get_fpexc(void)
{
	int fpexc;

	asm("mrc p10, 7, %0, cr8, cr0, 0" : "=r"(fpexc));
	return fpexc;
}

static void set_fpexc(int fpexc)
{
	asm("mcr p10, 7, %0, cr8, cr0, 0" : : "r"(fpexc));
}

static void turn_on_coproc_access(void)
{
	int cpacr;

	asm volatile("mrc p15, 0, %0, c1, c0, 2" : "=r" (cpacr));

	/* VFP is coprocessors 10 and 11: both bits means all accessible. */
	cpacr |= (0xF << 20);

	asm volatile("mcr p15, 0, %0, c1, c0, 2" : : "r" (cpacr));
}

int test(int smp_cpus, int vgic_enabled)
{
	double d1, d2, d3;
	register double d0 asm("d0");
	register double d16 asm("d16");
	int val;

	print("Turning on CP10/11 access\n");
	turn_on_coproc_access();

	print("Getting FP\n");
	val = get_fpexc();
	assert(!(val & FPEXC_EN));

	print("Enabling FP\n");
	set_fpexc(get_fpexc() | FPEXC_EN);

	print("Basic floating point test\n");
	d1 = 1.0 / 8;
	d2 = 1.0 / 16;
	d3 = d1 + d2;

	assert(d3 == 3.0 / 16);

	/* Now, try loading 2.0 and make sure host doesn't interfere! */
	d0 = 2.0;
	assert(d0 == 2.0);
	asm volatile("ldr %0, [%1]" : "=r"(val) : "r"(VFP_USE_REG));
	assert(d0 == 2.0);

	/* Same thing with upper 16 registers. */
	d16 = 2.0;
	assert(d16 == 2.0);
	asm volatile("ldr %0, [%1]" : "=r"(val) : "r"(VFP_USE_REG + 16));
	assert(d16 == 2.0);

	/* Now check host ioctl sees register correctly. */
	d0 = 2.0;
 	asm volatile("ldr %0, [%1]" : "=r"(val) : "r"(VFP_CHECK_REG));

	d0 = 2.0;
 	asm volatile("ldr %0, [%1]" : "=r"(val) : "r"(VFP_SET_REG));
	assert(d0 == 3.0);

	d16 = 2.0;
 	asm volatile("ldr %0, [%1]" : "=r"(val) : "r"(VFP_CHECK_REG+16));

	d16 = 2.0;
 	asm volatile("ldr %0, [%1]" : "=r"(val) : "r"(VFP_SET_REG+16));
	assert(d16 == 3.0);

	return 0;
}
