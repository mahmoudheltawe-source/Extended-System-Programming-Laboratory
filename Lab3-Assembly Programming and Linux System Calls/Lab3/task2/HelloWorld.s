section .data
msg db 'hello world', 0xA ; Message to print
msg_len equ $ - msg
section .text
global _start
_start:
; Write message to stdout
mov eax, 4	        ; syscall number for sys_write 
mov ebx, 1	        ; file descriptor 1 is stdout 
mov ecx, msg	    ; pointer to message 
mov edx, msg_len	        ; length of message 
int 0x80	        ; call kernel 
; Exit program 
mov eax, 1	        ; syscall number for sys_exit 
xor ebx, ebx	    ; exit code 0 
int 0x80	        ; call kernel  
