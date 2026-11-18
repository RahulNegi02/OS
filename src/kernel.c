#include <stdint.h>

#include "gdt.h"
#include "interrupts.h"
#include "vmm.h"
#include "keyboard.h"
#include "pmm.h"
#include "multiboot.h"
#include "heap.h"
#include "process.h"

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
    for (int i = 0; text[i] != '\0' && col + i < 80; i++)
        put_char(row, col + i, text[i], 0x0A);
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

        put_char(
            row,
            col + i,
            digits[count - 1 - i],
            0x0B
        );
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
        print_text("memory - RAM statistics", output_row + 5, 0);
        print_text("memtest - Test PMM", output_row + 6, 0);
        print_text("vmtest - Test VMM", output_row + 7, 0);
        print_text("heap - Heap statistics", output_row + 8, 0);
        print_text("heaptest - Test heap", output_row + 9, 0);

        shell_row = output_row + 11;
    }

    else if (string_equal(command, "clear")) {

        clear_shell();
    }

    else if (string_equal(command, "ticks")) {

        print_text("Current timer ticks:", output_row, 0);

        print_number(
            timer_get_ticks(),
            output_row,
            21
        );

        shell_row = output_row + 2;
    }

    else if (string_equal(command, "about")) {

        print_text(
            "CampusOS - College Lab Operating System",
            output_row, 0
        );

        print_text(
            "Architecture: x86 32-bit",
            output_row + 1, 0
        );

        print_text(
            "Kernel: Custom freestanding kernel",
            output_row + 2, 0
        );

        shell_row = output_row + 4;
    }

    else if (string_equal(command, "memory")) {

        uint32_t total = pmm_get_total_frames();
        uint32_t free = pmm_get_free_frames();

        print_text("Total frames:", output_row, 0);
        print_number(total, output_row, 14);

        print_text("Free frames:", output_row + 1, 0);
        print_number(free, output_row + 1, 14);

        print_text("Used frames:", output_row + 2, 0);
        print_number(total - free, output_row + 2, 14);

        print_text("Page tables:", output_row + 3, 0);
        print_number(
            vmm_get_used_page_tables(),
            output_row + 3, 14
        );

        shell_row = output_row + 5;
    }

    else if (string_equal(command, "memtest")) {

        uint32_t before = pmm_get_free_frames();

        uint32_t first = pmm_alloc_frame();
        uint32_t second = pmm_alloc_frame();

        int passed =
            (first != 0 &&
             second != 0 &&
             first != second);

        if (first)
            pmm_free_frame(first);

        if (second)
            pmm_free_frame(second);

        if (pmm_get_free_frames() != before)
            passed = 0;

        if (passed)
            print_text("PMM allocation test: PASS", output_row, 0);
        else
            print_text("PMM allocation test: FAIL", output_row, 0);

        shell_row = output_row + 2;
    }

    else if (string_equal(command, "vmtest")) {

        const uint32_t test_address = 0x40000000U;
        const uint32_t test_value = 0xC0DEF00DU;

        uint32_t before = pmm_get_free_frames();
        uint32_t tables_before = vmm_get_used_page_tables();

        int passed = 0;

        if (vmm_alloc_page(
                test_address,
                VMM_PAGE_WRITABLE)) {

            volatile uint32_t *memory =
                (volatile uint32_t *)test_address;

            *memory = test_value;

            uint32_t physical =
                vmm_get_physical_address(test_address);

            if (*memory == test_value &&
                physical != 0)
                passed = 1;

            if (!vmm_free_page(test_address))
                passed = 0;
        }

        if (pmm_get_free_frames() != before)
            passed = 0;

        if (vmm_get_used_page_tables() != tables_before)
            passed = 0;

        if (passed)
            print_text("VMM mapping test: PASS", output_row, 0);
        else
            print_text("VMM mapping test: FAIL", output_row, 0);

        shell_row = output_row + 2;
    }

    else if (string_equal(command, "heap")) {

        uint32_t total = heap_get_total_pages();
        uint32_t used = heap_get_used_pages();

        print_text("Kernel Heap Statistics:", output_row, 0);

        print_text("Total pages:", output_row + 1, 0);
        print_number(total, output_row + 1, 14);

        print_text("Used pages:", output_row + 2, 0);
        print_number(used, output_row + 2, 14);

        print_text("Free pages:", output_row + 3, 0);
        print_number(total - used, output_row + 3, 14);

        shell_row = output_row + 5;
    }

    else if (string_equal(command, "heaptest")) {

        uint32_t before = heap_get_allocation_count();

        print_text("===== HEAP DIAGNOSTICS =====", output_row, 0);

        print_text("Free PMM frames:", output_row + 1, 0);
        print_number(pmm_get_free_frames(), output_row + 1, 18);

        print_text("VMM page tables:", output_row + 2, 0);
        print_number(vmm_get_used_page_tables(), output_row + 2, 18);

        uint8_t *a = (uint8_t *)kmalloc(100);

        print_text("100 bytes:", output_row + 3, 0);
        print_text(a ? "PASS" : "FAIL", output_row + 3, 18);

        uint8_t *b = (uint8_t *)kmalloc(5000);

        print_text("5000 bytes:", output_row + 4, 0);
        print_text(b ? "PASS" : "FAIL", output_row + 4, 18);

        uint8_t *c = (uint8_t *)kmalloc(9000);

        print_text("9000 bytes:", output_row + 5, 0);
        print_text(c ? "PASS" : "FAIL", output_row + 5, 18);

        int passed = (a && b && c);

        if (a) {
            a[0] = 0xAA;
            a[99] = 0x55;
            if (a[0] != 0xAA || a[99] != 0x55)
                passed = 0;
        }

        if (b) {
            b[0] = 0x12;
            b[4999] = 0x34;
            if (b[0] != 0x12 || b[4999] != 0x34)
                passed = 0;
        }

        if (c) {
            c[0] = 0x56;
            c[8999] = 0x78;
            if (c[0] != 0x56 || c[8999] != 0x78)
                passed = 0;
        }

        if (a) kfree(a);
        if (b) kfree(b);
        if (c) kfree(c);

        print_text("Allocations restored:", output_row + 6, 0);
        print_text(
            heap_get_allocation_count() == before ? "PASS" : "FAIL",
            output_row + 6, 21
        );

        print_text("Final PMM frames:", output_row + 7, 0);
        print_number(pmm_get_free_frames(), output_row + 7, 18);

        print_text("Final VMM tables:", output_row + 8, 0);
        print_number(vmm_get_used_page_tables(), output_row + 8, 18);

        print_text(
            passed ? "HEAP TEST: PASS" : "HEAP TEST: FAIL",
            output_row + 9, 0
        );

        shell_row = output_row + 11;
    }

    else if (command[0] == '\0') {

        shell_row = output_row + 1;
    }

    else {

        print_text(
            "Unknown command. Type help.",
            output_row, 0
        );

        shell_row = output_row + 2;
    }

    if (shell_row >= 23)
        clear_shell();

    show_prompt();
}



