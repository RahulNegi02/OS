
#ifndef IDT_H
#define IDT_H

#include <stdint.h>
#include <stdbool.h>

bool idt_init(void);

void isr_handler(uint64_t *stack_frame);

bool idt_expect_page_fault(
    uint64_t fault_address,
    uint64_t error_mask,
    uint64_t error_value,
    uintptr_t recovery_address
);

void idt_cancel_expected_page_fault(void);

#endif

