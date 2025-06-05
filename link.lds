OUTPUT_FORMAT("elf64-x86-64")
ENTRY(start)

SECTIONS
{
    KERNEL_PHYS_BASE = 0x200000;
    KERNEL_VIRT_BASE = 0xffff800000000000;

    /* Place the multiboot2 header near the start of the file. */
    . = KERNEL_PHYS_BASE;
    .multiboot : {
        KEEP(*(.multiboot))
    }

    /* Higher half kernel sections. */
    . = KERNEL_VIRT_BASE + KERNEL_PHYS_BASE;

    .text : AT(LOADADDR(.multiboot) + SIZEOF(.multiboot)) {
        *(.text)
    }

    .rodata : AT(LOADADDR(.text) + SIZEOF(.text)) {
        *(.rodata)
        *(.eh_frame*)
    }

    . = ALIGN(16);
    .data : AT(LOADADDR(.rodata) + SIZEOF(.rodata)) {
        *(.data)
        *(.got*)
    }

    .bss : AT(LOADADDR(.data) + SIZEOF(.data)) {
        PROVIDE(bss_start = .);
        *(.bss)
        PROVIDE(bss_end = .);
    }

    PROVIDE(end = .);
}
