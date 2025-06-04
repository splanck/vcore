section .text
    global start_aps
    global send_ipi
    extern cpu_count

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
    mov edx, ecx
    shl edx, 24
    mov dword [LAPIC_BASE + ICR_HIGH], edx
    mov dword [LAPIC_BASE + ICR_LOW], 0x000C4500
    mov dword [LAPIC_BASE + ICR_HIGH], edx
    mov dword [LAPIC_BASE + ICR_LOW], 0x000C4608
    inc ecx
    cmp ecx, eax
    jl .loop
.done:
    ret

; send_ipi(cpu, vector)
; rdi=cpu id, sil=vector
send_ipi:
    mov edx, edi
    shl edx, 24
    mov dword [LAPIC_BASE + ICR_HIGH], edx
    movzx eax, sil
    or eax, 0x00004000
    mov dword [LAPIC_BASE + ICR_LOW], eax
    ret
