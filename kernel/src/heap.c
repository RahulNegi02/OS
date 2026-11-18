
#include "heap.h"
#include "pmm.h"
#include "vmm.h"

#define HEAP_START       0xFFFF900000000000ULL
#define HEAP_PAGE_SIZE   4096ULL
#define HEAP_MAX_PAGES   256ULL
#define HEAP_SIZE        (HEAP_MAX_PAGES * HEAP_PAGE_SIZE)
#define HEAP_MAX_ALLOCS  128ULL

#define HEAP_FLAGS (VMM_READ | VMM_WRITE | VMM_NO_EXEC)

typedef struct {
    void *address;
    uint64_t first_page;
    uint64_t page_count;
    bool used;
} heap_allocation_t;

static uint64_t heap_hhdm_offset;
static bool heap_initialized;

static bool page_used[HEAP_MAX_PAGES];
static heap_allocation_t allocations[HEAP_MAX_ALLOCS];

bool heap_init(uint64_t hhdm_offset)
{
    if (heap_initialized) {
        return false;
    }

    /* Verify that the complete heap range is unmapped. */
    for (uint64_t i = 0; i < HEAP_MAX_PAGES; i++) {
        uint64_t physical = 0;
        uint64_t address =
            HEAP_START + i * HEAP_PAGE_SIZE;

        if (vmm_translate(address, hhdm_offset, &physical)) {
            return false;
        }
    }

    heap_hhdm_offset = hhdm_offset;

    for (uint64_t i = 0; i < HEAP_MAX_PAGES; i++) {
        page_used[i] = false;
    }

    for (uint64_t i = 0; i < HEAP_MAX_ALLOCS; i++) {
        allocations[i].address = NULL;
        allocations[i].first_page = 0;
        allocations[i].page_count = 0;
        allocations[i].used = false;
    }

    heap_initialized = true;
    return true;
}

void *kmalloc(size_t size)
{
    if (!heap_initialized || size == 0 ||
        (uint64_t)size > HEAP_SIZE) {
        return NULL;
    }

    uint64_t pages =
        ((uint64_t)size + HEAP_PAGE_SIZE - 1) /
        HEAP_PAGE_SIZE;

    /* Find an available allocation record. */
    uint64_t slot = HEAP_MAX_ALLOCS;

    for (uint64_t i = 0; i < HEAP_MAX_ALLOCS; i++) {
        if (!allocations[i].used) {
            slot = i;
            break;
        }
    }

    if (slot == HEAP_MAX_ALLOCS) {
        return NULL;
    }

    /* Find contiguous free virtual pages. */
    uint64_t first_page = HEAP_MAX_PAGES;
    uint64_t run = 0;

    for (uint64_t i = 0; i < HEAP_MAX_PAGES; i++) {
        if (!page_used[i]) {
            if (run == 0) {
                first_page = i;
            }

            run++;

            if (run == pages) {
                break;
            }
        } else {
            run = 0;
            first_page = HEAP_MAX_PAGES;
        }
    }

    if (run < pages || first_page >= HEAP_MAX_PAGES) {
        return NULL;
    }

    /* Allocate physical frames and map them. */
    uint64_t mapped = 0;

    for (; mapped < pages; mapped++) {
        uint64_t physical = pmm_alloc_frame();

        if (physical == 0) {
            break;
        }

        uint64_t address =
            HEAP_START +
            (first_page + mapped) * HEAP_PAGE_SIZE;

        if (!vmm_map_page_flags(
                address,
                physical,
                heap_hhdm_offset,
                HEAP_FLAGS)) {

            pmm_free_frame(physical);
            break;
        }

        /* Initialize the new page to zero. */
        volatile uint8_t *memory =
            (volatile uint8_t *)(uintptr_t)address;

        for (uint64_t j = 0; j < HEAP_PAGE_SIZE; j++) {
            memory[j] = 0;
        }
    }

    /* Roll back if the complete allocation failed. */
    if (mapped != pages) {
        while (mapped > 0) {
            mapped--;

            uint64_t address =
                HEAP_START +
                (first_page + mapped) * HEAP_PAGE_SIZE;

            uint64_t physical = 0;

            if (vmm_unmap_page(
                    address,
                    heap_hhdm_offset,
                    &physical)) {
                pmm_free_frame(physical);
            }
        }

        return NULL;
    }

    /* Update metadata only after successful mapping. */
    for (uint64_t i = 0; i < pages; i++) {
        page_used[first_page + i] = true;
    }

    void *result = (void *)(uintptr_t)(
        HEAP_START + first_page * HEAP_PAGE_SIZE);

    allocations[slot].address = result;
    allocations[slot].first_page = first_page;
    allocations[slot].page_count = pages;
    allocations[slot].used = true;

    return result;
}

