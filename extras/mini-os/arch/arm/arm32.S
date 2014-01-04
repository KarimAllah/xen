#define PHYS_START (0x80008000)

.section .text

.globl _start
_start:
	@ zImage header
.rept   8
        mov     r0, r0
.endr
        b       1f
        .word   0x016f2818      @ Magic numbers to help the loader
        .word   _start		@ absolute load/run zImage address
        .word   _end - _start   @ zImage size
	@ end of zImage header

1:
#if FIXME
@ core processor specific initialization
	@ Initialize ACTLR core register
	mrc p15, 0, <Rt>, c1, c0, 1
	mcr p15, 0, <Rt>, c1, c0, 1
#endif

@ Save dtb pointer passed by the hypervisor
	mov	r4, r2		@ save dtb pointer

@ Build pagetables
	bl	build_pagetables

	ldr	r2, =page_dir
	MCR	p15, 0, r2, c2, c0, 0	@ set ttbr0

	@ Set access permission for domains
	mov	r0, #0x3
	MCR	p15, 0, r0, c3, c0, 0
	isb
	
	ldr	r0, =mmu_switched

	@ enable mmu / sctlr
	mrc	p15, 0, r1, c1, c0, 0	@ read sctlr
	orr	r1, r1, #0x1		@ enable mmu
	@orr	r1, r1, #0x4		@ enable cache
	orr	r1, r1, #0x02		@ enable barrier enable
	@mcr	p15, 0, r1, c1, c0, 0	@ write sctlr
	isb

	@ jump to the virtual address now
	mov	pc, r0

@ If we're here, then we're very lucky
mmu_switched:

@ set VBAR = exception_vector_table
	@ SCTLR.V = 0
	adr	r0, exception_vector_table
	mcr	p15, 0, r0, c12, c0, 0

@ load stack
	ldr	sp, =stack
	add	sp, sp, #(4*4*1024)


	mov	r0, r4		@ set r0 = dtb pointer
	b	arch_init


@ Populate the whole pagedir with 1MB section descriptors for now (1-1 mapping)
build_pagetables:
	ldr	r0, =(0x2 + (1 << 16) + (1<<19)) @ First section entry (sharable + NS ).
	ldr	r1, =page_dir
	add	r2, r1, #(4*4*1024) @ Limit

1:
	str	r0, [r1] @ write the section entry

	add	r0, r0, #(1 << 20) @ next physical page.
	add	r1, r1, #4 @ next pagedir entry
	cmp	r1, r2
	bne	1b

	mov	pc, lr

.pushsection .data
.align 13
.globl stack
stack:
	.fill (4*1024), 4, 0x0
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

.popsection

@ exception base address
.align 5
.globl exception_vector_table
exception_vector_table:
	b	. @ reset
	b	. @ undefined instruction
	b	. @ supervisor call
	b	. @ prefetch call
	b	. @ prefetch abort
	b	. @ data abort //FIXME CLREX
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

#if 0
pagetables_code:
	bl	build_pagetables

	ldr	r2, =(page_dir+PHYS_START)
	MCR	p15, 0, r2, c2, c0, 0	@ set ttbr0

	@ Set access permission for domains
	mov	r0, #0x3
	MCR	p15, 0, r0, c3, c0, 0

	ldr	r0, =mmu_switched

	@ enable mmu / sctlr
	isb
	MRC	p15, 0, r1, c1, c0, 0	@ read sctlr
	orr	r1, r1, #0x1
	MCR	p15, 0, r1, c1, c0, 0	@ write sctlr
	MRC     p15, 0, r1, c0, c0, 0	@ read id reg

	@ jump to the virtual address now
	mov	pc, r0

mmu_switched:
	b	.

build_pagetables:
	ldr	r2, =(page_dir + PHYS_START)
	ldr	r0, =(pagetable + PHYS_START + 1)
	str	r0, [r2]

	ldr	r2, =(pagetable + PHYS_START)
	add	r1, r2, #1000

	@ setup entry
	ldr	r0, =(PHYS_START + 0x3)
2:
	str	r0, [r2], #4
	add	r0, r0, #1 << 12
	cmp	r2, r1
	bne	2b
	mov	pc, lr

.pushsection .data
.align	14
page_dir:
	.fill (1024), 4, 0x0
pagetable:
	.fill (1024*50), 4, 0x0
.popsection
#endif
