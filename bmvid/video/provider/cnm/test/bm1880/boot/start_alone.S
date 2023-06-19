/*************************************************************************
 *
 * Startup Code
 *
 *************************************************************************/

#define FVP_PRIMARY_CPU         0x0
#define MPIDR_CPU_MASK      MPIDR_AFFLVL_MASK
#define MPIDR_CLUSTER_MASK  (MPIDR_AFFLVL_MASK << MPIDR_AFFINITY_BITS)
#define MPIDR_AFFINITY_BITS (8)
#define MPIDR_AFFLVL_MASK   (0xff)


.macro  switch_el, xreg, el3_label, el2_label, el1_label
	mrs \xreg, CurrentEL
	cmp \xreg, 0xc
	b.eq    \el3_label
	cmp \xreg, 0x8
	b.eq    \el2_label
	cmp \xreg, 0x4
	b.eq    \el1_label
.endm

	.section	.text.start
	.global		start
	.type		start, %function
start:
	bl is_my_cpu_primary
	cbnz w0, reset
	b .

reset:
	adr		x0, vectors
	switch_el x1, 3f, 2f, 1f

	/*
	 * Could be EL3/EL2/EL1, Inital State:
	 * Little Endian, MMU Disabled, i/dCache Disabled
	 */

3:	msr vbar_el3, x0
	mrs x0, scr_el3
	orr x0, x0, #0xf
	msr scr_el3, x0
	msr cptr_el3, xzr
	mrs x0, sctlr_el3
	mov x1, #0x5
	bic x0, x0, x1
	msr sctlr_el3, x0

	b 0f

2:	msr vbar_el2, x0
	mov x0, #0x33ff
	msr cptr_el2, x0

	mrs x0, sctlr_el2
	mov x1, #0x5
	bic x0, x0, x1
	msr sctlr_el2, x0
	b 0f

1:	msr vbar_el1, x0
	mov x0, #3 << 20
	msr cpacr_el1, x0
	mrs x0, sctlr_el1
	mov x1, #0x5
	bic x0, x0, x1
	msr sctlr_el1, x0

0:
	/* Init stack */
	ldr x0, =__stack_end
	bic sp, x0, #0xf

	/* Init data */
	ldr		x1, =__rw_start
	ldr		x2, =__data_start
	ldr		x3, =__data_end
	cmp		x2, x3
	beq		data_init_end
data_init_loop:
	ldr		x0, [x1], #8
	str		x0, [x2], #8
	cmp		x2, x3
	blo		data_init_loop
data_init_end:

	/* Clear bss */
/*  // xun for palladium
	ldr		x2, =__bss_start
	ldr		x3, =__bss_end
	cmp		x2, x3
	bcs		bss_clear_end
bss_clear_loop:
	str		xzr, [x2], #8
	cmp		x2, x3
	bcc		bss_clear_loop
bss_clear_end:
*/
	/* Early init */
	stp x29, x30, [sp, #-16]!
	bl	system_init
	ldp x29, x30, [sp]

	bl main

	b		.

	.size	start, . - start

	.align	8
	.global	is_my_cpu_primary
	.type	is_my_cpu_primary, %function

is_my_cpu_primary:
	mrs x0, mpidr_el1
	and x0, x0, #(MPIDR_CLUSTER_MASK | MPIDR_CPU_MASK)
	cmp x0, #FVP_PRIMARY_CPU
	cset    w0, eq
	ret
	.size	is_my_cpu_primary, . - is_my_cpu_primary


/*
 * Exception vectors.
 */
	.align	11
	.globl	vectors
vectors:
	.align	7
	b	_do_bad_sync	/* Current EL Synchronous Thread */

	.align	7
	b	_do_bad_irq	/* Current EL IRQ Thread */

	.align	7
	b	_do_bad_fiq	/* Current EL FIQ Thread */

	.align	7
	b	_do_bad_error	/* Current EL Error Thread */

	.align	7
	b	_do_sync	/* Current EL Synchronous Handler */

	.align	7
	b	_do_irq		/* Current EL IRQ Handler */

	.align	7
	b	_do_fiq		/* Current EL FIQ Handler */

	.align	7
	b	_do_error	/* Current EL Error Handler */


/*
 * Enter Exception.
 * This will save the processor state that is ELR/X0~X30
 * to the stack frame.
 */
.macro	exception_entry
	stp	x29, x30, [sp, #-16]!
	stp	x27, x28, [sp, #-16]!
	stp	x25, x26, [sp, #-16]!
	stp	x23, x24, [sp, #-16]!
	stp	x21, x22, [sp, #-16]!
	stp	x19, x20, [sp, #-16]!
	stp	x17, x18, [sp, #-16]!
	stp	x15, x16, [sp, #-16]!
	stp	x13, x14, [sp, #-16]!
	stp	x11, x12, [sp, #-16]!
	stp	x9, x10, [sp, #-16]!
	stp	x7, x8, [sp, #-16]!
	stp	x5, x6, [sp, #-16]!
	stp	x3, x4, [sp, #-16]!
	stp	x1, x2, [sp, #-16]!

	/* Could be running at EL3/EL2/EL1 */
	switch_el x11, 3f, 2f, 1f
3:	mrs	x1, esr_el3
	mrs	x2, elr_el3
	b	0f
2:	mrs	x1, esr_el2
	mrs	x2, elr_el2
	b	0f
1:	mrs	x1, esr_el1
	mrs	x2, elr_el1
0:
	stp	x2, x0, [sp, #-16]!
	mov	x0, sp
.endm

_do_bad_sync:
	b	.

_do_bad_irq:
	b	.

_do_bad_fiq:
	b	.

_do_bad_error:
	b	.

_do_sync:
	b	.

_do_irq:
	exception_entry
	bl	do_irq
	b exception_exit
	b	.

_do_fiq:
	b	.

_do_error:
	b	.

exception_exit:
	ldp	x2, x0, [sp],#16
	switch_el x11, 3f, 2f, 1f
3:	msr	elr_el3, x2
	b	0f
2:	msr	elr_el2, x2
	b	0f
1:	msr	elr_el1, x2
0:
	ldp	x1, x2, [sp],#16
	ldp	x3, x4, [sp],#16
	ldp	x5, x6, [sp],#16
	ldp	x7, x8, [sp],#16
	ldp	x9, x10, [sp],#16
	ldp	x11, x12, [sp],#16
	ldp	x13, x14, [sp],#16
	ldp	x15, x16, [sp],#16
	ldp	x17, x18, [sp],#16
	ldp	x19, x20, [sp],#16
	ldp	x21, x22, [sp],#16
	ldp	x23, x24, [sp],#16
	ldp	x25, x26, [sp],#16
	ldp	x27, x28, [sp],#16
	ldp	x29, x30, [sp],#16
	eret