void kfree(void *ptr)
{
    if (!heap_initialized || ptr == NULL) {
        return;
    }

    /* Find the exact allocation. */
    uint64_t slot = HEAP_MAX_ALLOCS;

    for (uint64_t i = 0; i < HEAP_MAX_ALLOCS; i++) {
        if (allocations[i].used &&
            allocations[i].address == ptr) {
            slot = i;
            break;
        }
    }

    /* Ignore unknown and interior pointers. */
    if (slot == HEAP_MAX_ALLOCS) {
        return;
    }

    uint64_t first = allocations[slot].first_page;
    uint64_t count = allocations[slot].page_count;

    /* Validate allocation metadata. */
    if (count == 0 ||
        first >= HEAP_MAX_PAGES ||
        count > HEAP_MAX_PAGES - first) {
        return;
    }

    uint64_t physical_frames[HEAP_MAX_PAGES];

    /*
     * Phase 1:
     * Validate every page before changing any mapping.
     */
    for (uint64_t i = 0; i < count; i++) {
        if (!page_used[first + i]) {
            return;
        }

        uint64_t address =
            HEAP_START + (first + i) * HEAP_PAGE_SIZE;

        uint64_t physical = 0;

        if (!vmm_translate(
                address,
                heap_hhdm_offset,
                &physical)) {
            return;
        }

        if (physical == 0 ||
            physical % HEAP_PAGE_SIZE != 0) {
            return;
        }

        physical_frames[i] = physical;
    }

    /*
     * Phase 2:
     * Unmap every page.
     * If an unmap fails, restore removed mappings.
     */
    uint64_t unmapped = 0;

    while (unmapped < count) {
        uint64_t address =
            HEAP_START +
            (first + unmapped) * HEAP_PAGE_SIZE;

        uint64_t physical = 0;

        if (!vmm_unmap_page(
                address,
                heap_hhdm_offset,
                &physical)) {
            break;
        }

        /*
         * Count the page immediately after successful unmapping.
         */
        unmapped++;

        if (physical != physical_frames[unmapped - 1]) {
            break;
        }
    }

    if (unmapped != count) {
        /* Restore every page that was successfully unmapped. */
        while (unmapped > 0) {
            unmapped--;

            uint64_t address =
                HEAP_START +
                (first + unmapped) * HEAP_PAGE_SIZE;

            vmm_map_page_flags(
                address,
                physical_frames[unmapped],
                heap_hhdm_offset,
                HEAP_FLAGS);
        }

        return;
    }

    /*
     * Phase 3:
     * Return physical frames to the PMM.
     */
    for (uint64_t i = 0; i < count; i++) {
        if (!pmm_free_frame(physical_frames[i])) {
            /*
             * Indicates an allocator consistency error.
             * Do not reuse allocation metadata.
             */
            return;
        }
    }

    /* Phase 4: Release heap metadata. */
    for (uint64_t i = 0; i < count; i++) {
        page_used[first + i] = false;
    }

    allocations[slot].address = NULL;
    allocations[slot].first_page = 0;
    allocations[slot].page_count = 0;
    allocations[slot].used = false;
}