/* CampusOS Kernel Memory Stress Test */

static void stress_print(int row, const char *message)
{
    volatile uint16_t *video =
        (volatile uint16_t *)0xB8000;

    int i = 0;

    while (i < 80) {
        video[row * 80 + i] =
            (uint16_t)(0x0F00 | ' ');
        i++;
    }

    i = 0;
    while (message[i] && i < 80) {
        video[row * 80 + i] =
            (uint16_t)(0x0F00 | (uint8_t)message[i]);
        i++;
    }
}


static void kernel_memory_stress_test(void)
{
    void *blocks[64];
    void *small[32];

    uint32_t baseline_count = heap_get_allocation_count();
    uint32_t baseline_pages = heap_get_used_pages();

    uint32_t i, round;
    uint32_t allocated = 0;
    uint32_t block_size = 256u * 1024u;

    int stress_ok = 1;
    int small_integrity_ok = 1;
    int large_integrity_ok = 1;
    int exhaustion_ok = 0;

    stress_print(14, "===== CAMPUSOS MEMORY STRESS TEST =====");

    /* TEST 1: Repeated allocation/free cycles */

    for (round = 0; round < 50; round++) {

        for (i = 0; i < 32; i++) {

            uint32_t size = 1u + ((i * 977u) % 8192u);

            small[i] = kmalloc(size);

            if (!small[i]) {
                stress_ok = 0;
                break;
            }

            uint8_t *memory = (uint8_t *)small[i];

            memory[0] = (uint8_t)i;

            if (size > 1) {
                memory[size - 1] = (uint8_t)(255u - i);
            }

            if (memory[0] != (uint8_t)i) {
                small_integrity_ok = 0;
            }

            if (size > 1 &&
                memory[size - 1] != (uint8_t)(255u - i)) {
                small_integrity_ok = 0;
            }
        }

        while (i > 0) {
            i--;
            kfree(small[i]);
        }

        if (heap_get_allocation_count() != baseline_count ||
            heap_get_used_pages() != baseline_pages) {
            stress_ok = 0;
            break;
        }
    }

    stress_print(15, stress_ok ?
        "PASS: 50 allocation/free cycles" :
        "FAIL: Allocation/free cycles");

    stress_print(16, small_integrity_ok ?
        "PASS: Small memory write/read integrity" :
        "FAIL: Small memory integrity");

    /* TEST 2: Exhaust the heap allocator */

    allocated = 0;

    for (i = 0; i < 64; i++) {

        blocks[i] = kmalloc(block_size);

        if (!blocks[i]) {
            exhaustion_ok = 1;
            break;
        }

        allocated++;

        uint8_t *memory = (uint8_t *)blocks[i];

        memory[0] = 0xA5;
        memory[block_size / 2] = 0xC3;
        memory[block_size - 1] = 0x5A;

        if (memory[0] != 0xA5 ||
            memory[block_size / 2] != 0xC3 ||
            memory[block_size - 1] != 0x5A) {
            large_integrity_ok = 0;
        }
    }

    /* If 64 allocations unexpectedly succeed, try one more. */

    if (!exhaustion_ok) {
        void *extra = kmalloc(1);

        if (!extra) {
            exhaustion_ok = 1;
        } else {
            kfree(extra);
        }
    }

    stress_print(17, exhaustion_ok && allocated > 0 ?
        "PASS: Allocation failure returns NULL" :
        "FAIL: Allocation exhaustion");

    stress_print(18, large_integrity_ok ?
        "PASS: Large block memory integrity" :
        "FAIL: Large block integrity");

    /* TEST 3: Release every large allocation */

    while (allocated > 0) {
        allocated--;
        kfree(blocks[allocated]);
    }

    if (heap_get_allocation_count() != baseline_count ||
        heap_get_used_pages() != baseline_pages) {
        stress_ok = 0;
    }

    stress_print(19, stress_ok ?
        "PASS: No memory leaks detected" :
        "FAIL: Memory leak detected");

    stress_print(20,
        (stress_ok && small_integrity_ok &&
         large_integrity_ok && exhaustion_ok) ?
        "RESULT: ALL MEMORY TESTS PASSED" :
        "RESULT: MEMORY TEST FAILURE");
}



