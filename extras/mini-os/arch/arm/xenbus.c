#include <mini-os/os.h>
#include <mini-os/mm.h>
#include <xen/hvm/params.h>
#include <xen/io/xs_wire.h>
#include <mini-os/hypervisor.h>

static inline int hvm_get_parameter(int idx, uint64_t *value)
{
	struct xen_hvm_param xhv;
	int ret;

	xhv.domid = DOMID_SELF;
	xhv.index = idx;
	ret = HYPERVISOR_hvm_op(HVMOP_get_param, &xhv);
	if (ret < 0) {
		BUG();
	}
	*value = xhv.value;
	return ret;
}

void arch_init_xenbus(struct xenstore_domain_interface **xenstore_buf, uint32_t *store_evtchn) {
	uint64_t value;
	uint64_t xenstore_pfn;

	if (hvm_get_parameter(HVM_PARAM_STORE_EVTCHN, &value))
		BUG();

	*store_evtchn = (int)value;

	if(hvm_get_parameter(HVM_PARAM_STORE_PFN, &value))
		BUG();
	xenstore_pfn = (unsigned long)value;

	*xenstore_buf = pfn_to_virt(xenstore_pfn);
}
