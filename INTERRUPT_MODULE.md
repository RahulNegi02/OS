
# CampusOS — Interrupt Handling Module

## Responsibilities
- Global Descriptor Table (GDT) initialization
- Interrupt Descriptor Table (IDT) initialization
- CPU exception handling
- Programmable Interrupt Controller (PIC) configuration
- Programmable Interval Timer (PIT) setup
- Hardware IRQ dispatching

## Main source files
- src/gdt.c
- src/gdt_flush.asm
- src/idt.c
- src/interrupts.asm
- src/exceptions.c
- src/irq.asm
- src/irq.c
- include/gdt.h
- include/interrupts.h

## Public interfaces
- gdt_init(): initialize the GDT
- idt_init(): install exception and IRQ gates
- pic_init(): initialize and remap the PIC
- pit_init(frequency): configure the PIT frequency
- timer_get_ticks(): read the timer interrupt counter
- exception_handler(frame): report a CPU exception
- irq_handler(vector): process an IRQ vector and acknowledge the PIC

## Current verification
- Kernel boots in QEMU
- Multiboot validation passes
- Divide-by-zero exception test passed
- General Protection Fault test passed
- Timer runs at approximately 100 ticks per second

## Known limitations
- Other IRQs have not been individually tested
- Keyboard driver is owned by another team member
- Process scheduling is not implemented in this module
