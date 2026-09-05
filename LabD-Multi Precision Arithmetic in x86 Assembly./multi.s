; Lab D -- NASM, 32-bit Linux, CDECL.
; Build: nasm -f elf32 multi.s -o multi.o
;        gcc -m32 -no-pie multi.o -o multi
;
; The handout's layout is inconsistent: an unsigned-char size cannot
; represent 599 hex digits (300 bytes), or a 255-byte sum plus carry.
; Use its example's `dw` size and BYTE elements (db), as required by the
; example output: struct multi { unsigned short size; unsigned char num[]; }.
; All numbers are little endian; the size counts bytes, excluding the header.
; Returned structures own one malloc allocation and must be freed by callers.

BITS 32

%define NUM_OFFSET 2
%define MAX_HEX_DIGITS 599
%define INPUT_CAPACITY (MAX_HEX_DIGITS + 3) ; digits, CR, LF, terminating NUL

; Save CDECL callee-saved registers and align the stack before every call.
; The first 16 bytes of each frame are reserved for outgoing arguments.
%macro ENTER 1
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    and esp, -16
    sub esp, %1
%endmacro

%macro LEAVE 0
    lea esp, [ebp - 12]
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
%endmacro

global print_multi, getmulti, MaxMin, add_multi, rand_num, PRmulti
global x_struct, y_struct, STATE, MASK
extern printf, puts, fgets, malloc, free, fputs, stdin, stderr

section .rodata
    hex_format db '%02hhx', 0
    argc_format db '%d', 10, 0
    empty_line db 0
    usage_message db 'Usage: multi [-I | -R]', 10, 0
    input_message db 'multi: expected two hexadecimal lines of 1 to 599 digits (or allocation failed)', 10, 0
    memory_message db 'multi: memory allocation failed', 10, 0
    MASK dw 0x002d                 ; XOR taps at bit positions 0, 2, 3, 5

section .data
    x_struct: dw 5
    x_num: db 0xaa, 1, 2, 0x44, 0x4f
    y_struct: dw 6
    y_num: db 0xaa, 1, 2, 3, 0x44, 0x4f
    align 2
    STATE dw 0xace1                ; A fixed, nonzero seed makes tests repeatable.

section .text

%ifndef MULTI_LIBRARY
global main

%ifdef PART0
; Preliminary exercise: argc with printf, then every argv entry with puts.
main:
    ENTER 16
    mov ebx, [ebp + 8]
    mov esi, [ebp + 12]
    mov dword [esp], argc_format
    mov [esp + 4], ebx
    call printf
    xor edi, edi
.argument:
    cmp edi, ebx
    jae .done
    mov eax, [esi + edi * 4]
    mov [esp], eax
    call puts
    inc edi
    jmp .argument
.done:
    xor eax, eax
    LEAVE
%else
; Final program: print the two operands and their sum, one per line.
main:
    ENTER 32
    mov dword [esp + 16], 0        ; owned first operand (NULL for static data)
    mov dword [esp + 20], 0        ; owned second operand
    mov dword [esp + 24], 0        ; owned sum
    mov dword [esp + 28], 1        ; exit status, changed on success
    cmp dword [ebp + 8], 1
    je .defaults
    cmp dword [ebp + 8], 2
    jne .usage
    mov eax, [ebp + 12]
    mov eax, [eax + 4]
    cmp byte [eax], '-'
    jne .usage
    cmp byte [eax + 1], 'I'
    je .check_input
    cmp byte [eax + 1], 'R'
    jne .usage
    cmp byte [eax + 2], 0
    jne .usage
    jmp .random
.check_input:
    cmp byte [eax + 2], 0
    jne .usage
    call getmulti
    test eax, eax
    jz .input_error
    mov esi, eax
    mov [esp + 16], eax
    call getmulti
    test eax, eax
    jz .input_error
    mov ebx, eax
    mov [esp + 20], eax
    jmp .calculate
.random:
    call PRmulti
    test eax, eax
    jz .memory_error
    mov esi, eax
    mov [esp + 16], eax
    call PRmulti
    test eax, eax
    jz .memory_error
    mov ebx, eax
    mov [esp + 20], eax
    jmp .calculate
