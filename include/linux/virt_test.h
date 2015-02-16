#ifdef CONFIG_ARM64
typedef unsigned long long ccount_t;
#else
typedef unsigned long ccount_t;
#endif

struct virt_test {
	char *name;
	ccount_t (*test_fn)(void);
	bool run;
};

extern volatile int cpu1_ipi_ack;
