#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

struct interrupt_frame_t {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;
};

void idt_init(void);
void double_fault_test(void);
void exception_handler(struct interrupt_frame_t *frame);

void pic_init(void);
void pit_init(uint32_t frequency);
void irq_handler(uint32_t vector);
uint32_t timer_get_ticks(void);

/* Unexpected IRQ debugging functions */
uint32_t get_unexpected_irq_count(void);
uint32_t get_last_unexpected_irq(void);

#endif