/* Combined linker script for LPC845 - avoids relative INCLUDE issues */

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 64K
  RAM   (rwx) : ORIGIN = 0x10000000, LENGTH = 16K
}

_Min_Heap_Size = 0x400;
_Min_Stack_Size = 0x800;

/* Stack pointer goes at top of RAM */
_vStackTop = ORIGIN(RAM) + LENGTH(RAM);

/* Legacy symbol for compatibility */
_estack = _vStackTop;

ENTRY(Reset_Handler)

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))  /* Vector table from startup C file */
    . = ALIGN(4);
  } > FLASH

  .crp 0x000002FC :
  {
    KEEP(*(.crp))
  } > FLASH = 0xFF

  .text :
  {
    KEEP(*(.after_vectors))
    *(.text*)
    *(.rodata*)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    KEEP(*(.preinit_array*))
    KEEP(*(.init_array*))
    KEEP(*(.fini_array*))
    . = ALIGN(4);
    _etext = .;
  } > FLASH

  _sidata = LOADADDR(.data);

  .data : AT(_etext)
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM

  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM

  .heap_stack (NOLOAD) :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } > RAM
}
