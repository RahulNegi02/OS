
#include <stdint.h>

static volatile uint32_t timer_ticks = 0;

/* Unexpected interrupt tracking */
static volatile uint32_t unexpected_irq_count = 0;
static volatile uint32_t last_unexpected_irq = 0;

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1"
                      :
                      : "a"(value), "Nd"(port));
}

static void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8) {
        outb(0xA0, 0x20);
    }

    outb(0x20, 0x20);
}


void irq_handler(uint32_t vector)
{
    /* Validate hardware IRQ vector */
    if (vector < 32 || vector > 47) {
        return;
    }

    /* Handle system timer */
    if (vector == 32) {
        timer_ticks++;
    }
    else {
        /* Record unexpected or unhandled IRQ */
        unexpected_irq_count++;
        last_unexpected_irq = vector;
    }

    /* Send End Of Interrupt */
    pic_send_eoi((uint8_t)(vector - 32));
}

uint32_t timer_get_ticks(void)
{
    return timer_ticks;
}
uint32_t get_unexpected_irq_count(void)
{
    return unexpected_irq_count;
}

uint32_t get_last_unexpected_irq(void)
{
    return last_unexpected_irq;
}
void pic_init(void)
{
    /* Start PIC initialization. */
    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    /* Remap master PIC to vectors 32-39. */
    outb(0x21, 0x20);
    outb(0xA1, 0x28);

    /* Tell master and slave how they are connected. */
    outb(0x21, 0x04);
    outb(0xA1, 0x02);

    /* Use 8086 mode. */
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    /* Initially mask all hardware IRQs. */
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
}

void pit_init(uint32_t frequency)
{
    if (frequency == 0) {
        return;
    }

    uint32_t divisor = 1193182 / frequency;

    if (divisor == 0 || divisor > 65535) {
        return;
    }

    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));

    /* Unmask only IRQ0 (timer) on the master PIC. */
    outb(0x21, 0xFE);
    outb(0xA1, 0xFF);
}