/* ==============================================
   CAMPUSOS PAGE FAULT AND MEMORY PROTECTION TESTS
   ============================================== */

#define PF_TEST_PAGE       0xC1000000U
#define PF_INVALID_PAGE    0xC1001000U

static int test_protected_write(uint32_t address)
{
    page_fault_arm(address, (uint32_t)&&recovery);

    *(volatile uint8_t *)address = 0x5A;

    page_fault_disarm();
    return 0;

recovery:
    return page_fault_was_caught();
}

static int test_invalid_read(uint32_t address)
{
    page_fault_arm(address, (uint32_t)&&recovery);

    volatile uint8_t value = *(volatile uint8_t *)address;
    (void)value;

    page_fault_disarm();
    return 0;

recovery:
    return page_fault_was_caught();
}

static void kernel_page_fault_test(void)
{
    int protection_ok = 0;
    int write_fault_ok = 0;
    int invalid_fault_ok = 0;
    int recovery_ok = 1;
    int cleanup_ok = 1;

    uint32_t error;

    stress_print(14, "===== PAGE FAULT AND PROTECTION TEST =====");

    /*
     * TEST 1:
     * Allocate a page without write permission.
     */

    if (vmm_alloc_page(PF_TEST_PAGE, VMM_PAGE_PRESENT)) {

        /* Reading from a read-only page must be allowed. */

        volatile uint8_t read_value =
            *(volatile uint8_t *)PF_TEST_PAGE;

        (void)read_value;

        protection_ok = 1;

        /*
         * TEST 2:
         * Attempt writing to read-only page.
         * Expected: Present + Write fault.
         */

        write_fault_ok = test_protected_write(PF_TEST_PAGE);

        error = page_fault_get_last_error();

        if ((error & 3U) != 3U)
            write_fault_ok = 0;

        /*
         * Restore write permission and verify access.
         */

        if (write_fault_ok &&
            vmm_protect_page(PF_TEST_PAGE, 1)) {

            *(volatile uint8_t *)PF_TEST_PAGE = 0x5A;

            if (*(volatile uint8_t *)PF_TEST_PAGE != 0x5A)
                recovery_ok = 0;

        } else {
            recovery_ok = 0;
        }

        if (!vmm_free_page(PF_TEST_PAGE))
            cleanup_ok = 0;

    } else {
        protection_ok = 0;
        write_fault_ok = 0;
        recovery_ok = 0;
    }

    stress_print(15, protection_ok ?
        "PASS: Read-only page can be read" :
        "FAIL: Read-only page test");

    stress_print(16, write_fault_ok ?
        "PASS: Write protection fault detected" :
        "FAIL: Write protection fault");

    /*
     * TEST 3:
     * Read from an unmapped virtual page.
     * Expected: Page not present + Read.
     */

    invalid_fault_ok = test_invalid_read(PF_INVALID_PAGE);

    error = page_fault_get_last_error();

    if (error & 1U)
        invalid_fault_ok = 0;

    stress_print(17, invalid_fault_ok ?
        "PASS: Invalid address fault detected" :
        "FAIL: Invalid address test");

    stress_print(18, recovery_ok ?
        "PASS: Fault recovery successful" :
        "FAIL: Fault recovery");

    stress_print(19, cleanup_ok ?
        "PASS: Protection test cleanup" :
        "FAIL: Protection cleanup");

    stress_print(20,
        (protection_ok && write_fault_ok &&
         invalid_fault_ok && recovery_ok && cleanup_ok) ?
        "RESULT: ALL PAGE FAULT TESTS PASSED" :
        "RESULT: PAGE FAULT TEST FAILURE");
}



