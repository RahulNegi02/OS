BITS 32

section .multiboot
align 4
    dd 0x1BADB002
    dd 0x00000003
    dd -(0x1BADB002 + 0x00000003)

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
global _start
extern kernel_main

_start:
    cli

    ; Save GRUB Multiboot information.
    ; EAX = Multiboot magic
    ; EBX = Multiboot information address

    mov esp, stack_top

    ; Pass arguments using cdecl.
    push ebx
    push eax

    call kernel_main

.hang:
    cli
    hlt
    jmp .hang