.defaults:
    mov esi, x_struct
    mov ebx, y_struct
.calculate:
    mov [esp], esi
    mov [esp + 4], ebx
    call add_multi
    test eax, eax
    jz .memory_error
    mov edi, eax
    mov [esp + 24], eax
    mov [esp], esi
    call print_multi
    mov [esp], ebx
    call print_multi
    mov [esp], edi
    call print_multi
    mov dword [esp + 28], 0
    jmp .cleanup
.usage:
    mov eax, usage_message
    jmp .error
.input_error:
    mov eax, input_message
    jmp .error
.memory_error:
    mov eax, memory_message
.error:
    mov [esp], eax
    mov eax, [stderr]
    mov [esp + 4], eax
    call fputs
.cleanup:
    mov eax, [esp + 24]
    mov [esp], eax
    call free
    mov eax, [esp + 20]
    mov [esp], eax
    call free
    mov eax, [esp + 16]
    mov [esp], eax
    call free
    mov eax, [esp + 28]
    LEAVE
%endif
%endif

; void print_multi(struct multi *p)
; Skip unused high zero bytes, but print at least one byte (zero is "00").
print_multi:
    ENTER 16
    mov esi, [ebp + 8]
    movzx ebx, word [esi]
    dec ebx
.skip_zero:
    test ebx, ebx
    jz .print_byte
    cmp byte [esi + NUM_OFFSET + ebx], 0
    jne .print_byte
    dec ebx
    jmp .skip_zero
.print_byte:
    movzx eax, byte [esi + NUM_OFFSET + ebx]
    mov dword [esp], hex_format
    mov [esp + 4], eax            ; variadic unsigned-char argument is promoted
    call printf
    dec ebx
    jns .print_byte
    mov dword [esp], empty_line
    call puts
    LEAVE

; struct multi *getmulti(void)
; fgets reads one line. Prepending '0' to odd-length input lets conversion
; process pairs uniformly. Return NULL on EOF, invalid input, or malloc failure.
getmulti:
    ENTER 640
    mov byte [esp + 16], '0'     ; reserved prefix, before the fgets buffer
    lea esi, [esp + 17]
    mov [esp], esi
    mov dword [esp + 4], INPUT_CAPACITY
    mov eax, [stdin]
    mov [esp + 8], eax
    call fgets
    test eax, eax
    jz .failed
    xor ecx, ecx
.length:
    cmp byte [esi + ecx], 0
    je .trim_lf
    inc ecx
    jmp .length
.trim_lf:
    test ecx, ecx
    jz .failed
    cmp byte [esi + ecx - 1], 10
    jne .check_length
    dec ecx
    jz .failed
    cmp byte [esi + ecx - 1], 13
    jne .check_length
    dec ecx
.check_length:
    test ecx, ecx
    jz .failed
    cmp ecx, MAX_HEX_DIGITS
    ja .failed
    test ecx, 1
    jz .even
    dec esi                     ; include the reserved '0' prefix
    inc ecx
.even:
    mov ebx, ecx
    shr ebx, 1
    lea eax, [ebx + NUM_OFFSET]
    mov [esp], eax
    call malloc
    test eax, eax
    jz .failed
    mov edi, eax
    mov [edi], bx
    dec ebx                     ; first pair is the most significant byte
.pair:
    mov al, [esi]
    call hex_digit
    jc .invalid
    shl al, 4
    mov dl, al
    mov al, [esi + 1]
    call hex_digit
    jc .invalid
    or al, dl
    mov [edi + NUM_OFFSET + ebx], al
    add esi, 2
    dec ebx
    jns .pair
    mov eax, edi
    LEAVE
.invalid:
    mov [esp], edi
    call free
.failed:
    xor eax, eax
    LEAVE

; Internal register convention: AL = ASCII digit -> AL = nibble, CF = error.
; Preserves all other registers, including DL (the first nibble in getmulti).
hex_digit:
    cmp al, '0'
    jb .invalid
    cmp al, '9'
    jbe .decimal
    or al, 0x20                 ; accept A-F as well as a-f
    cmp al, 'a'
    jb .invalid
    cmp al, 'f'
    ja .invalid
    sub al, 'a' - 10
    clc
    ret
