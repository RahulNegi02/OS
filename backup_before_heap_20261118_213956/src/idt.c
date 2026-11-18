
#include <stdint.h>
#include "interrupts.h"

/* IDT gate descriptor */
struct __attribute__((packed)) idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t offset_high;
};

/* IDT register */
struct __attribute__((packed)) idt_pointer {
    uint16_t limit;
    uint32_t base;
};

/* 256 interrupt descriptor entries */
static struct idt_entry idt[256];
static struct idt_pointer idtr;

#define IDT_FLAGS 0x8E

/* CPU exception handlers: vectors 0-31 */
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

/* Hardware IRQ handlers: vectors 32-47 */
extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

/* Configure one IDT gate */
static void idt_set_gate(uint8_t vector, uint32_t address)
{
    idt[vector].offset_low  = (uint16_t)(address & 0xFFFF);
    idt[vector].selector    = 0x08;
    idt[vector].zero        = 0;
    idt[vector].flags       = IDT_FLAGS;
    idt[vector].offset_high = (uint16_t)((address >> 16) & 0xFFFF);
}
static void idt_set_task_gate(uint8_t vector, uint16_t selector)
{
    idt[vector].offset_low  = 0;
    idt[vector].selector    = selector;
    idt[vector].zero        = 0;
    idt[vector].flags       = 0x85;
    idt[vector].offset_high = 0;
}
/* Load the IDT register */
static void idt_load(uint32_t address)
{
    __asm__ volatile (
        "lidt (%0)"
        :
        : "r"(address)
        : "memory"
    );
}

/* Initialize and load the IDT */
void idt_init(void)
{
    /* Clear all 256 entries */
    for (int i = 0; i < 256; i++) {
        idt[i].offset_low  = 0;
        idt[i].selector    = 0x08;
        idt[i].zero        = 0;
        idt[i].flags       = 0;
        idt[i].offset_high = 0;
    }

    /* CPU exception handlers: vectors 0-31 */
    void (*isrs[32])(void) = {
        isr0,  isr1,  isr2,  isr3,
        isr4,  isr5,  isr6,  isr7,
        isr8,  isr9,  isr10, isr11,
        isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19,
        isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27,
        isr28, isr29, isr30, isr31
    };

    for (int i = 0; i < 32; i++) {
        if (i == 8)
            continue;

        idt_set_gate((uint8_t)i, (uint32_t)isrs[i]);
    }

    /* Dedicated Double Fault Task Gate */
    idt_set_task_gate(8, 0x20);
    /* Hardware IRQ handlers: vectors 32-47 */
    void (*irqs[16])(void) = {
        irq0,  irq1,  irq2,  irq3,
        irq4,  irq5,  irq6,  irq7,
        irq8,  irq9,  irq10, irq11,
        irq12, irq13, irq14, irq15
    };

    for (int i = 0; i < 16; i++) {
        idt_set_gate((uint8_t)(32 + i), (uint32_t)irqs[i]);
    }

    /* Load the IDT */
    idtr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr.base  = (uint32_t)&idt;

    idt_load((uint32_t)&idtr);
}
void double_fault_test(void)
{
    /* Temporarily invalidate the GP fault handler */
    idt[13].selector = 0x28;

    /* Trigger General Protection Fault */
    __asm__ volatile ("int $13");

    /* Should never return */
    for (;;)
        __asm__ volatile ("hlt");
}