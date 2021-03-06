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
	orr	r1, r1, #0x5		@ (dcache, MMU)
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
irqstack_end:
.globl shared_info_page
.align 13
shared_info_page:
	.fill (1024), 4, 0x0
.align	14
page_dir:
	.fill (4*1024), 4, 0x0
fault_dump:
	.fill 18, 4, 0x0		@ On fault, we save the registers + CPSR + handler address

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

	ldr	r1, [r0, #17 << 2]
	sub	r1, r1, #12		@ Fix to point at start of handler
	str	r1, [r0, #17 << 2]

	@ Call C code to format the register dump.
	@ Clobbers the stack, but we're not going to return anyway.
	ldr	r13, =stack_end
	bl	dump_registers
1:
	wfi
	b	1b

@ We want to store a unique value to identify this handler, without corrupting
@ any of the registers. So, we store r15 (which will point just after the branch).
@ Later, we subtract 12 so the user gets pointed at the start of the exception
@ handler.
#define FAULT(name)			\
.globl fault_##name;			\
fault_##name:				\
	ldr	r13, =fault_dump;	\
	str	r15, [r13, #17 << 2];	\
	b	fault

FAULT(reset)
FAULT(undefined_instruction)
FAULT(svc)
FAULT(prefetch_call)
FAULT(prefetch_abort)
FAULT(data_abort)

@ exception base address
.align 5
.globl exception_vector_table
exception_vector_table:
	b	fault_reset
	b	fault_undefined_instruction
	b	fault_svc
	b	fault_prefetch_call
	b	fault_prefetch_abort
	b	fault_data_abort @ FIXME CLREX
	b	irq_handler @ IRQ
	.word 0xe7f000f0    @ abort on FIQ

irq_handler:
	ldr	sp, =irqstack_end
	push	{r0 - r12, r14}

	ldr	r0, IRQ_handler
	cmp	r0, #0
	.word	0x07f000f0    @ undeq - register dump if no handler
	blx	r0

	@ Return from IRQ
	pop	{r0 - r12, r14}
	subs	pc, lr, #4

.globl IRQ_handler
IRQ_handler:
	.long	0x0


.globl __arch_switch_threads
@ r0 = &prev->sp & r1 = &(next->sp)
__arch_switch_threads:
	stmia	r0, {sp, lr}	@ Store current sp and ip to prev's struct thread
	str	fp, [sp, #-4]	@ Store fp on the old stack

	ldmia	r1, {sp, lr}	@ Load new sp, ip from next's struct thread
	ldr	fp, [sp, #-4]	@ Restore fp from the stack

	mov	pc, lr

.globl arm_start_thread
arm_start_thread:
	pop	{r0, r1}
	@ r0 = user data
	@ r1 -> thread's main function
	ldr	lr, =exit_thread
	bx	r1
