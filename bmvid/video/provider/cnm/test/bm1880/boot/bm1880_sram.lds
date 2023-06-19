OUTPUT_FORMAT("elf64-littleaarch64", "elf64-littleaarch64", "elf64-littleaarch64") OUTPUT_ARCH(aarch64)
ENTRY(start)

MEMORY
{
	sram (xrw)      : ORIGIN = 0x04000000, LENGTH = 0x00020000
}

STACK_SIZE = DEFINED(STACK_SIZE) ? STACK_SIZE : 0x7000;

SECTIONS
{
  .text :
  {
    . = ALIGN(8);
    __text_start = .;
    KEEP(*(.text.start))
    . = ALIGN(8);
    *(.text .text*)
    *(.rodata .rodata*)
    . = ALIGN(8);
  } > sram

  __rw_start = LOADADDR(.data);

  .data :
  {
    . = ALIGN(8);
    __data_start = .;
    *(.data .data.*)
    . = ALIGN(8);
    __data_end = .;
  } > sram

  .bss (NOLOAD) :
  {
    . = ALIGN(8);
    __bss_start = . ;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(8);
    __bss_end = . ;
  } > sram

  .stack (NOLOAD) :
  {
    . = ALIGN(8);
    __stack_start = . ;
    . = . + STACK_SIZE;
    . = ALIGN(8);
    __stack_end = . ;
  } > sram

  /DISCARD/ :
  {
	*(.init*)
	*(.fini*)
  }

}

