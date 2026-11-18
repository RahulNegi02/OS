#include <stdint.h>
#include "serial.h"

#define COM1 0x3F8

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void serial_init(void)
{
    outb(COM1 + 1, 0x00); // Disable interrupts
    outb(COM1 + 3, 0x80); // Enable divisor latch
    outb(COM1 + 0, 0x03); // Baud rate divisor: 3
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03); // 8 bits, no parity, 1 stop bit
    outb(COM1 + 2, 0xC7); // Enable and clear FIFO
    outb(COM1 + 4, 0x0B); // Configure modem control
}

void serial_write_char(char c)
{
    // Wait until the transmitter can accept a character.
    while ((inb(COM1 + 5) & 0x20) == 0) {
        __asm__ volatile ("pause");
    }

    outb(COM1, (uint8_t)c);
}

void serial_writestring(const char *str)
{
    while (*str != '\0') {
        if (*str == '\n') {
            serial_write_char('\r');
        }

        serial_write_char(*str);
        str++;
    }
}