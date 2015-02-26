struct virt_test {
	char *name;
	unsigned long (*test_fn)(void);
};

extern volatile int cpu1_ipi_ack;
void init_virt_test(void);