/* =========================================
   CAMPUSOS PROCESS MANAGEMENT TEST
   ========================================= */

static void kernel_process_test(void)
{
    int creation_ok = 0;
    int identity_ok = 0;
    int cleanup_ok = 0;
    int heap_ok = 0;

    uint32_t baseline =
        heap_get_allocation_count();

    process_t *p1 = 0;
    process_t *p2 = 0;
    process_t *p3 = 0;

    stress_print(5, "===== PROCESS MANAGEMENT TEST =====");

    if (!process_init()) {
        stress_print(6, "FAIL: Process initialization");
        return;
    }

    p1 = process_create((void (*)(void))kernel_process_test);
    p2 = process_create((void (*)(void))kernel_process_test);
    p3 = process_create((void (*)(void))kernel_process_test);

    creation_ok = (p1 && p2 && p3);

    identity_ok = creation_ok;

    if (identity_ok) {

        identity_ok =
            p1->pid != p2->pid &&
            p2->pid != p3->pid &&
            p1->pid != p3->pid;

        identity_ok = identity_ok &&
            p1->state == PROCESS_READY &&
            p2->state == PROCESS_READY &&
            p3->state == PROCESS_READY;

        identity_ok = identity_ok &&
            p1->kernel_stack &&
            p2->kernel_stack &&
            p3->kernel_stack;

        identity_ok = identity_ok &&
            p1->kernel_stack != p2->kernel_stack &&
            p2->kernel_stack != p3->kernel_stack &&
            p1->kernel_stack != p3->kernel_stack;

        identity_ok = identity_ok &&
            !(p1->kernel_stack_top & 0x0F) &&
            !(p2->kernel_stack_top & 0x0F) &&
            !(p3->kernel_stack_top & 0x0F);

        identity_ok = identity_ok &&
            process_get_count() == 3 &&
            process_get_first() == p1;
    }

    stress_print(6, process_get_count() == 3 ?
        "PASS: Three processes created" :
        "FAIL: Process creation");

    stress_print(7, creation_ok ?
        "PASS: Process records allocated" :
        "FAIL: Process allocation");

    stress_print(8, identity_ok ?
        "PASS: Unique PIDs and separate stacks" :
        "FAIL: PID/stack validation");

    /* Destroy every successfully created process. */

    cleanup_ok = 1;

    if (p1 && !process_destroy(p1->pid))
        cleanup_ok = 0;

    if (p2 && !process_destroy(p2->pid))
        cleanup_ok = 0;

    if (p3 && !process_destroy(p3->pid))
        cleanup_ok = 0;

    cleanup_ok = cleanup_ok &&
        process_get_count() == 0 &&
        process_get_first() == 0;

    stress_print(9, cleanup_ok ?
        "PASS: Process destruction" :
        "FAIL: Process destruction");

    heap_ok =
        heap_get_allocation_count() == baseline;

    stress_print(10, heap_ok ?
        "PASS: Heap allocations restored" :
        "FAIL: Heap allocation leak");

    stress_print(11,
        (creation_ok && identity_ok &&
         cleanup_ok && heap_ok) ?
        "RESULT: ALL PROCESS TESTS PASSED" :
        "RESULT: PROCESS TEST FAILURE");
}

void kernel_main(uint32_t multiboot_magic,
                 uint32_t multiboot_info_addr)
{
    __asm__ volatile ("cli");

    if (multiboot_magic != MULTIBOOT_BOOTLOADER_MAGIC) {

        print_text("ERROR: Invalid Multiboot magic", 0, 0);

        while (1)
            __asm__ volatile ("hlt");
    }

    if (multiboot_info_addr == 0) {

        print_text("ERROR: Missing Multiboot information", 0, 0);

        while (1)
            __asm__ volatile ("hlt");
    }

    gdt_init();
    idt_init();

    pmm_init(multiboot_info_addr);
    vmm_init();
    heap_init();
    process_init();
    kernel_memory_stress_test();
    kernel_process_test();
    /* Page fault test temporarily disabled */

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

    while (1) {

        uint32_t ticks = timer_get_ticks();

        if (ticks != last_ticks) {

            last_ticks = ticks;

            clear_line(1);

            print_text("Timer ticks:", 1, 0);

            print_number(ticks, 1, 13);
        }

        if (keyboard_try_read_char(&key)) {

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
