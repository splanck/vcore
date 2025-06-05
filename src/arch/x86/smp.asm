section .text
    global start_aps
    global send_ipi
    extern cpu_count
    extern cpu_mark_online

%define LAPIC_BASE 0xfee00000
%define ICR_LOW   0x300
%define ICR_HIGH  0x310

; Start application processors using INIT-SIPI
start_aps:
    mov eax, [rel cpu_count]
    cmp eax, 1
    jle .done
    mov ecx, 1
.loop:
    mov rax, LAPIC_BASE
    mov edx, ecx
    shl edx, 24
    mov dword [rax + ICR_HIGH], edx
    mov dword [rax + ICR_LOW], 0x000C4500        ; INIT
    mov dword [rax + ICR_HIGH], edx
    mov dword [rax + ICR_LOW], 0x000C4608        ; SIPI
    mov dword [rax + ICR_HIGH], edx
    mov dword [rax + ICR_LOW], 0x000C4608        ; SIPI again
    mov edi, ecx
    call cpu_mark_online
    inc ecx
    cmp ecx, eax
    jl .loop
.done:
    ret

; send_ipi(cpu, vector)
; rdi=cpu id, sil=vector
send_ipi:
    mov rax, LAPIC_BASE
    mov edx, edi
    shl edx, 24
    mov dword [rax + ICR_HIGH], edx
    movzx edx, sil
    or edx, 0x00004000
    mov dword [rax + ICR_LOW], edx
    ret
section .note.GNU-stack
