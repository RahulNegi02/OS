#include <stdint.h>
#include "heap.h"
#include "vmm.h"

#define HEAP_START       0xD0000000u
#define HEAP_SIZE        (16u * 1024u * 1024u)
#define HEAP_PAGE_SIZE   4096u
#define HEAP_PAGE_COUNT  (HEAP_SIZE / HEAP_PAGE_SIZE)
#define HEAP_BITMAP_SIZE (HEAP_PAGE_COUNT / 8u)

#define HEAP_MAGIC       0x48454150u

#define HEAP_FLAGS (VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE)

typedef struct {
    uint32_t magic;
    uint32_t page_count;
    uint32_t requested_size;
    uint32_t reserved;
} heap_header_t;

static uint8_t heap_bitmap[HEAP_BITMAP_SIZE];

static uint32_t used_pages = 0;
static uint32_t allocation_count = 0;
static uint8_t initialized = 0;

static void bitmap_set(uint32_t index)
{
    heap_bitmap[index / 8u] |=
        (uint8_t)(1u << (index % 8u));
}

static void bitmap_clear(uint32_t index)
{
    heap_bitmap[index / 8u] &=
        (uint8_t)~(1u << (index % 8u));
}

static int bitmap_test(uint32_t index)
{
    return (heap_bitmap[index / 8u] &
        (uint8_t)(1u << (index % 8u))) != 0;
}

static void zero_memory(void *memory, uint32_t size)
{
    uint8_t *ptr = (uint8_t *)memory;

    for (uint32_t i = 0; i < size; i++) {
        ptr[i] = 0;
    }
}

static int find_free_pages(uint32_t required)
{
    if (required == 0 || required > HEAP_PAGE_COUNT) {
        return -1;
    }

    uint32_t consecutive = 0;
    uint32_t start = 0;

    for (uint32_t i = 0; i < HEAP_PAGE_COUNT; i++) {

        if (!bitmap_test(i)) {

            if (consecutive == 0) {
                start = i;
            }

            consecutive++;

            if (consecutive == required) {
                return (int)start;
            }

        } else {
            consecutive = 0;
        }
    }

    return -1;
}

void heap_init(void)
{
    if (initialized) {
        return;
    }

    zero_memory(heap_bitmap, HEAP_BITMAP_SIZE);

    used_pages = 0;
    allocation_count = 0;

    initialized = 1;
}

void *kmalloc(uint32_t size)
{
    if (!initialized || size == 0) {
        return 0;
    }

    if (size > HEAP_SIZE - sizeof(heap_header_t) -
        (HEAP_PAGE_SIZE - 1u)) {
        return 0;
    }

    uint32_t total_size = size + sizeof(heap_header_t);

    uint32_t required_pages =
        (total_size + HEAP_PAGE_SIZE - 1u) /
        HEAP_PAGE_SIZE;

    int start_page = find_free_pages(required_pages);

    if (start_page < 0) {
        return 0;
    }

    uint32_t start = (uint32_t)start_page;

    /*
     * Reserve virtual pages in the heap bitmap.
     */
    for (uint32_t i = 0; i < required_pages; i++) {
        bitmap_set(start + i);
    }

    /*
     * Map physical frames using the existing VMM.
     */
    uint32_t mapped = 0;

    for (uint32_t i = 0; i < required_pages; i++) {

        uint32_t virtual_address =
            HEAP_START + ((start + i) * HEAP_PAGE_SIZE);

        if (vmm_alloc_page(virtual_address, HEAP_FLAGS) != 0) {
            break;
        }

        mapped++;
    }

    /*
     * Roll back if any page mapping fails.
     */
    if (mapped != required_pages) {

        for (uint32_t i = 0; i < mapped; i++) {

            uint32_t virtual_address =
                HEAP_START + ((start + i) * HEAP_PAGE_SIZE);

            vmm_free_page(virtual_address);
        }

        for (uint32_t i = 0; i < required_pages; i++) {
            bitmap_clear(start + i);
        }

        return 0;
    }

    uint32_t allocation_address =
        HEAP_START + (start * HEAP_PAGE_SIZE);

    heap_header_t *header =
        (heap_header_t *)allocation_address;

    header->magic = HEAP_MAGIC;
    header->page_count = required_pages;
    header->requested_size = size;
    header->reserved = 0;

    void *result = (void *)(allocation_address +
        sizeof(heap_header_t));

    zero_memory(result, size);

    used_pages += required_pages;
    allocation_count++;

    return result;
}

void kfree(void *ptr)
{
    if (!initialized || ptr == 0) {
        return;
    }

    uint32_t address = (uint32_t)ptr;

    if (address < HEAP_START + sizeof(heap_header_t) ||
        address >= HEAP_START + HEAP_SIZE) {
        return;
    }

    uint32_t header_address =
        address - sizeof(heap_header_t);

    if (header_address < HEAP_START) {
        return;
    }

    if ((header_address - HEAP_START) %
        HEAP_PAGE_SIZE != 0) {
        return;
    }

    uint32_t start_page =
        (header_address - HEAP_START) / HEAP_PAGE_SIZE;

    if (start_page >= HEAP_PAGE_COUNT ||
        !bitmap_test(start_page)) {
        return;
    }

    heap_header_t *header =
        (heap_header_t *)header_address;

    if (header->magic != HEAP_MAGIC) {
        return;
    }

    uint32_t pages = header->page_count;

    if (pages == 0 ||
        pages > HEAP_PAGE_COUNT - start_page) {
        return;
    }

    /*
     * Validate that all pages belong to this allocation.
     */
    for (uint32_t i = 0; i < pages; i++) {
        if (!bitmap_test(start_page + i)) {
            return;
        }
    }

    /*
     * Invalidate the header before releasing the pages.
     */
    header->magic = 0;

    for (uint32_t i = 0; i < pages; i++) {

        uint32_t virtual_address =
            HEAP_START +
            ((start_page + i) * HEAP_PAGE_SIZE);

        vmm_free_page(virtual_address);
        bitmap_clear(start_page + i);
    }

    if (used_pages >= pages) {
        used_pages -= pages;
    } else {
        used_pages = 0;
    }

    if (allocation_count > 0) {
        allocation_count--;
    }
}

uint32_t heap_get_used_pages(void)
{
    return used_pages;
}

uint32_t heap_get_allocation_count(void)
{
    return allocation_count;
}

uint32_t heap_get_free_pages(void)
{
    return HEAP_PAGE_COUNT - used_pages;
}
