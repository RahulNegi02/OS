#include <stdint.h>
#include "gdt.h"
#include "interrupts.h"
#include "paging.h"

static void print_text(const char *text, int row)
{
    volatile uint16_t *video =
        (volatile uint16_t *)0xB8000;

    int offset = row * 80;

    for (int i = 0; text[i] != '\0' && i < 80; i++) {
        video[offset + i] =
            (uint16_t)text[i] | (0x0A << 8);
    }
}
void kernel_main(void)
{
    /* Disable interrupts during initialization */
    __asm__ volatile ("cli");

    /* Initialize kernel components */
gdt_init();
idt_init();
paging_init();
double_fault_tss_init();
pic_init();
pit_init(100);

    print_text("CampusOS: GDT, IDT and PIC initialized", 0);

    /* Enable interrupts after initialization */
    __asm__ volatile ("sti");

    uint32_t last_ticks = 0;

    while (1) {
        uint32_t ticks = timer_get_ticks();

        if (ticks != last_ticks) {
            last_ticks = ticks;

            volatile uint16_t *video =
                (volatile uint16_t *)0xB8000;

            const char *label = "Timer ticks: ";
            int pos = 80;

            for (int i = 0; label[i] != '\0'; i++) {
                video[pos + i] =
                    (uint16_t)label[i] | (0x0B << 8);
            }

            char digits[10];
            int count = 0;
            uint32_t n = ticks;

            do {
                digits[count++] = '0' + (n % 10);
                n /= 10;
            } while (n && count < 10);

            for (int i = 0; i < count; i++) {
                video[pos + 13 + i] =
                    (uint16_t)digits[count - 1 - i]
                    | (0x0B << 8);
            }
        }

        __asm__ volatile ("hlt");
    }
}
