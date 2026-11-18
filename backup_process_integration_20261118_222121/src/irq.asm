
BITS 32

section .text

extern irq_handler

global irq0
global irq1
global irq_common

%macro IRQ_STUB 2
global irq%1
irq%1:
    push dword 0
    push dword %2
    jmp irq_common
%endmacro

; Hardware IRQ vectors after PIC remapping
IRQ_STUB 0, 32
IRQ_STUB 1, 33
IRQ_STUB 2, 34
IRQ_STUB 3, 35
IRQ_STUB 4, 36
IRQ_STUB 5, 37
IRQ_STUB 6, 38
IRQ_STUB 7, 39
IRQ_STUB 8, 40
IRQ_STUB 9, 41
IRQ_STUB 10, 42
IRQ_STUB 11, 43
IRQ_STUB 12, 44
IRQ_STUB 13, 45
IRQ_STUB 14, 46
IRQ_STUB 15, 47

irq_common:
    cld
    pusha

    ; Pass the interrupt vector to C.
    mov eax, [esp + 32]
    push eax
    call irq_handler
    add esp, 4

    popa
    add esp, 8
    iretd
