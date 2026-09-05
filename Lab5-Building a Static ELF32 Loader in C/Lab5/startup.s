section .text
global startup
; startup(argc, argv, entry): transfer to a syscall-only program's _start.
; Build argc, argv[], NULL, empty envp, and an AT_NULL auxiliary terminator.
startup:
    mov ecx, [esp+4]
    mov ebx, [esp+8]
    mov edx, [esp+12]
    lea eax, [ecx*4+20]
    sub esp, eax
    and esp, -16
    mov [esp], ecx
    xor eax, eax
.copy:
    cmp eax, ecx
    jae .done
    mov esi, [ebx+eax*4]
    mov [esp+eax*4+4], esi
    inc eax
    jmp .copy
.done:
    mov dword [esp+ecx*4+4], 0
    mov dword [esp+ecx*4+8], 0
    mov dword [esp+ecx*4+12], 0
    mov dword [esp+ecx*4+16], 0
    mov eax, edx
    xor edx, edx
    jmp eax
section .note.GNU-stack noalloc noexec nowrite progbits
