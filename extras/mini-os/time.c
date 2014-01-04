void arch_init_time(void);
void arch_fini_time(void);

void init_time(void)
{
	arch_init_time();
}

void fini_time(void)
{
	arch_fini_time();
}
