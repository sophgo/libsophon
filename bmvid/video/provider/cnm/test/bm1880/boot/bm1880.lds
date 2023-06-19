OUTPUT_FORMAT("elf64-littleaarch64", "elf64-littleaarch64", "elf64-littleaarch64")
OUTPUT_ARCH(aarch64)

MEMORY
{
  .sdram(xrw)	: ORIGIN = 0x110000000, LENGTH = 0x0A800000
}

ENTRY(start)
SECTIONS
{
	.text : {
		. = ALIGN(8);
		*(.text.start)
		*(.text*)
	} > .sdram

	.rodata : {
		. = ALIGN(8);
		*(SORT_BY_ALIGNMENT(SORT_BY_NAME(.rodata*)))
	} > .sdram

	.data : {
		. = ALIGN(8);
		*(.data*)
	} > .sdram

	.bss (NOLOAD) : {
		. = ALIGN(0x1000);
		__bss_start = .;
		*(.bss.heap)
		*(.bss*)
        *(COMMON)
		__bss_end = .;
	} > .sdram
}
