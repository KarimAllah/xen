#include <console.h>
#include <arm/arch_mm.h>
#include <libfdt.h>
#include <lib.h>

#define PHYS_START (0x80008000 + (1000 * 4 * 1024))
#define PHYS_SIZE (40*1024*1024)

extern void *device_tree;

static void build_pagetable(unsigned long *start_pfn, unsigned long *max_pfn)
{
	// FIXME Create small pages descriptors here instead of the 1M superpages created earlier.
	return;
}

unsigned long allocate_ondemand(unsigned long n, unsigned long alignment)
{
	// FIXME
	BUG();
	return -1;
}

void arch_init_mm(unsigned long* start_pfn_p, unsigned long* max_pfn_p)
{
	printk("    _text: %p(VA)\n", &_text);
	printk("    _etext: %p(VA)\n", &_etext);
	printk("    _erodata: %p(VA)\n", &_erodata);
	printk("    _edata: %p(VA)\n", &_edata);
	printk("    stack start: %p(VA)\n", stack);
	printk("    _end: %p(VA)\n", &_end);

	*start_pfn_p = 0;
	*max_pfn_p = 0;

	if (fdt_num_mem_rsv(device_tree) != 0)
		printk("WARNING: reserved memory not supported!\n");

        int node = 0;
        int depth = 0;
	for (;;)
        {
                node = fdt_next_node(device_tree, node, &depth);
                if (node <= 0 || depth < 0)
                    break;
		/*
		int name_len = 0;
		const char *name = fdt_get_name(device_tree, node, &name_len);
		printk("Found node: %d (%.*s)\n", node, name_len, name);
		*/

		const char *device_type = fdt_getprop(device_tree, node, "device_type", NULL);
		if (device_type && !strcmp(device_type, "memory"))
		{
			/* Note: we assume there's only a single region of memory.
			 * Since Xen is already translating our "physical"
			 * addresses to the real physical RAM, there's no
			 * reason for it to give us multiple blocks. */
			int len = 0;
			const uint64_t *regs = fdt_getprop(device_tree, node, "reg", &len);
			if (regs == NULL || len != 16) {
				printk("Bad 'reg' property: %p %d\n", regs, len);
				continue;
			}
			unsigned int start = (unsigned int) &_text;
			unsigned int end = (unsigned int) &_end;
			unsigned int mem_base = fdt64_to_cpu(regs[0]);
			unsigned int mem_size = fdt64_to_cpu(regs[1]);
			printk("Found memory at %p (len 0x%x)\n", mem_base, mem_size);

			BUG_ON(mem_base > start);	/* Our image isn't in our RAM! */
			*start_pfn_p = PFN_UP(end);
			int heap_len = mem_size - ((*start_pfn_p << PAGE_SHIFT) - mem_base);
			*max_pfn_p = *start_pfn_p + PFN_DOWN(heap_len);

			printk("Using pages %d to %d as free space for heap.\n", *start_pfn_p, *max_pfn_p);
			break;
		}
	}

	if (*max_pfn_p == 0)
	{
		printk("No memory found in FDT!\n");
		BUG();
	}

	/* The device tree is probably in memory that we're about to hand over to the page
	 * allocator, so make sure we don't use it after this.
	 */
	device_tree = NULL;

	build_pagetable(start_pfn_p, max_pfn_p);
}

void arch_init_p2m(unsigned long max_pfn)
{
}

void arch_init_demand_mapping_area(unsigned long cur_pfn)
{
}
