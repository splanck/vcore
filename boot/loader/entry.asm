section .data
global kernel_entry
kernel_entry: dq 0

section .text
extern EMain
global start

start:
    mov rsp,0xffff800000200000
    call EMain

    mov rax,[rel kernel_entry]
    jmp rax
    
End:
    hlt
    jmp End


section .note.GNU-stack noalloc noexec nowrite progbits
