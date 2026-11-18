
#include <stdint.h>
#include "gdt.h"
#include "interrupts.h"
#include "paging.h"
#include "keyboard.h"

static volatile uint16_t *video =
    (volatile uint16_t *)0xB8000;

static int shell_row = 4;
static int input_col = 10;

static void put_char(int row, int col, char c, uint8_t color)
{
    if (row >= 0 && row < 25 && col >= 0 && col < 80) {
        video[row * 80 + col] =
            (uint16_t)c | ((uint16_t)color << 8);
    }
}

static void print_text(const char *text, int row, int col)
{
    for (int i = 0; text[i] != '\0' && col + i < 80; i++) {
        put_char(row, col + i, text[i], 0x0A);
    }
}

static void clear_line(int row)
{
    for (int col = 0; col < 80; col++)
        put_char(row, col, ' ', 0x0F);
}

static void clear_shell(void)
{
    for (int row = 3; row < 25; row++)
        clear_line(row);

    shell_row = 4;

    print_text("CampusOS Command Shell", 3, 0);
}

static void print_number(uint32_t number, int row, int col)
{
    char digits[10];
    int count = 0;

    do {
        digits[count++] = '0' + (number % 10);
        number /= 10;
    } while (number && count < 10);

    for (int i = 0; i < count; i++) {
        put_char(row, col + i, digits[count - 1 - i], 0x0B);
    }
}

static int string_equal(const char *a, const char *b)
{
    int i = 0;

    while (a[i] && b[i]) {
        if (a[i] != b[i])
            return 0;
        i++;
    }

    return a[i] == '\0' && b[i] == '\0';
}

static void show_prompt(void)
{
    clear_line(shell_row);
    print_text("CampusOS> ", shell_row, 0);
    input_col = 10;
}

static void execute_command(const char *command)
{
    int output_row = shell_row + 1;

    if (string_equal(command, "help")) {
        print_text("Available commands:", output_row, 0);
        print_text("help - Show commands", output_row + 1, 0);
        print_text("clear - Clear screen", output_row + 2, 0);
        print_text("ticks - Show timer", output_row + 3, 0);
        print_text("about - OS information", output_row + 4, 0);

        shell_row = output_row + 6;
    }
    else if (string_equal(command, "clear")) {
        clear_shell();
    }
    else if (string_equal(command, "ticks")) {
        print_text("Current timer ticks:", output_row, 0);
        print_number(timer_get_ticks(), output_row, 21);

        shell_row = output_row + 2;
    }
    else if (string_equal(command, "about")) {
        print_text("CampusOS - College Lab Operating System",
                   output_row, 0);
        print_text("Architecture: x86 32-bit",
                   output_row + 1, 0);
        print_text("Kernel: Custom freestanding kernel",
                   output_row + 2, 0);

        shell_row = output_row + 4;
    }
    else if (command[0] == '\0') {
        shell_row = output_row + 1;
    }
    else {
        print_text("Unknown command. Type help.",
                   output_row, 0);
        shell_row = output_row + 2;
    }

    if (shell_row >= 23)
        clear_shell();

    show_prompt();
}

void kernel_main(void)
{
    __asm__ volatile ("cli");

    gdt_init();
    idt_init();
    paging_init();
    double_fault_tss_init();
    pic_init();
    pit_init(100);
    keyboard_init();

    print_text("CampusOS: Kernel initialized", 0, 0);
    print_text("Timer ticks:", 1, 0);
    print_text("CampusOS Command Shell", 3, 0);

    __asm__ volatile ("sti");

    uint32_t last_ticks = 0;

    char key;
    char command[64];
    int command_length = 0;

    show_prompt();

    while (1)
    {
        uint32_t ticks = timer_get_ticks();

        if (ticks != last_ticks) {
            last_ticks = ticks;

            clear_line(1);
            print_text("Timer ticks:", 1, 0);
            print_number(ticks, 1, 13);
        }

        if (keyboard_try_read_char(&key))
        {
            if (key == '\b') {
                if (command_length > 0) {
                    command_length--;
                    input_col--;

                    command[command_length] = '\0';

                    put_char(shell_row, input_col, ' ', 0x0F);
                }
            }
            else if (key == '\n') {
                command[command_length] = '\0';

                execute_command(command);

                command_length = 0;
                command[0] = '\0';
            }
            else if (command_length < 63 && input_col < 79) {
                command[command_length++] = key;
                command[command_length] = '\0';

                put_char(shell_row, input_col, key, 0x0F);
                input_col++;
            }
        }

        __asm__ volatile ("hlt");
    }
}
