#include <console.h>
#include <arm/arch_mm.h>

#define PHYS_START (0x80008000 + (1000 * 4 * 1024))
#define PHYS_SIZE (40*1024*1024)

static void build_pagetable(unsigned long *start_pfn, unsigned long *max_pfn)
{
	// FIXME Create small pages descriptors here instead of the 1M superpages created earlier.
	return;
}

unsigned long allocate_ondemand(unsigned long n, unsigned long alignment)
{
	// FIXME
	BUG();
}

void arch_init_mm(unsigned long* start_pfn_p, unsigned long* max_pfn_p)
{
	printk("    _text: %p(VA)\n", &_text);
	printk("    _etext: %p(VA)\n", &_etext);
	printk("    _erodata: %p(VA)\n", &_erodata);
	printk("    _edata: %p(VA)\n", &_edata);
	printk("    stack start: %p(VA)\n", stack);
	printk("    _end: %p(VA)\n", &_end);

	// FIXME Get from dt!
	*start_pfn_p = (((unsigned long)&_end) >> PAGE_SHIFT) + 1000;
	*max_pfn_p = ((unsigned long)&_end + PHYS_SIZE) >> PAGE_SHIFT;

	printk("    start_pfn: %lx\n", *start_pfn_p);
	printk("    max_pfn: %lx\n", *max_pfn_p);

	build_pagetable(start_pfn_p, max_pfn_p);
}

void arch_init_p2m(unsigned long max_pfn)
{
}

void arch_init_demand_mapping_area(unsigned long cur_pfn)
{
}
