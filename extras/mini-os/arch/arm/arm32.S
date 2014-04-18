#define PHYS_START (0x80008000)

.section .text

.globl _start
_start:
	@ zImage header
.rept   8
        mov     r0, r0
.endr
        b       reset
        .word   0x016f2818      @ Magic numbers to help the loader
        .word   _start		@ absolute load/run zImage address
        .word   _end - _start   @ zImage size
	@ end of zImage header

@ Called at boot time. Sets up MMU, exception vectors and stack, and then calls C setup() function.
@ => r2 -> DTB
@ <= never returns
reset:
	@ Fill in the top-level translation table (at page_dir). Based on Mini-OS code.
	@ Populate the whole pagedir with 1MB section descriptors.
	@ TEX[2:0] C B = 001 1 1 (outer and inner write-back, write-allocate)
	ldr	r0, =(0x2 +  		/* Section entry */ \
		      0xc +  		/* C B */ \
		      (3 << 10) + 	/* Read/write */ \
		      (1 << 12) +	/* TEX */ \
		      (1 << 16) +	/* Sharable */ \
		      (1<<19))		/* Non-secure */
	ldr	r1, =page_dir
	add	r3, r1, #4*4*1024	@ Limit (4 GB address space, 4 byte entries)

1:
	str	r0, [r1],#4		@ write the section entry
	add	r0, r0, #1 << 20 	@ next physical page
	cmp	r1, r3
	bne	1b

	@ Tell the system where our new table is located.
	ldr	r3, =page_dir
	mcr	p15, 0, r3, c2, c0, 0	@ set ttbr0

	@ Set access permission for domains
	@ Domains are deprecated, but we have to configure them anyway.
	@ We mark every page as being domain 0 and set domain 0 to "client mode"
	@ (client mod = use access flags in page table).
	mov	r0, #1			@ 1 = client
	mcr	p15, 0, r0, c3, c0, 0	@ DACR

	@ Invalidate TLB
	mcr	p15, 0, r1, c8, c7, 0	@ TLBIALL

	@ Enable MMU / SCTLR
	mrc	p15, 0, r1, c1, c0, 0	@ SCTLR
	orr	r1, r1, #0x7		@ (dcache, barriers, MMU)
	orr	r1, r1, #3 << 11	@ (icache, branch prediction)
	mcr	p15, 0, r1, c1, c0, 0	@ SCTLR
	isb

	@ Set VBAR -> exception_vector_table
	@ SCTLR.V = 0
	adr	r0, exception_vector_table
	mcr	p15, 0, r0, c12, c0, 0

	@ Initialise 16 KB stack
	ldr	sp, =stack_end

	mov	r0, r2			@ C wants the DTB pointer in r0
	b	arch_init

.pushsection .data
.align 13
.globl stack
stack:
	.fill (4*1024), 4, 0x0
stack_end:
.align 13
irqstack:
	.fill (1024), 4, 0x0
.globl shared_info_page
.align 13
shared_info_page:
	.fill (1024), 4, 0x0
.align	14
page_dir:
	.fill (4*1024), 4, 0x0
fault_dump:
	.fill 17, 4, 0x0		@ On fault, we save the registers + CPSR here

.popsection

fault:
	cpsid	aif			@ Disable interrupts

	ldr	r13, =fault_dump
	stmia	r13, {r0-r12}		@ Dump the non-banked registers directly (well, unless from FIQ mode)
	str	r14, [r13, #15 << 2]	@ Our r14 is the faulting r15
	mov	r0, r13

	@ Save the caller's CPSR (our SPSR) too.
	mrs	r1, SPSR
	str	r1, [r13, #16 << 2]

	@ Switch to the mode we came from to get r13 and r14.
	@ If coming from user mode, use System mode instead so we're still
	@ privileged.
	and	r1, r1, #0x1f		@ r1 = SPSR mode
	cmp	r1, #0x10		@ If from User mode
	moveq	r1, #0x1f		@ Then use System mode instead

	mrs	r3, CPSR		@ r3 = our CPSR
	bic	r2, r3, #0x1f
	orr	r2, r2, r1
	msr	CPSR, r2		@ Change to mode r1

	@ Save old mode's r13, r14
	str	r13, [r0, #13 << 2]
	str	r14, [r0, #14 << 2]

	msr	CPSR, r3		@ Back to fault mode

	@ Call C code to format the register dump.
	@ Clobbers the stack, but we're not going to return anyway.
	ldr	r13, =stack_end
	bl	dump_registers
1:
	wfi
	b	1b

@ exception base address
.align 5
.globl exception_vector_table
exception_vector_table:
	b	fault @ reset
	b	fault @ undefined instruction
	b	fault @ supervisor call
	b	fault @ prefetch call
	b	fault @ prefetch abort
	b	fault @ data abort //FIXME CLREX
	b	irq_handler @ irq
	b	firq_handler @ firq

irq_handler:
firq_handler:
	ldr	sp, =irqstack
	add	sp, sp, #(4*1024)

	@ Save registers
	stmda	sp!, {r0 - r12}
	stmda	sp!, {lr}

	@ FIXME Find interrupt id and dispatch to the correct handler.
	@ If event_irq
	ldr	r0, IRQ_handler
	cmp	r0, #0
	beq	. @ If no IRQ handler was setup, just get stuck here!

	adr	lr, 1f
	mov	pc, r0
1:
	@bl	do_hypervisor_callback

	@ Restore registers
	ldmib	sp!, {lr}
	ldmib	sp!, {r0 - r12}

	@ Return from IRQ
	subs	pc, lr, #4

.globl IRQ_handler
IRQ_handler:
	.long	0x0


.globl __arch_switch_threads
@ r0 = &prev->sp & r1 = &(next->sp)
__arch_switch_threads:
	@ store sp, ip for prev thread
	str	sp, [r0] @ sp
	str	lr, [r0, #4] @ ip
	str	fp, [sp, #-4] @ store fp on the stack
	@ Load sp, ip for next thread
	ldr	sp, [r1] @ sp
	ldr	lr, [r1, #4] @ ip
	ldr	fp, [sp, #-4] @ restore fp from the stack
	mov	pc, lr
