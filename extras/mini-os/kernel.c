/******************************************************************************
 * kernel.c
 * 
 * Assorted crap goes here, including the initial C entry point, jumped at
 * from head.S.
 * 
 * Copyright (c) 2002-2003, K A Fraser & R Neugebauer
 * Copyright (c) 2005, Grzegorz Milos, Intel Research Cambridge
 * Copyright (c) 2006, Robert Kaiser, FH Wiesbaden
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 */

#include <mini-os/hypervisor.h>
#include <mini-os/mm.h>
#include <mini-os/events.h>
#include <mini-os/time.h>
#include <mini-os/types.h>
#include <mini-os/lib.h>
#include <mini-os/sched.h>
#include <mini-os/xenbus.h>
#include <mini-os/gnttab.h>
#include <mini-os/netfront.h>
#include <mini-os/blkfront.h>
#include <mini-os/fbfront.h>
#include <mini-os/pcifront.h>
#include <mini-os/xmalloc.h>
#include <fcntl.h>
#include <xen/features.h>
#include <xen/version.h>

uint8_t xen_features[XENFEAT_NR_SUBMAPS * 32];

void setup_xen_features(void)
{
    xen_feature_info_t fi;
    int i, j;

    for (i = 0; i < XENFEAT_NR_SUBMAPS; i++) 
    {
        fi.submap_idx = i;
        if (HYPERVISOR_xen_version(XENVER_get_features, &fi) < 0)
            break;

        for (j=0; j<32; j++)
            xen_features[i*32+j] = !!(fi.submap & 1<<j);
    }
}

#ifdef CONFIG_XENBUS
/* This should be overridden by the application we are linked against. */
__attribute__((weak)) void app_shutdown(unsigned reason)
{
    printk("Shutdown requested: %d\n", reason);
}

static void shutdown_thread(void *p)
{
    const char *path = "control/shutdown";
    const char *token = path;
    xenbus_event_queue events = NULL;
    char *shutdown, *err;
    unsigned int shutdown_reason;
    xenbus_watch_path_token(XBT_NIL, path, token, &events);
    while ((err = xenbus_read(XBT_NIL, path, &shutdown)) != NULL)
    {
        free(err);
        xenbus_wait_for_watch(&events);
    }
    err = xenbus_unwatch_path_token(XBT_NIL, path, token);
    free(err);
    err = xenbus_write(XBT_NIL, path, "");
    free(err);
    printk("Shutting down (%s)\n", shutdown);

    if (!strcmp(shutdown, "poweroff"))
        shutdown_reason = SHUTDOWN_poweroff;
    else if (!strcmp(shutdown, "reboot"))
        shutdown_reason = SHUTDOWN_reboot;
    else
        /* Unknown */
        shutdown_reason = SHUTDOWN_crash;
    app_shutdown(shutdown_reason);
    free(shutdown);
}
#endif


/* This should be overridden by the application we are linked against. */
__attribute__((weak)) int app_main(start_info_t *si)
{
	printk("Dummy main: start_info=%p\n", si);
    return 0;
}

void dump_registers(int *saved_registers) {
	int i;
	printk("Fault!\n");
	for (i = 0; i < 16; i++) {
		printk("r%d = %x\n", i, saved_registers[i]);
	}
	printk("CPSR = %x\n", saved_registers[16]);
}

void gic_init(void);

static void test_memory(void) {
	uint32_t *prev = NULL;
	int i;

	int size = 4096 * 1024;
	for (;;) {
		uint32_t *block = malloc(size);

		printk("malloc(%d) -> %p\n", size, block);

		if (!block) {
			size >>= 1;
			if (size < 8) {
				printk("out of memory\n");
				break;
			}
		} else {
			/* Add to linked list. */
			block[0] = (int) prev;
			block[1] = size;
			prev = block;

			/* Fill the remaining words of the page with their addresses. */
			for (i = 2; i < size / 4; i++) {
				block[i] = (uint32_t) (block + i);
			}
		}
	}

	printk("Checking...");

	while (prev) {
		uint32_t *block = prev;
		int size = block[1];
		prev = (uint32_t *) block[0];

		printk("Checking block at %p\n", block);

		for (i = 2; i < size / 4; i++) {
			uint32_t expected = (uint32_t) (block + i);
			if (block[i] != expected) {
				printk("Corrupted: got %p at %p!\n", block[i], expected);
				BUG();
			}
		}

		free(block);
	}

	printk("Memory test done\n");
}

void start_kernel(void)
{
    /* Set up events. */
    init_events();

    __sti();

    setup_xen_features();

    /* Init memory management. */
    init_mm();

    /* Init time and timers. */
    init_time();

    /* Init the console driver. */
    init_console();

    /* Init grant tables */
    init_gnttab();

    /* Init scheduler. */
    init_sched();
 
    /* Init XenBus */
    init_xenbus();


#ifdef CONFIG_XENBUS
    create_thread("shutdown", shutdown_thread, NULL);
#endif


    gic_init();

    if (0) test_memory();

//#define VTIMER_TEST
#ifdef VTIMER_TEST
    while(1){
		int x, y, z;
    	z = 0;
    	// counter
    	__asm__ __volatile__("mrrc p15, 1, %0, %1, c14;isb":"=r"(x), "=r"(y));
    	printk("Counter: %x-%x\n", x, y);

    	__asm__ __volatile__("mrrc p15, 3, %0, %1, c14;isb":"=r"(x), "=r"(y));
    	printk("CompareValue: %x-%x\n", x, y);

    	// TimerValue
    	__asm__ __volatile__("mrc p15, 0, %0, c14, c3, 0;isb":"=r"(x));
    	printk("TimerValue: %x\n", x);

    	// control register
    	__asm__ __volatile__("mrc p15, 0, %0, c14, c3, 1;isb":"=r"(x));
    	printk("ControlRegister: %x\n", x);
    	while(z++ < 0xfffff){}
    }
#endif

    /* Call (possibly overridden) app_main() */
#if defined(__arm__) || defined(__aarch64__)
    app_main(NULL);
#else
    app_main(&start_info);
#endif

    /* Everything initialised, start idle thread */
    run_idle_thread();
}

void stop_kernel(void)
{
    /* TODO: fs import */

    local_irq_disable();

#if 0
    /* Reset grant tables */
    fini_gnttab();
#endif

    /* Reset XenBus */
    fini_xenbus();

#if 0
    /* Reset timers */
    fini_time();
#endif

    /* Reset memory management. */
    fini_mm();

    /* Reset events. */
    fini_events();

    /* Reset arch details */
    arch_fini();
}

void arch_do_exit(void);

/*
 * do_exit: This is called whenever an IRET fails in entry.S.
 * This will generally be because an application has got itself into
 * a really bad state (probably a bad CS or SS). It must be killed.
 * Of course, minimal OS doesn't have applications :-)
 */

void do_exit(void)
{
    printk("Do_exit called!\n");
    arch_do_exit();
    for( ;; )
    {
        struct sched_shutdown sched_shutdown = { .reason = SHUTDOWN_crash };
        HYPERVISOR_sched_op(SCHEDOP_shutdown, &sched_shutdown);
    }
}
