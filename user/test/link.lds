OUTPUT_FORMAT("elf64-x86-64")
ENTRY(start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x400000;

    .text : { *(.text) *(.rodata) } :text

    . = ALIGN(16);
    .data : { *(.data) *(.bss) } :data
}
