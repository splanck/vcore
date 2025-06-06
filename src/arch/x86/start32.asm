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
    ; check for Multiboot2 magic (eax == 0x36d76289)
    cmp eax,0x36d76289
    jne .no_multiboot

    mov esi,ebx            ; ebx -> multiboot2 information structure
    add esi,8              ; skip total_size and reserved fields

.parse_tag:
    mov edx,[esi]          ; tag type
    mov ecx,[esi+4]        ; tag size
    cmp edx,0              ; end tag ?
    je .done_multiboot
    cmp edx,6              ; type 6 -> memory map
    jne .next_tag

    ; parse memory map
    mov ebx,[esi+8]        ; entry_size
    mov eax,ecx
    sub eax,16
    xor edx,edx
    div ebx                ; eax = entry count
    mov [0x20000],eax      ; store count

    mov edi,0x20008        ; destination for entries
    mov ebp,eax            ; loop counter
    lea esi,[esi+16]       ; first entry
.copy_loop:
    mov eax,[esi]
    mov [edi],eax
    mov eax,[esi+4]
    mov [edi+4],eax
    mov eax,[esi+8]
    mov [edi+8],eax
    mov eax,[esi+12]
    mov [edi+12],eax
    mov eax,[esi+16]
    mov [edi+16],eax
    add esi,ebx
    add edi,20
    dec ebp
    jnz .copy_loop
    jmp .done_multiboot

.next_tag:
    add ecx,7              ; align to 8 bytes
    and ecx,0xfffffff8
    add esi,ecx
    jmp .parse_tag

.done_multiboot:
.no_multiboot:
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
