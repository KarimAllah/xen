#include <mini-os/os.h>
#include <xen/xen.h>
#include <xen/memory.h>
#include <hypervisor.h>
#include <arm/arch_mm.h>

/*
 * This structure contains start-of-day info, such as pagetable base pointer,
 * address of the shared_info structure, and things like that.
 */
union start_info_union start_info_union;

/*
 * Shared page for communicating with the hypervisor.
 * Events flags go here, for example.
 */
shared_info_t *HYPERVISOR_shared_info;

extern char shared_info_page[PAGE_SIZE];

void start_kernel(void);

/*
 * INITIAL C ENTRY POINT.
 */
void arch_init(void *dtb_pointer)
{
    struct xen_add_to_physmap xatp;

    memset(&__bss_start, 0, &_end - &__bss_start);

    printk("dtb_pointer : %x\n", dtb_pointer);

    /* Map shared_info page */
	xatp.domid = DOMID_SELF;
	xatp.idx = 0;
	xatp.space = XENMAPSPACE_shared_info;
	xatp.gpfn = virt_to_pfn(shared_info_page);
	if (HYPERVISOR_memory_op(XENMEM_add_to_physmap, &xatp) != 0)
		BUG();
	HYPERVISOR_shared_info = (struct shared_info *)shared_info_page;

	start_kernel();
}

void
arch_fini(void)
{

}

void
arch_do_exit(void)
{
}
