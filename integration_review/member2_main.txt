
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

#include "pmm.h"
#include "serial.h"
#include "vmm.h"
#include "idt.h"
#include "heap.h"

extern int test_write_readonly(volatile uint8_t *address);
extern char test_write_readonly_recovery[];
extern int test_execute_nx(void *address);
extern char test_execute_nx_recovery[];

// Limine base revision request.
__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] =
    LIMINE_BASE_REVISION(6);

// Framebuffer request.
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

// HHDM request.
__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

// Limine request markers.
__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] =
    LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] =
    LIMINE_REQUESTS_END_MARKER;


// Halt the CPU.
static void hcf(void)
{
    for (;;) {
        asm volatile ("hlt");
    }
}

// Read the current page-table root address from CR3.
static uint64_t read_cr3(void)
{
    uint64_t value;

    __asm__ volatile ("mov %%cr3, %0" : "=r"(value));

    return value;
}

// Print a 64-bit value in hexadecimal.
static void serial_write_hex(uint64_t value)
{
    static const char digits[] = "0123456789ABCDEF";

    serial_writestring("0x");

    for (int shift = 60; shift >= 0; shift -= 4) {
        serial_write_char(digits[(value >> shift) & 0xF]);
    }
}

// Scale an 8-bit colour channel to the framebuffer channel size.
static uint32_t fb_channel(
    uint8_t value,
    uint8_t mask_size,
    uint8_t mask_shift
)
{
    uint64_t max = ((uint64_t)1 << mask_size) - 1;

    return (uint32_t)(
        ((uint64_t)value * max / 255) << mask_shift
    );
}

// Build a pixel using the framebuffer's RGB layout.
static uint32_t fb_pixel(
    struct limine_framebuffer *fb,
    uint8_t red,
    uint8_t green,
    uint8_t blue
)
{
    return fb_channel(red, fb->red_mask_size, fb->red_mask_shift)
         | fb_channel(green, fb->green_mask_size, fb->green_mask_shift)
         | fb_channel(blue, fb->blue_mask_size, fb->blue_mask_shift);
}

// Display a blue-green gradient on the framebuffer.
static void fb_pattern(struct limine_framebuffer *fb)
{
    volatile uint32_t *fb_ptr = fb->address;

    for (size_t y = 0; y < fb->height; y++) {
        for (size_t x = 0; x < fb->width; x++) {
            uint8_t nX = (uint8_t)(x * 255 / fb->width);
            uint8_t nY = (uint8_t)(y * 255 / fb->height);

            fb_ptr[y * (fb->pitch / 4) + x] =
                fb_pixel(fb, 0, nY, nX);
        }
    }
}

// Kernel entry point.
void kmain(void)
{
    // Check Limine base revision support.
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) {
        hcf();
    }

    // Initialize serial output.
    serial_init();
    serial_writestring("[CampusOS] Kernel started\n");

    if (!idt_init()) {
        serial_writestring("[IDT] Initialization failed\n");
        hcf();
    }

    // Inspect the current paging state.
    serial_writestring("[VMM] Inspecting current paging state\n");

    uint64_t cr3 = read_cr3();

    serial_writestring("[VMM] CR3 = ");
    serial_write_hex(cr3);
    serial_writestring("\n");

    // Get and validate the HHDM response.
    struct limine_hhdm_response *hhdm = hhdm_request.response;

    if (hhdm == NULL) {
        serial_writestring("[VMM] HHDM request failed\n");
        hcf();
    }

    // Print the HHDM offset.
    serial_writestring("[VMM] HHDM offset = ");
    serial_write_hex(hhdm->offset);
    serial_writestring("\n");

    // Enable CPU NX protection before using NX page-table entries.
    bool nx_available = vmm_enable_nx();

    if (nx_available) {
        serial_writestring("[VMM] CPU NX protection enabled\n");
    } else {
        serial_writestring("[VMM] CPU NX protection unavailable\n");
    }

    // Test translation of the kernel entry point.
    serial_writestring("[VMM] Testing kernel address translation\n");

    uint64_t kernel_virtual = (uint64_t)(uintptr_t)&kmain;
    uint64_t kernel_physical = 0;

    serial_writestring("[VMM] Kernel virtual address = ");
    serial_write_hex(kernel_virtual);
    serial_writestring("\n");

    if (vmm_translate(
            kernel_virtual,
            hhdm->offset,
            &kernel_physical)) {

        serial_writestring("[VMM] Kernel physical address = ");
        serial_write_hex(kernel_physical);
        serial_writestring("\n");

        serial_writestring("[VMM] Kernel translation passed\n");

    } else {
        serial_writestring("[VMM] Kernel translation failed\n");
        hcf();
    }

    // Initialize the physical memory manager BEFORE allocating frames.
    serial_writestring("[PMM] Initializing physical memory manager\n");

    if (!pmm_init()) {
        serial_writestring("[PMM] Initialization failed\n");
        hcf();
    }

    // Record available physical memory before running tests.
uint64_t pmm_free_baseline = pmm_get_free_frames();

