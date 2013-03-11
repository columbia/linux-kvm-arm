#ifndef GUEST_H
#define GUEST_H
#include <stdint.h>
#include <stdbool.h>
#include "guest-util.h"

void ok(void);
void fail(void);
void putc(char c);
extern int skip_undef, undef_count;

static inline void print(const char *p)
{
	while (*p)
		putc(*(p++));
}

#ifndef NULL
#define NULL ((void *)0)
#endif

#define stringify(expr)		stringify_1(expr)
/* Double-indirection required to stringify expansions */
#define stringify_1(expr)	#expr

#define assert(cond)							\
	do {								\
		if (!(cond)) {						\
			print(__FILE__ ":" stringify(__LINE__) ":"	\
			      stringify(cond) ": FAILED\n");		\
			fail();						\
		} else							\
			ok();						\
	} while(0)

typedef uint32_t u32;


static inline unsigned char readb(unsigned long addr)
{
	unsigned char out;
	asm volatile("ldrb %0, [%1]" : "=r"(out) : "r"(addr));
	return out;
}

static inline unsigned long readl(unsigned long addr)
{
	unsigned char out;
	asm volatile("ldr %0, [%1]" : "=r"(out) : "r"(addr));
	return out;
}

static inline void writeb(unsigned long addr, unsigned char val)
{
	asm volatile("strb %0, [%1]" : : "r"(val), "r"(addr));
}

static inline void writel(unsigned long addr, unsigned long val)
{
	asm volatile("str %0, [%1]" : : "r"(val), "r"(addr));
}

/* Each guest needs to write this. */
int test(int smp_cpus, int vgic_enabled);

/* Secondary CPUs write to this value */
extern volatile bool second_cpu_up;
extern volatile bool first_cpu_ack;

static inline void dmb(void)
{
	asm volatile ("dmb");
}

static inline void dsb(void)
{
	asm volatile ("dsb");
}

static inline void isb(void)
{
	asm volatile ("isb");
}

static inline void clean_cache(void *addr)
{
	unsigned long reg = (unsigned long)addr;
	asm volatile("mcr p15, 0, %[reg], c7, c14, 1": : [reg] "r" (reg));
	isb();
}

static inline void cpu_wfi(void)
{
	asm volatile("wfi");
}

static inline void cpu_wfe(void)
{
	asm volatile("wfe");
}

static inline void cpu_sev(void)
{
	asm volatile("sev");
}

#endif /* GUEST_H */