.decimal:
    sub al, '0'
    clc
    ret
.invalid:
    stc
    ret

; Non-CDECL helper: EAX, EBX = operands -> EAX = longer, EBX = shorter.
; Equal sizes keep their original order. Clobbers ECX and flags.
MaxMin:
    movzx ecx, word [eax]
    cmp cx, [ebx]
    jae .done
    xchg eax, ebx
.done:
    ret

; struct multi *add_multi(struct multi *p, struct multi *q)
; Allocate a header and max(size_p, size_q) + 1 bytes, including final carry.
; Neither input is changed. Save carry in DL across loop-control comparisons.
add_multi:
    ENTER 32
    mov eax, [ebp + 8]
    mov ebx, [ebp + 12]
    call MaxMin
    mov esi, eax
    movzx eax, word [esi]
    cmp eax, 65535              ; an extra byte must fit in the length field
    je .failed
    mov [esp + 16], eax
    movzx edx, word [ebx]
    mov [esp + 20], edx
    add eax, NUM_OFFSET + 1
    mov [esp], eax
    call malloc
    test eax, eax
    jz .failed
    mov edi, eax
    mov eax, [esp + 16]
    inc eax
    mov [edi], ax
    xor ecx, ecx                ; byte index
    xor edx, edx                ; carry = 0
.both:
    cmp ecx, [esp + 20]
    jae .remaining
    mov al, [esi + NUM_OFFSET + ecx]
    bt edx, 0                   ; restore carry before ADC
    adc al, [ebx + NUM_OFFSET + ecx]
    setc dl
    mov [edi + NUM_OFFSET + ecx], al
    inc ecx
    jmp .both
.remaining:
    cmp ecx, [esp + 16]
    jae .last_carry
    mov al, [esi + NUM_OFFSET + ecx]
    bt edx, 0
    adc al, 0
    setc dl
    mov [edi + NUM_OFFSET + ecx], al
    inc ecx
    jmp .remaining
.last_carry:
    mov [edi + NUM_OFFSET + ecx], dl
    mov eax, edi
    LEAVE
.failed:
    xor eax, eax
    LEAVE

; unsigned int rand_num(void)
; One Fibonacci LFSR step: feedback = parity(STATE & MASK), then shift right
; and insert feedback at bit 15. MASK's taps all fit in the low byte, which
; is the byte examined by x86 PF. SETPO gives 1 for odd parity (XOR feedback).
; Return the updated 16-bit state zero-extended in EAX. Clobbers EDX/flags.
rand_num:
    movzx eax, word [STATE]
    mov edx, eax
    and dx, [MASK]
    setpo dl
    movzx edx, dl
    shl edx, 15
    shr eax, 1
    or eax, edx
    mov [STATE], ax
    ret

; Internal CDECL helper: collect eight successive output bits into one byte.
; Read the low bit after each state transition; first bit becomes byte bit 7.
random_byte:
    ENTER 16
    xor ebx, ebx
    mov esi, 8
.bit:
    call rand_num
    and eax, 1
    shl ebx, 1
    or ebx, eax
    dec esi
    jnz .bit
    mov eax, ebx
    LEAVE

; struct multi *PRmulti(void)
; Generate an 8-bit length (retry zero), then exactly 8*n bits of byte data.
PRmulti:
    ENTER 16
.length:
    call random_byte
    test eax, eax
    jz .length
    mov ebx, eax
    add eax, NUM_OFFSET
    mov [esp], eax
    call malloc
    test eax, eax
    jz .done
    mov esi, eax
    mov [esi], bx
    xor edi, edi
.byte:
    call random_byte
    mov [esi + NUM_OFFSET + edi], al
    inc edi
    cmp edi, ebx
    jb .byte
    mov eax, esi
.done:
    LEAVE

; Mark the stack non-executable for the ELF linker.
section .note.GNU-stack noalloc noexec nowrite progbits
