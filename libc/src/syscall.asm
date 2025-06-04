section .text
global writeu
global sleepu
global exitu
global waitu
global keyboard_readu
global get_total_memoryu
global open_file
global read_file
global get_file_size
global close_file
global fork
global exec
global read_root_directory
global create_file
global write_file
global delete_file
global sbrk

writeu:
    sub rsp,16
    xor eax,eax

    mov [rsp],rdi
    mov [rsp+8],rsi

    mov rdi,2
    mov rsi,rsp
    int 0x80

    add rsp,16
    ret

sleepu:
    sub rsp,8
    mov eax,1

    mov [rsp],rdi
    mov rdi,1
    mov rsi,rsp

    int 0x80

    add rsp,8
    ret

exitu:
    mov eax,2
    mov rdi,0

    int 0x80

    ret

waitu:
    sub rsp,8
    mov eax,3

    mov [rsp],rdi
    mov rdi,1
    mov rsi,rsp

    int 0x80

    add rsp,8
    ret

keyboard_readu:
    mov eax,4
    xor edi,edi
    
    int 0x80

    ret

get_total_memoryu:
    mov eax,5
    xor edi,edi

    int 0x80

    ret

open_file:
    sub rsp,8
    mov eax,6

    mov [rsp],rdi
    mov rdi,1
    mov rsi,rsp

    int 0x80

    add rsp,8

    ret

read_file:
    sub rsp,24
    mov eax,7

    mov [rsp],rdi
    mov [rsp+8],rsi
    mov [rsp+16],rdx

    mov rdi,3
    mov rsi,rsp
    
    int 0x80

    add rsp,24
    ret

get_file_size:
    sub rsp,8
    mov eax,8

    mov [rsp],rdi
    mov rdi,1
    mov rsi,rsp

    int 0x80

    add rsp,8

    ret

close_file:
    sub rsp,8
    mov eax,9

    mov [rsp],rdi
    mov rdi,1
    mov rsi,rsp

    int 0x80

    add rsp,8

    ret

fork:
    mov eax,10
    xor edi,edi
    
    int 0x80

    ret

exec:
    sub rsp,8
    mov eax,11

    mov [rsp],rdi
    mov rdi,1
    mov rsi,rsp

    int 0x80

    add rsp,8
    ret

read_root_directory:
    sub rsp,8
    mov eax,12

    mov [rsp],rdi
    mov rdi,1
    mov rsi,rsp

    int 0x80

    add rsp,8
    ret

sbrk:
    sub rsp,8
    mov eax,13

    mov [rsp],rdi
    mov rdi,1
    mov rsi,rsp

    int 0x80

    add rsp,8
    ret

create_file:
    sub rsp,8
    mov eax,14

    mov [rsp],rdi
    mov rdi,1
    mov rsi,rsp

    int 0x80

    add rsp,8
    ret

write_file:
    sub rsp,24
    mov eax,15

    mov [rsp],rdi
    mov [rsp+8],rsi
    mov [rsp+16],rdx

    mov rdi,3
    mov rsi,rsp

    int 0x80

    add rsp,24
    ret

delete_file:
    sub rsp,8
    mov eax,16

    mov [rsp],rdi
    mov rdi,1
    mov rsi,rsp

    int 0x80

    add rsp,8
    ret

; Socket system calls
global socket
global sendto
global recvfrom
global set_priority
global get_priority
global get_runtime
global mkdir
global opendir
global readdir
global rmdir

socket:
    sub rsp,8
    mov eax,17
    mov [rsp],rdi
    mov rdi,1
    mov rsi,rsp
    int 0x80
    add rsp,8
    ret

sendto:
    sub rsp,24
    mov eax,18
    mov [rsp],rdi
    mov [rsp+8],rsi
    mov [rsp+16],rdx
    mov rdi,3
    mov rsi,rsp
    int 0x80
    add rsp,24
    ret

recvfrom:
    sub rsp,24
    mov eax,19
    mov [rsp],rdi
    mov [rsp+8],rsi
    mov [rsp+16],rdx
    mov rdi,3
    mov rsi,rsp
    int 0x80
    add rsp,24
    ret

set_priority:
    sub rsp,8
    mov eax,20
    mov [rsp],rdi
    mov rdi,1
    mov rsi,rsp
    int 0x80
    add rsp,8
    ret

get_priority:
    mov eax,21
    xor edi,edi
    int 0x80
    ret

get_runtime:
    mov eax,22
    xor edi,edi
    int 0x80
    ret

mkdir:
    sub rsp,8
    mov eax,23
    mov [rsp],rdi
    mov rdi,1
    mov rsi,rsp
    int 0x80
    add rsp,8
    ret

opendir:
    sub rsp,8
    mov eax,24
    mov [rsp],rdi
    mov rdi,1
    mov rsi,rsp
    int 0x80
    add rsp,8
    ret

readdir:
    sub rsp,16
    mov eax,25
    mov [rsp],rdi
    mov [rsp+8],rsi
    mov rdi,2
    mov rsi,rsp
    int 0x80
    add rsp,16
    ret

rmdir:
    sub rsp,8
    mov eax,26
    mov [rsp],rdi
    mov rdi,1
    mov rsi,rsp
    int 0x80
    add rsp,8
    ret



