struct virt_test {
	char *name;
	unsigned long (*test_fn)(void);
	bool run;
};
