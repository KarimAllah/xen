
void arch_init_xenbus(struct xenstore_domain_interface **xenstore_buf, uint32_t *store_evtchn) {
	*xenstore_buf = mfn_to_virt(start_info.store_mfn);
	*store_evtchn = start_info.store_evtchn
}
