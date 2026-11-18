
#include <stdint.h>
#include "interrupts.h"

static volatile uint16_t * const video =
    (volatile uint16_t *)0xB8000;

static void print_text(int row, int col, const char *text)
{
    int pos = row * 80 + col;

    for (int i = 0; text[i] != '\0' && col + i < 80; i++) {
        video[pos + i] =
            (uint16_t)text[i] | (0x0C << 8);
    }
}

static void print_hex(int row, int col, uint32_t value)
{
    const char digits[] = "0123456789ABCDEF";
    int pos = row * 80 + col;

    for (int i = 0; i < 8 && col + i < 80; i++) {
        unsigned shift = (unsigned)(7 - i) * 4;

        video[pos + i] =
            (uint16_t)digits[(value >> shift) & 0x0F]
            | (0x0C << 8);
    }
}


/* Controlled page fault recovery state */

static volatile uint32_t expected_fault_address = 0;
static volatile uint32_t page_fault_recovery_eip = 0;
static volatile uint32_t expected_fault_active = 0;
static volatile uint32_t page_fault_caught = 0;
static volatile uint32_t last_page_fault_error = 0;

void page_fault_arm(uint32_t address, uint32_t recovery_eip)
{
    page_fault_caught = 0;
    last_page_fault_error = 0;
    expected_fault_address = address;
    page_fault_recovery_eip = recovery_eip;
    expected_fault_active = 1;
}

void page_fault_disarm(void)
{
    expected_fault_active = 0;
}

uint32_t page_fault_was_caught(void)
{
    return page_fault_caught;
}

uint32_t page_fault_get_last_error(void)
{
    return last_page_fault_error;
}

static const char *get_exception_name(uint32_t number)
{
    switch (number) {
        case 0:  return "Divide-by-Zero";
        case 1:  return "Debug Exception";
        case 2:  return "Non-Maskable Interrupt";
        case 3:  return "Breakpoint";
        case 4:  return "Overflow";
        case 5:  return "Bound Range Exceeded";
        case 6:  return "Invalid Opcode";
        case 7:  return "Device Not Available";
        case 8:  return "Double Fault";
        case 9:  return "Coprocessor Segment Overrun";
        case 10: return "Invalid TSS";
        case 11: return "Segment Not Present";
        case 12: return "Stack-Segment Fault";
        case 13: return "General Protection Fault";
        case 14: return "Page Fault";
        case 16: return "x87 Floating-Point Exception";
        case 17: return "Alignment Check";
        case 18: return "Machine Check";
        case 19: return "SIMD Floating-Point Exception";
        case 20: return "Virtualization Exception";
        case 21: return "Control Protection Exception";
        case 29: return "VMM Communication Exception";
        case 30: return "Security Exception";
        default: return "Reserved/Unknown Exception";
    }
}


void exception_handler(struct interrupt_frame_t *frame)
{
    __asm__ volatile ("cli");

    print_text(0, 0, "========== CampusOS Kernel Exception ==========");

    print_text(2, 0, "Exception Number:");
    print_hex(2, 20, frame->int_no);

    print_text(3, 0, "Exception Name:");
    print_text(3, 20, get_exception_name(frame->int_no));

    print_text(4, 0, "CPU Error Code:");
    print_hex(4, 20, frame->err_code);

    print_text(5, 0, "Instruction Address:");
    print_hex(5, 20, frame->eip);

    if (frame->int_no == 14)
    {
        uint32_t cr2;

        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));

        /* CONTROLLED PAGE FAULT RECOVERY */

        if (expected_fault_active &&
            cr2 == expected_fault_address) {

            expected_fault_active = 0;
            page_fault_caught = 1;
            last_page_fault_error = frame->err_code;

            frame->eip = page_fault_recovery_eip;

            return;
        }


        print_text(7, 0, "PAGE FAULT DIAGNOSTICS");

        print_text(8, 0, "Fault Address (CR2):");
        print_hex(8, 25, cr2);

        print_text(9, 0, "Page Present:");

        if (frame->err_code & 1)
            print_text(9, 25, "Protection Violation");
        else
            print_text(9, 25, "Page Not Present");

        print_text(10, 0, "Access Type:");

        if (frame->err_code & 2)
            print_text(10, 25, "Write");
        else
            print_text(10, 25, "Read");

        print_text(11, 0, "Privilege:");

        if (frame->err_code & 4)
            print_text(11, 25, "User Mode");
        else
            print_text(11, 25, "Supervisor Mode");

        print_text(12, 0, "Reserved Bit Violation:");

        if (frame->err_code & 8)
            print_text(12, 25, "Yes");
        else
            print_text(12, 25, "No");

        print_text(13, 0, "Instruction Fetch:");

        if (frame->err_code & 16)
            print_text(13, 25, "Yes");
        else
            print_text(13, 25, "No");

        print_text(14, 0, "Protection Key Fault:");

        if (frame->err_code & 32)
            print_text(14, 25, "Yes");
        else
            print_text(14, 25, "No");

        print_text(15, 0, "Shadow Stack Fault:");

        if (frame->err_code & 64)
            print_text(15, 25, "Yes");
        else
            print_text(15, 25, "No");
    }

    print_text(17, 0, "CampusOS: Kernel halted due to CPU exception.");

    for (;;)
    {
        __asm__ volatile ("hlt");
    }
}
void double_fault_handler(void)
{
    __asm__ volatile ("cli");

    print_text(0, 0, "========== CampusOS DOUBLE FAULT ==========");

    print_text(2, 0, "Exception Number: 8");
    print_text(3, 0, "Exception Name: Double Fault");

    print_text(5, 0, "CPU detected a critical exception condition.");

    print_text(7, 0, "CampusOS: Emergency handler activated.");

    print_text(9, 0, "Kernel halted.");

    for (;;)
    {
        __asm__ volatile ("hlt");
    }
}