serial_writestring("[MEMORY] Initial free frames = ");
serial_write_hex(pmm_free_baseline);
serial_writestring("\n");

    serial_writestring("[PMM] Initialization successful\n");

    // Test mapping and unmapping one 4 KiB page.
    const uint64_t test_virtual = 0x0000400000000000ULL;
    uint64_t existing_physical = 0;

    if (vmm_translate(
            test_virtual,
            hhdm->offset,
            &existing_physical)) {
        serial_writestring("[VMM] Test virtual address is already mapped\n");
        hcf();
    }

    uint64_t test_frame = pmm_alloc_frame();

    if (test_frame == 0) {
        serial_writestring("[VMM] Could not allocate test frame\n");
        hcf();
    }

    if (!vmm_map_page(
            test_virtual,
            test_frame,
            hhdm->offset)) {
        serial_writestring("[VMM] Mapping test failed\n");
        hcf();
    }

    // Write and read through the newly mapped virtual address.
    volatile uint64_t *test_pointer =
        (volatile uint64_t *)(uintptr_t)test_virtual;

    *test_pointer = 0xCAFE1234ABCDEF00ULL;

    if (*test_pointer != 0xCAFE1234ABCDEF00ULL) {
        serial_writestring("[VMM] Read/write test failed\n");
        hcf();
    }

    // Verify the mapping translates to the allocated frame.
    uint64_t translated_physical = 0;

    if (!vmm_translate(
            test_virtual,
            hhdm->offset,
            &translated_physical) ||
        translated_physical != test_frame) {
        serial_writestring("[VMM] Mapping translation test failed\n");
        hcf();
    }

    // Remove the mapping.
    uint64_t unmapped_physical = 0;

    if (!vmm_unmap_page(
            test_virtual,
            hhdm->offset,
            &unmapped_physical) ||
        unmapped_physical != test_frame) {
        serial_writestring("[VMM] Unmapping test failed\n");
        hcf();
    }

    // Confirm that the virtual address is no longer mapped.
    if (vmm_translate(
            test_virtual,
            hhdm->offset,
            &existing_physical)) {
        serial_writestring("[VMM] Address remains mapped after unmap\n");
        hcf();
    }

    // Release the physical frame used by the test.
    if (!pmm_free_frame(test_frame)) {
        serial_writestring("[VMM] Test frame release failed\n");
        hcf();
    }

    serial_writestring(
        "[VMM] Map, read/write, translate and unmap tests passed\n"
    );

    // PMM allocation/free tests.
    uint64_t initial_free = pmm_get_free_frames();

    if (initial_free < 2) {
        serial_writestring("[PMM] Not enough free frames\n");
        hcf();
    }

    uint64_t frame1 = pmm_alloc_frame();
    uint64_t frame2 = pmm_alloc_frame();

    if (frame1 == 0 || frame2 == 0 || frame1 == frame2) {
        serial_writestring("[PMM] Allocation test failed\n");
        hcf();
    }

    if (frame1 % PMM_PAGE_SIZE != 0 ||
        frame2 % PMM_PAGE_SIZE != 0) {
        serial_writestring("[PMM] Alignment test failed\n");
        hcf();
    }

    if (pmm_get_free_frames() != initial_free - 2) {
        serial_writestring("[PMM] Allocation count test failed\n");
        hcf();
    }

    if (!pmm_free_frame(frame1)) {
        serial_writestring("[PMM] Free test failed\n");
        hcf();
    }

    if (pmm_get_free_frames() != initial_free - 1) {
        serial_writestring("[PMM] Free count test failed\n");
        hcf();
    }

    // A double-free must fail.
    if (pmm_free_frame(frame1)) {
        serial_writestring("[PMM] Double-free test failed\n");
        hcf();
    }

    if (pmm_get_free_frames() != initial_free - 1) {
        serial_writestring("[PMM] Double-free count test failed\n");
        hcf();
    }

    // Check that the freed frame can be reused.
    uint64_t frame3 = pmm_alloc_frame();

    if (frame3 != frame1) {
        serial_writestring("[PMM] Frame reuse test failed\n");
        hcf();
    }

    if (!pmm_free_frame(frame2)) {
        serial_writestring("[PMM] Free frame2 failed\n");
        hcf();
    }

    if (!pmm_free_frame(frame3)) {
        serial_writestring("[PMM] Free frame3 failed\n");
        hcf();
    }

    if (pmm_get_free_frames() != initial_free) {
        serial_writestring("[PMM] Free count restoration failed\n");
        hcf();
    }

    serial_writestring("[PMM] Allocation and free tests passed\n");

    // Test invalid free requests.
    uint64_t free_before_invalid_tests = pmm_get_free_frames();

    if (pmm_free_frame(1)) {
        serial_writestring("[PMM] Unaligned-free test failed\n");
        hcf();
    }

    if (pmm_free_frame(0)) {
        serial_writestring("[PMM] Zero-free test failed\n");
        hcf();
    }

    if (pmm_free_frame(4ULL * 1024 * 1024 * 1024)) {
        serial_writestring("[PMM] Out-of-range-free test failed\n");
        hcf();
    }

    if (pmm_free_frame(PMM_PAGE_SIZE)) {
        serial_writestring("[PMM] Unallocated-free test failed\n");
        hcf();
    }

    if (pmm_get_free_frames() != free_before_invalid_tests) {
        serial_writestring("[PMM] Invalid-free count test failed\n");
        hcf();
    }

    serial_writestring("[PMM] Invalid-free tests passed\n");
    serial_writestring("[PMM] All tests passed\n");

    // Test VMM rejection of invalid mapping requests.
    serial_writestring("[VMM] Starting invalid-operation tests\n");

    const uint64_t edge_test_virtual = 0x0000400000000000ULL;
    uint64_t edge_test_physical = 0;

    // Test 1: Reject mapping flags that do not include VMM_READ.
    if (vmm_map_page_flags(
            edge_test_virtual,
            0,
            hhdm->offset,
            0)) {
        serial_writestring("[VMM] Invalid-flags test FAILED\n");
        hcf();
    }

    serial_writestring("[VMM] Invalid-flags test passed\n");

    // Test 2: Reject an unaligned virtual address.
    if (vmm_map_page_flags(
            edge_test_virtual + 1,
            0,
            hhdm->offset,
            VMM_READ)) {
        serial_writestring("[VMM] Unaligned-address test FAILED\n");
        hcf();
    }

    serial_writestring("[VMM] Unaligned-address test passed\n");

    // Test 3: Map a page, then confirm duplicate mapping is rejected.
    edge_test_physical = pmm_alloc_frame();

    if (edge_test_physical == 0) {
        serial_writestring("[VMM] Duplicate-map test allocation FAILED\n");
        hcf();
    }

    if (!vmm_map_page_flags(
            edge_test_virtual,
            edge_test_physical,
            hhdm->offset,
            VMM_READ | VMM_WRITE)) {
        serial_writestring("[VMM] Initial mapping test FAILED\n");
        pmm_free_frame(edge_test_physical);
        hcf();
    }

    // This must fail because the virtual address is already mapped.
    if (vmm_map_page_flags(
            edge_test_virtual,
            edge_test_physical,
            hhdm->offset,
            VMM_READ | VMM_WRITE)) {
        serial_writestring("[VMM] Duplicate-map rejection FAILED\n");
        hcf();
    }

    // Confirm the original mapping still points to the allocated frame.
    uint64_t edge_translated_physical = 0;

    if (!vmm_translate(
            edge_test_virtual,
            hhdm->offset,
            &edge_translated_physical) ||
        edge_translated_physical != edge_test_physical) {
        serial_writestring("[VMM] Mapping-preservation test FAILED\n");
        hcf();
    }

    // Unmap the original mapping.
    uint64_t edge_unmapped_physical = 0;

    if (!vmm_unmap_page(
            edge_test_virtual,
            hhdm->offset,
            &edge_unmapped_physical) ||
        edge_unmapped_physical != edge_test_physical) {
        serial_writestring("[VMM] Cleanup unmap FAILED\n");
        hcf();
    }

    if (!pmm_free_frame(edge_test_physical)) {
        serial_writestring("[VMM] Cleanup frame-free FAILED\n");
        hcf();
    }

    serial_writestring("[VMM] Duplicate-map rejection passed\n");
    serial_writestring("[VMM] Mapping-preservation test passed\n");
    serial_writestring("[VMM] Invalid-operation tests passed\n");

    // Test VMM rollback when page-table allocation fails.
    serial_writestring("[VMM] Starting rollback test\n");

    const uint64_t rollback_virtual = 0x0000500000000000ULL;
    const uint64_t rollback_physical = 0x0000000000000000ULL;
    uint64_t rollback_free_before = pmm_get_free_frames();

    // Fail the next PMM allocation.
    pmm_test_fail_allocation_after(0);

    // The mapping should fail because creating a page table fails.
    if (vmm_map_page_flags(
            rollback_virtual,
            rollback_physical,
            hhdm->offset,
            VMM_READ | VMM_WRITE)) {
        serial_writestring("[VMM] Rollback test FAILED: mapping succeeded\n");
        hcf();
    }

    // No page-table frame should have been allocated.
    if (pmm_get_free_frames() != rollback_free_before) {
        serial_writestring("[VMM] Rollback test FAILED: frame count changed\n");
        hcf();
    }

    serial_writestring("[VMM] Immediate allocation-failure test passed\n");

    // Test rollback when the second page-table allocation fails.
    serial_writestring("[VMM] Testing rollback after one allocation\n");

    const uint64_t rollback_second_va = 0x0000500000000000ULL;
    uint64_t rollback_second_free_before = pmm_get_free_frames();

    // Allow one allocation, then force the next allocation to fail.
    pmm_test_fail_allocation_after(1);

    if (vmm_map_page_flags(
            rollback_second_va,
            0,
            hhdm->offset,
            VMM_READ | VMM_WRITE)) {
        serial_writestring("[VMM] Rollback test FAILED: second-allocation failure\n");
        hcf();
    }

    if (pmm_get_free_frames() != rollback_second_free_before) {
        serial_writestring("[VMM] Rollback test FAILED: first table leaked\n");
        hcf();
    }

    serial_writestring("[VMM] One-table rollback test passed\n");

    // Test rollback when the third page-table allocation fails.
    serial_writestring("[VMM] Testing rollback after two allocations\n");

    const uint64_t rollback_third_va = 0x0000580000000000ULL;
    uint64_t rollback_third_free_before = pmm_get_free_frames();

    // Allow two allocations, then force the third to fail.
    pmm_test_fail_allocation_after(2);

    if (vmm_map_page_flags(
            rollback_third_va,
            0,
            hhdm->offset,
            VMM_READ | VMM_WRITE)) {
        serial_writestring("[VMM] Rollback test FAILED: third-allocation failure\n");
        hcf();
    }

    if (pmm_get_free_frames() != rollback_third_free_before) {
        serial_writestring("[VMM] Rollback test FAILED: tables leaked\n");
        hcf();
    }

    serial_writestring("[VMM] Two-table rollback test passed\n");
    serial_writestring("[VMM] All rollback tests passed\n");

    // Test that unmapping the last page reclaims empty page tables.
    serial_writestring("[VMM] Starting page-table reclamation tests\n");

    const uint64_t reclaim_va = 0x0000600000000000ULL;

    uint64_t reclaim_frame = pmm_alloc_frame();

    if (reclaim_frame == 0) {
        serial_writestring("[VMM] Reclamation test FAILED: frame allocation\n");
        hcf();
    }

    uint64_t reclaim_free_before_map = pmm_get_free_frames();

    if (!vmm_map_page_flags(
            reclaim_va,
            reclaim_frame,
            hhdm->offset,
            VMM_READ | VMM_WRITE)) {
        serial_writestring("[VMM] Reclamation test FAILED: mapping\n");
        pmm_free_frame(reclaim_frame);
        hcf();
    }

    uint64_t reclaim_unmapped_frame = 0;

    if (!vmm_unmap_page(
            reclaim_va,
            hhdm->offset,
            &reclaim_unmapped_frame)) {
        serial_writestring("[VMM] Reclamation test FAILED: unmapping\n");
        hcf();
    }

    if (reclaim_unmapped_frame != reclaim_frame) {
        serial_writestring("[VMM] Reclamation test FAILED: wrong frame\n");
        hcf();
    }

    if (pmm_get_free_frames() != reclaim_free_before_map) {
        serial_writestring("[VMM] Reclamation test FAILED: empty tables leaked\n");
        hcf();
    }

    if (!pmm_free_frame(reclaim_unmapped_frame)) {
        serial_writestring("[VMM] Reclamation test FAILED: freeing data frame\n");
        hcf();
    }

    serial_writestring("[VMM] Empty page-table reclamation test passed\n");

    // Test that removing one of two mappings preserves the other.
    serial_writestring("[VMM] Testing shared page-table preservation\n");

    const uint64_t shared_va1 = 0x0000600000000000ULL;
    const uint64_t shared_va2 = 0x0000600000001000ULL;

    uint64_t shared_frame1 = pmm_alloc_frame();
    uint64_t shared_frame2 = pmm_alloc_frame();

    if (shared_frame1 == 0 || shared_frame2 == 0) {
        serial_writestring("[VMM] Shared-table test FAILED: frame allocation\n");

        if (shared_frame1 != 0) {
            pmm_free_frame(shared_frame1);
        }

        if (shared_frame2 != 0) {
            pmm_free_frame(shared_frame2);
        }

        hcf();
    }

    uint64_t shared_free_before_map = pmm_get_free_frames();

    if (!vmm_map_page_flags(
            shared_va1,
            shared_frame1,
            hhdm->offset,
            VMM_READ | VMM_WRITE)) {
        serial_writestring("[VMM] Shared-table test FAILED: first mapping\n");
        hcf();
    }

    if (!vmm_map_page_flags(
            shared_va2,
            shared_frame2,
            hhdm->offset,
            VMM_READ | VMM_WRITE)) {
        serial_writestring("[VMM] Shared-table test FAILED: second mapping\n");
        hcf();
    }

    uint64_t shared_unmapped_frame1 = 0;

    if (!vmm_unmap_page(
            shared_va1,
            hhdm->offset,
            &shared_unmapped_frame1)) {
        serial_writestring("[VMM] Shared-table test FAILED: first unmap\n");
        hcf();
    }

    // Verify that the second mapping still exists after unmapping the first.
    uint64_t shared_translated_frame2 = 0;

    if (!vmm_translate(
            shared_va2,
            hhdm->offset,
            &shared_translated_frame2) ||
        (shared_translated_frame2 & ~0xFFFULL) != shared_frame2) {
        serial_writestring(
            "[VMM] Shared-table test FAILED: second mapping lost\n"
        );
        hcf();
    }

    serial_writestring(
        "[DEBUG] Second mapping remains translatable\n"
    );

    uint64_t shared_free_after_first_unmap =
        pmm_get_free_frames();

    serial_writestring("[DEBUG] Free frames before mappings: ");
    serial_write_hex(shared_free_before_map);
    serial_writestring("\n");

    // if (shared_free_after_first_unmap != shared_free_before_map) {
    //     serial_writestring("[VMM] Shared-table test FAILED: reclaimed a shared table\n");
    //     hcf();
    // }

    uint64_t shared_unmapped_frame2 = 0;

    if (!vmm_unmap_page(
            shared_va2,
            hhdm->offset,
            &shared_unmapped_frame2)) {
        serial_writestring("[VMM] Shared-table test FAILED: second unmap\n");
        hcf();
    }

    if (pmm_get_free_frames() != shared_free_before_map) {
        serial_writestring(
            "[VMM] Shared-table test FAILED: empty tables not reclaimed\n"
        );
        hcf();
    }

    if (!pmm_free_frame(shared_unmapped_frame1) ||
        !pmm_free_frame(shared_unmapped_frame2)) {
        serial_writestring("[VMM] Shared-table test FAILED: freeing data frames\n");
        hcf();
    }

    serial_writestring("[VMM] Shared page-table preservation test passed\n");
    serial_writestring("[VMM] All page-table reclamation tests passed\n");


    // ==========================================
    // READ-ONLY PAGE PROTECTION TEST
    // ==========================================

    serial_writestring(
        "[VMM] Starting read-only protection test\n"
    );

    const uint64_t readonly_va = 0x0000500000000000ULL;
    uint64_t readonly_existing = 0;
    uint64_t readonly_frame = 0;
    uint64_t readonly_unmapped_frame = 0;

    // Confirm that the virtual address is unused.
    if (vmm_translate(
            readonly_va,
            hhdm->offset,
            &readonly_existing)) {
        serial_writestring(
            "[VMM] Read-only test address already mapped\n"
        );
        hcf();
    }

    // Allocate a physical frame.
    readonly_frame = pmm_alloc_frame();

    if (readonly_frame == 0) {
        serial_writestring(
            "[VMM] Read-only test frame allocation failed\n"
        );
        hcf();
    }

    // Initialize the physical frame through the HHDM.
    volatile uint8_t *readonly_physical =
        (volatile uint8_t *)(uintptr_t)(
            hhdm->offset + readonly_frame
        );

    *readonly_physical = 0x55;

    // Map the page as readable but not writable.
    uint64_t readonly_flags = VMM_READ;

    if (nx_available) {
        readonly_flags |= VMM_NO_EXEC;
    }

    if (!vmm_map_page_flags(
            readonly_va,
            readonly_frame,
            hhdm->offset,
            readonly_flags)) {

        serial_writestring(
            "[VMM] Read-only mapping failed\n"
        );

        pmm_free_frame(readonly_frame);
        hcf();
    }

    serial_writestring(
        "[VMM] Read-only mapping successful\n"
    );

    // Register an expected protection fault.
    // Error bit 0 = protection violation.
    // Error bit 1 = write access.
    if (!idt_expect_page_fault(
            readonly_va,
            (1ULL << 0) | (1ULL << 1),
            (1ULL << 0) | (1ULL << 1),
            (uintptr_t)test_write_readonly_recovery)) {

        serial_writestring(
            "[VMM] Could not register expected page fault\n"
        );

        if (vmm_unmap_page(
                readonly_va,
                hhdm->offset,
                &readonly_unmapped_frame)) {
            pmm_free_frame(readonly_unmapped_frame);
        }

        hcf();
    }

    // Attempt a write. The CPU should generate a page fault.
    int readonly_result = test_write_readonly(
        (volatile uint8_t *)(uintptr_t)readonly_va
    );

    // Cancel any expectation left armed if the write unexpectedly
    // succeeded and the assembly returned normally.
    idt_cancel_expected_page_fault();

    if (readonly_result != 1) {
        serial_writestring(
            "[VMM] Read-only protection test FAILED\n"
        );
        hcf();
    }

    serial_writestring(
        "[VMM] Read-only protection fault recovered\n"
    );

    // Verify that the original byte was not changed.
    volatile uint8_t *readonly_virtual =
        (volatile uint8_t *)(uintptr_t)readonly_va;

    if (*readonly_virtual != 0x55) {
        serial_writestring(
            "[VMM] Read-only data verification FAILED\n"
        );
        hcf();
    }

    serial_writestring(
        "[VMM] Read-only data verification passed\n"
    );

    // Unmap the virtual page.
    if (!vmm_unmap_page(
            readonly_va,
            hhdm->offset,
            &readonly_unmapped_frame)) {

        serial_writestring(
            "[VMM] Read-only unmapping failed\n"
        );
        hcf();
    }

    // Free the physical frame.
    if (readonly_unmapped_frame != readonly_frame ||
        !pmm_free_frame(readonly_unmapped_frame)) {

        serial_writestring(
            "[VMM] Read-only frame release failed\n"
        );
        hcf();
    }

    serial_writestring(
        "[VMM] Read-only protection test passed\n"
    );


    // ==========================================
    // KERNEL HEAP INITIALIZATION AND TESTS
    // ==========================================

    serial_writestring("[HEAP] Initializing kernel heap\n");

    if (!heap_init(hhdm->offset)) {
        serial_writestring(
            "[HEAP] Initialization failed: address range unavailable\n"
        );
        hcf();
    }

    serial_writestring("[HEAP] Initialization passed\n");

    // Test 1: Small allocation, zero initialization, and read/write.
    uint8_t *heap_test = (uint8_t *)kmalloc(64);

    if (heap_test == NULL) {
        serial_writestring("[HEAP] Small allocation failed\n");
        hcf();
    }

    for (uint64_t i = 0; i < 64; i++) {
        if (heap_test[i] != 0) {
            serial_writestring(
                "[HEAP] Zero-initialization test failed\n"
            );
            hcf();
        }

        heap_test[i] = (uint8_t)(i + 1);
    }

    for (uint64_t i = 0; i < 64; i++) {
        if (heap_test[i] != (uint8_t)(i + 1)) {
            serial_writestring("[HEAP] Read/write test failed\n");
            hcf();
        }
    }

    serial_writestring(
        "[HEAP] Small allocation and read/write passed\n"
    );

    // Test 2: Free and reuse the same virtual page.
    uintptr_t old_address = (uintptr_t)heap_test;
    kfree(heap_test);

    uint8_t *heap_test2 = (uint8_t *)kmalloc(64);

    if (heap_test2 == NULL) {
        serial_writestring("[HEAP] Allocation after free failed\n");
        hcf();
    }

    if ((uintptr_t)heap_test2 != old_address) {
        serial_writestring("[HEAP] Freed page was not reused\n");
        hcf();
    }

    kfree(heap_test2);

    serial_writestring("[HEAP] Free and page reuse passed\n");

    // Test 3: Multi-page allocation and read/write.
    uint8_t *large_test = (uint8_t *)kmalloc(5000);

    if (large_test == NULL) {
        serial_writestring("[HEAP] Multi-page allocation failed\n");
        hcf();
    }

    large_test[0] = 0x12;
    large_test[4095] = 0x34;
    large_test[4999] = 0x56;

    if (large_test[0] != 0x12 ||
        large_test[4095] != 0x34 ||
        large_test[4999] != 0x56) {
        serial_writestring("[HEAP] Multi-page read/write failed\n");
        hcf();
    }

    kfree(large_test);

    serial_writestring("[HEAP] Multi-page allocation passed\n");


    // Test 5: Verify multi-page memory recovery.

    serial_writestring("[HEAP] Starting multi-page free test\n");

    uint64_t multi_free_before = pmm_get_free_frames();

    void *multi_free_test = kmalloc(8192);

    if (multi_free_test == NULL) {
        serial_writestring("[HEAP] Multi-page allocation FAILED\n");
        hcf();
    }

    uint64_t multi_free_during = pmm_get_free_frames();

    if (multi_free_during >= multi_free_before) {
        serial_writestring("[HEAP] Frame allocation test FAILED\n");
        hcf();
    }

    kfree(multi_free_test);

    uint64_t multi_free_after = pmm_get_free_frames();

    if (multi_free_after != multi_free_before) {
        serial_writestring("[HEAP] Memory recovery test FAILED\n");
        hcf();
    }

    serial_writestring("[HEAP] Multi-page memory recovery passed\n");


    // Test 4: Invalid allocation sizes and invalid pointer.
    if (kmalloc(0) != NULL ||
        kmalloc((size_t)-1) != NULL) {
        serial_writestring("[HEAP] Invalid-size test failed\n");
        hcf();
    }

    // Unknown pointers must be ignored.
    kfree((void *)(uintptr_t)0x1234);

   
    serial_writestring("[HEAP] Invalid-input tests passed\n");
  
      // -----------------------------------------
    // HEAP ALLOCATION FAILURE TESTS
    // -----------------------------------------

    serial_writestring("[HEAP] Starting allocation failure tests\n");

    uint64_t free_before;
    uint64_t free_after;

    // Test 1: Fail the first physical allocation.
    free_before = pmm_get_free_frames();

    pmm_test_fail_allocation_after(0);

    void *failed_alloc = kmalloc(4096);

    free_after = pmm_get_free_frames();

    if (failed_alloc != NULL ||
        free_before != free_after) {
        serial_writestring("[HEAP] Immediate failure test FAILED\n");
        return;
    }

    serial_writestring("[HEAP] Immediate allocation failure passed\n");


    // Test 2: Allow one physical allocation,
    // then fail the next allocation.
    free_before = pmm_get_free_frames();

    pmm_test_fail_allocation_after(1);

    failed_alloc = kmalloc(4096);

    free_after = pmm_get_free_frames();

    if (failed_alloc != NULL ||
        free_before != free_after) {
        serial_writestring("[HEAP] Allocation rollback test FAILED\n");
        return;
    }

    serial_writestring("[HEAP] Allocation rollback test passed\n");


    // Test 3: Verify that the heap still works
    // after allocation failures.
    void *recovery_alloc = kmalloc(4096);

    if (recovery_alloc == NULL) {
        serial_writestring("[HEAP] Recovery allocation FAILED\n");
        return;
    }

    kfree(recovery_alloc);

    serial_writestring("[HEAP] Recovery allocation passed\n");

    serial_writestring("[HEAP] Allocation failure tests passed\n");
   
    // HEAP STRESS TEST: Multiple simultaneous allocations

    serial_writestring("[HEAP] Starting stress tests\n");

    uint64_t stress_free_before = pmm_get_free_frames();

    void *a = kmalloc(64);
    void *b = kmalloc(5000);
    void *c = kmalloc(8192);
    void *d = kmalloc(1024);

    if (a == NULL || b == NULL || c == NULL || d == NULL) {
        serial_writestring("[HEAP] Multiple allocation FAILED\n");
        hcf();
    }

    if (a == b || a == c || a == d ||
        b == c || b == d || c == d) {
        serial_writestring("[HEAP] Duplicate allocation FAILED\n");
        hcf();
    }

    serial_writestring("[HEAP] Multiple allocations passed\n");

    // Write test values.
    *(volatile uint8_t *)a = 0x11;
    *(volatile uint8_t *)b = 0x22;
    *(volatile uint8_t *)c = 0x33;
    *(volatile uint8_t *)d = 0x44;

    if (*(volatile uint8_t *)a != 0x11 ||
        *(volatile uint8_t *)b != 0x22 ||
        *(volatile uint8_t *)c != 0x33 ||
        *(volatile uint8_t *)d != 0x44) {
        serial_writestring("[HEAP] Data verification FAILED\n");
        hcf();
    }

    serial_writestring("[HEAP] Data verification passed\n");

    // Free in reverse allocation order.
    kfree(d);
    kfree(c);
    kfree(b);
    kfree(a);

    uint64_t stress_free_after = pmm_get_free_frames();

    if (stress_free_after != stress_free_before) {
        serial_writestring("[HEAP] Stress memory recovery FAILED\n");
        hcf();
    }

    serial_writestring("[HEAP] Stress memory recovery passed\n");
    serial_writestring("[HEAP] Stress tests passed\n");


    // HEAP CAPACITY TEST

    serial_writestring("[HEAP] Starting capacity test\n");

    uint64_t capacity_free_before = pmm_get_free_frames();

    void *capacity_test = kmalloc(256 * 4096);

    if (capacity_test == NULL) {
        serial_writestring("[HEAP] Maximum allocation FAILED\n");
        hcf();
    }

    serial_writestring("[HEAP] Maximum allocation passed\n");

    // Heap must reject another allocation when full.
    void *extra_test = kmalloc(4096);

    if (extra_test != NULL) {
        serial_writestring("[HEAP] Capacity limit FAILED\n");
        hcf();
    }

    serial_writestring("[HEAP] Capacity limit passed\n");

    kfree(capacity_test);

    if (pmm_get_free_frames() != capacity_free_before) {
        serial_writestring("[HEAP] Capacity recovery FAILED\n");
        hcf();
    }

    serial_writestring("[HEAP] Capacity recovery passed\n");
    serial_writestring("[HEAP] Capacity tests passed\n");

    
    // HEAP FRAGMENTATION TEST

    serial_writestring("[HEAP] Starting fragmentation test\n");

    void *fragment_blocks[128];

    uint64_t fragmentation_free_before =
        pmm_get_free_frames();

    // Allocate 128 blocks of 2 pages each.
    // Total: 256 pages (1 MiB).
    for (uint64_t i = 0; i < 128; i++) {
        fragment_blocks[i] = kmalloc(8192);

        if (fragment_blocks[i] == NULL) {
            serial_writestring("[HEAP] Fragment allocation FAILED\n");
            hcf();
        }
    }

    serial_writestring("[HEAP] Full heap allocation passed\n");

    // Free every alternate allocation.
    for (uint64_t i = 0; i < 128; i += 2) {
        kfree(fragment_blocks[i]);
    }

    serial_writestring("[HEAP] Fragment creation passed\n");

    // There are free pages, but no contiguous run of 3 pages.
    void *fragment_test = kmalloc(12288);

    if (fragment_test != NULL) {
        serial_writestring("[HEAP] Fragmentation test FAILED\n");
        hcf();
    }

    serial_writestring("[HEAP] Contiguous allocation rejection passed\n");

    // Free the remaining allocations.
    for (uint64_t i = 1; i < 128; i += 2) {
        kfree(fragment_blocks[i]);
    }

    if (pmm_get_free_frames() != fragmentation_free_before) {
        serial_writestring("[HEAP] Fragment recovery FAILED\n");
        hcf();
    }

    serial_writestring("[HEAP] Fragment recovery passed\n");
    serial_writestring("[HEAP] Fragmentation tests passed\n");

 serial_writestring("[HEAP] All tests passed\n");

    // ==========================================
    // END OF KERNEL HEAP TESTS
    // ==========================================


    // ==========================================
    // NX EXECUTION PROTECTION TEST
    // ==========================================

    if (nx_available) {

        serial_writestring(
            "[NX] Starting execution protection test\n"
        );

        const uint64_t nx_test_virtual =
            0x0000600000000000ULL;

        uint64_t nx_existing = 0;
        uint64_t nx_test_frame = pmm_alloc_frame();

        if (nx_test_frame == 0) {
            serial_writestring("[NX] Frame allocation failed\n");
            hcf();
        }

        if (vmm_translate(
                nx_test_virtual,
                hhdm->offset,
                &nx_existing)) {
            serial_writestring("[NX] Test address already mapped\n");
            pmm_free_frame(nx_test_frame);
            hcf();
        }

        // Put a RET instruction in the physical frame.
        volatile uint8_t *nx_bytes =
            (volatile uint8_t *)(uintptr_t)(
                hhdm->offset + nx_test_frame
            );

        nx_bytes[0] = 0xC3;

        // Map as readable but non-executable.
        if (!vmm_map_page_flags(
                nx_test_virtual,
                nx_test_frame,
                hhdm->offset,
                VMM_READ | VMM_NO_EXEC)) {

            serial_writestring("[NX] Mapping failed\n");
            pmm_free_frame(nx_test_frame);
            hcf();
        }

        serial_writestring(
            "[NX] Non-executable mapping successful\n"
        );

        // Page-fault bit 0: present/protection violation.
        // Page-fault bit 4: instruction fetch.
        if (!idt_expect_page_fault(
                nx_test_virtual,
                (1ULL << 0) | (1ULL << 4),
                (1ULL << 0) | (1ULL << 4),
                (uintptr_t)test_execute_nx_recovery)) {

            serial_writestring(
                "[NX] Expected fault registration failed\n"
            );
            hcf();
        }

        // Attempt to execute the non-executable page.
        int nx_result = test_execute_nx(
            (void *)(uintptr_t)nx_test_virtual
        );

        idt_cancel_expected_page_fault();

        if (nx_result != 1) {
            serial_writestring(
                "[NX] FAILED: execution was not blocked\n"
            );
            hcf();
        }

        serial_writestring(
            "[NX] Execution protection fault recovered\n"
        );

        // Unmap and release the physical frame.
        uint64_t nx_unmapped_frame = 0;

        if (!vmm_unmap_page(
                nx_test_virtual,
                hhdm->offset,
                &nx_unmapped_frame)) {

            serial_writestring("[NX] Unmapping failed\n");
            hcf();
        }

        if (nx_unmapped_frame != nx_test_frame ||
            !pmm_free_frame(nx_unmapped_frame)) {

            serial_writestring("[NX] Frame release failed\n");
            hcf();
        }

        serial_writestring(
            "[NX] Execution protection test passed\n"
        );

    } else {
        serial_writestring(
            "[NX] Test skipped: CPU NX unavailable\n"
        );
    }


    // Test NX (No-Execute) memory protection.
    // if (nx_available) {
    //     serial_writestring("[NX] Starting NX protection test\n");

    //     // Allocate a physical frame for the test.
    //     uint64_t nx_test_frame = pmm_alloc_frame();

    //     if (nx_test_frame == 0) {
    //         serial_writestring("[NX] Frame allocation failed\n");
    //         hcf();
    //     }

    //     // Use a canonical virtual address that was used by the
    //     // earlier mapping test and has since been unmapped.
    //     const uint64_t nx_test_virtual = 0x0000400000000000ULL;

    //     // Place a RET instruction (0xC3) in the physical frame
    //     // using the higher-half direct map.
    //     volatile uint8_t *nx_test_bytes =
    //         (volatile uint8_t *)(uintptr_t)(
    //             hhdm->offset + nx_test_frame
    //         );

    //     nx_test_bytes[0] = 0xC3;

    //     // Map the frame as readable but non-executable.
    //     if (!vmm_map_page_flags(
    //             nx_test_virtual,
    //             nx_test_frame,
    //             hhdm->offset,
    //             VMM_READ | VMM_NO_EXEC)) {

    //         serial_writestring("[NX] Non-executable mapping failed\n");
    //         pmm_free_frame(nx_test_frame);
    //         hcf();
    //     }

    //     serial_writestring(
    //         "[NX] Attempting to execute a non-executable page\n"
    //     );

    //     // Expected result: a page fault caused by instruction fetch.
    //     void (*nx_test_function)(void) =
    //         (void (*)(void))(uintptr_t)nx_test_virtual;

    //     nx_test_function();

    //     // Reaching this point means execution was NOT blocked.
    //     serial_writestring("[NX] FAILED: execution was allowed\n");

    //     // Clean up if the test unexpectedly returns.
    //     uint64_t unmapped_frame = 0;

    //     if (vmm_unmap_page(
    //             nx_test_virtual,
    //             hhdm->offset,
    //             &unmapped_frame)) {

    //         if (unmapped_frame == nx_test_frame) {
    //             pmm_free_frame(nx_test_frame);
    //         } else {
    //             serial_writestring("[NX] Unexpected unmapped frame\n");
    //         }
    //     } else {
    //         serial_writestring("[NX] Cleanup unmapping failed\n");
    //     }

    //     hcf();
    // } else {
    //     serial_writestring("[NX] Test skipped: NX unavailable\n");
    // }


    // ==========================================
    // FINAL MEMORY RECOVERY VERIFICATION
    // ==========================================

    serial_writestring(
        "[MEMORY] Starting final memory recovery check\n"
    );

    uint64_t pmm_free_final = pmm_get_free_frames();

    serial_writestring("[MEMORY] Initial free frames = ");
    serial_write_hex(pmm_free_baseline);
    serial_writestring("\n");

    serial_writestring("[MEMORY] Final free frames = ");
    serial_write_hex(pmm_free_final);
    serial_writestring("\n");

    if (pmm_free_final != pmm_free_baseline) {

        serial_writestring(
            "[MEMORY] FAILED: Free frame count mismatch\n"
        );

        hcf();
    }

    serial_writestring(
        "[MEMORY] Physical frame recovery passed\n"
    );

    serial_writestring(
        "[MEMORY] Final memory verification passed\n"
    );

    
    // Verify that a framebuffer is available.
    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1) {
        serial_writestring("[FB] No framebuffer available\n");
        hcf();
    }

    // Draw the gradient on each supported framebuffer.
    for (uint64_t i = 0;
         i < framebuffer_request.response->framebuffer_count;
         i++) {

        struct limine_framebuffer *framebuffer =
            framebuffer_request.response->framebuffers[i];

        if (framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB ||
            framebuffer->bpp != 32) {
            serial_writestring("[FB] Unsupported framebuffer format\n");
            hcf();
        }

        fb_pattern(framebuffer);
    }

    serial_writestring("[FB] Framebuffer pattern displayed\n");

    // Keep the kernel running.
    hcf();
}
