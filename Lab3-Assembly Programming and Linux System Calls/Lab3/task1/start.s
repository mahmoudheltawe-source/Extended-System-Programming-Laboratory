; Standalone i386 encoder; main receives argc/argv using cdecl.
section .data
Infile dd 0
Outfile dd 1
newline db 10
error_msg db "Encoder: invalid option or I/O error", 10
error_len equ $-error_msg
section .bss
character resb 1
section .text
global _start, main
_start:
    mov eax, [esp]
    lea edx, [esp+4]
    push edx
    push eax
    call main
    mov ebx, eax
    mov eax, 1
    int 0x80

main:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov esi, [ebp+12]
    xor edi, edi
.print_args:
    cmp edi, [ebp+8]
    jge .parse_start
    mov ecx, [esi+edi*4]
    xor edx, edx
.length:
    cmp byte [ecx+edx], 0
    je .print
    inc edx
    jmp .length
.print:
    mov ebx, 2
    call write_all
    mov ecx, newline
    mov edx, 1
    call write_all
    inc edi
    jmp .print_args
.parse_start:
    mov edi, 1
.parse:
    cmp edi, [ebp+8]
    jge .read
    mov ebx, [esi+edi*4]
    cmp byte [ebx], '-'
    jne fail
    cmp byte [ebx+1], 'i'
    je .input
    cmp byte [ebx+1], 'o'
    jne fail
    cmp dword [Outfile], 1
    jne fail
    add ebx, 2
    cmp byte [ebx], 0
    je fail
    mov ecx, 1 | 64 | 512 ; write, create, truncate
    mov edx, 0o644
    mov eax, 5
    int 0x80
    test eax, eax
    js fail
    mov [Outfile], eax
    jmp .next
.input:
    cmp dword [Infile], 0
    jne fail
    add ebx, 2
    cmp byte [ebx], 0
    je fail
    xor ecx, ecx
    mov eax, 5
    int 0x80
    test eax, eax
    js fail
    mov [Infile], eax
.next:
    inc edi
    jmp .parse
.read:
    mov eax, 3
    mov ebx, [Infile]
    mov ecx, character
    mov edx, 1
    int 0x80
    cmp eax, -4 ; retry interrupted reads
    je .read
    test eax, eax
    js fail
    jz .close
    call encode
    mov ebx, [Outfile]
    mov ecx, character
    mov edx, 1
    call write_all
    jmp .read
.close:
    mov ebx, [Infile]
    cmp ebx, 0
    je .close_output
    mov eax, 6
    int 0x80
    test eax, eax
    js fail
.close_output:
    mov ebx, [Outfile]
    cmp ebx, 1
    je .done
    mov eax, 6
    int 0x80
    test eax, eax
    js fail
.done:
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    leave
    ret

encode:
    cmp byte [character], 'A'
    jb .done
    cmp byte [character], 'z'
    ja .done
    inc byte [character]
.done:
    ret

; Write edx bytes at ecx to ebx, accounting for short writes and EINTR.
write_all:
    test edx, edx
    jz .done
.retry:
    mov eax, 4
    int 0x80
    cmp eax, -4
    je .retry
    test eax, eax
    jle fail
    add ecx, eax
    sub edx, eax
    jnz .retry
.done:
    ret
fail:
    mov eax, 4
    mov ebx, 2
    mov ecx, error_msg
    mov edx, error_len
    int 0x80
    mov eax, 1
    mov ebx, 1
    int 0x80
section .note.GNU-stack noalloc noexec nowrite progbits
