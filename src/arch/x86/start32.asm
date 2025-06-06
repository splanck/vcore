[BITS 32]
%define KERNEL_BASE 0xffff800000000000

section .text

global start32
global start
extern start64
extern Gdt64Ptr
%define KERNEL_PHYS_BASE 0x200000

start:
start32:
    ; zero page table area
    mov edi,0x70000
    xor eax,eax
    mov ecx,0x10000/4
    rep stosd

    ; setup low identity mapping via 1 GiB page
    mov dword[0x70000],0x71007
    mov dword[0x71000],10000111b

    ; map kernel virtual base to same physical memory
    mov eax,(KERNEL_BASE>>39)
    and eax,0x1ff
    mov dword[0x70000+eax*8],0x72003
    mov dword[0x72000],10000011b

    ; load 64-bit GDT for transition
    mov eax,(Gdt64Ptr - KERNEL_BASE + KERNEL_PHYS_BASE)
    lgdt [eax]

    ; enable PAE
    mov eax,cr4
    or eax,(1<<5)
    mov cr4,eax

    ; load page table
    mov eax,0x70000
    mov cr3,eax

    ; enable LME
    mov ecx,0xc0000080
    rdmsr
    or eax,(1<<8)
    wrmsr

    ; enable paging
    mov eax,cr0
    or eax,(1<<31)
    mov cr0,eax

    ; jump to 64-bit entry
    jmp 08:(start64 - KERNEL_BASE + KERNEL_PHYS_BASE)
