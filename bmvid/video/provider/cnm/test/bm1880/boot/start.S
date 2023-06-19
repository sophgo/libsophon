	.section .text.start
	.globl start
start:

clear_bss:
	ldr	x0, =__bss_start
	ldr	x1, =__bss_end

	cmp	x0, x1
	beq	clbss_d

clbss_l:
	str	wzr, [x0]		/* clear loop... */
	add	x0, x0, #4
	cmp	x0, x1
	bne	clbss_l
clbss_d:

	/* save and restore uboot gd and lr */
	ldr	x2, =uboot_gd
	str	x18, [x2]

	ldr	x2, =uboot_lr
	str	x30, [x2]
	
	bl	__asm_invalidate_dcache_all
	bl	__asm_invalidate_icache_all
	bl	__asm_invalidate_l3_icache

	bl	main

	ldr	x2, =uboot_gd
	ldr	x18, [x2]

	ldr	x2, =uboot_lr
	ldr	x30, [x2]

	ret

	.balign	8
uboot_gd:
	.quad	0

uboot_lr:
	.quad	0
