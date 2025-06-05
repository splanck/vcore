section .multiboot
extern start
align 8
multiboot_header_start:
    dd 0xe85250d6             ; magic
    dd 0                      ; architecture (i386/amd64)
    dd multiboot_header_end - multiboot_header_start
    dd -(0xe85250d6 + 0 + (multiboot_header_end - multiboot_header_start))

    ; Entry address tag
    dw 3                       ; type
    dw 0                       ; flags
    dd 16                      ; size of this tag
    dq start                   ; kernel entry point

    ; End tag
    dw 0
    dw 0
    dd 8
multiboot_header_end:
