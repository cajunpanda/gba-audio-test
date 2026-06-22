/* Minimal GBA linker script - runs code from ROM, data/bss in IWRAM.
   Dependency-free (no devkitARM specs needed). */
OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY
{
    rom   (rx)  : ORIGIN = 0x08000000, LENGTH = 32M
    iwram (rwx) : ORIGIN = 0x03000000, LENGTH = 32K
    ewram (rwx) : ORIGIN = 0x02000000, LENGTH = 256K
}

SECTIONS
{
    .text 0x08000000 :
    {
        KEEP (*(.gba_header))   /* cart header + Nintendo logo must be first */
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > rom

    __data_lma = .;             /* load address of .data in ROM */

    .data : AT (__data_lma)
    {
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end = .;
    } > iwram

    .bss (NOLOAD) :
    {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > iwram

    /* user/system stack top lives just below IRQ stack in IWRAM */
    __stack_top = 0x03007F00;

    /DISCARD/ : { *(.comment) *(.note*) *(.ARM.attributes) }
}
