#include <stdint.h>
#include "heap.h"
#include "vmm.h"

#define PAGE_SIZE 4096U

#define HEAP_START 0xD0000000U
#define HEAP_MAX_PAGES 4096U

#define HEAP_BITMAP_WORDS (HEAP_MAX_PAGES / 32U)

#define HEAP_MAGIC 0x48454150U
#define HEAP_HEADER_SIZE 16U

struct heap_header {
    uint32_t magic;
    uint32_t page_count;
    uint32_t first_page;
    uint32_t requested_size;
};

static uint32_t heap_bitmap[HEAP_BITMAP_WORDS];

static uint32_t heap_used_pages = 0;

static int page_is_used(uint32_t page)
{
    return (heap_bitmap[page / 32U] &
            (1U << (page % 32U))) != 0;
}

static void mark_page_used(uint32_t page)
{
    heap_bitmap[page / 32U] |=
        (1U << (page % 32U));
}

static void mark_page_free(uint32_t page)
{
    heap_bitmap[page / 32U] &=
        ~(1U << (page % 32U));
}

static int find_free_pages(uint32_t count)
{
    uint32_t consecutive = 0;
    uint32_t start = 0;

    for (uint32_t i = 0; i < HEAP_MAX_PAGES; i++) {

        if (!page_is_used(i)) {

            if (consecutive == 0)
                start = i;

            consecutive++;

            if (consecutive == count)
                return (int)start;
        }
        else {
            consecutive = 0;
        }
    }

    return -1;
}

void heap_init(void)
{
    for (uint32_t i = 0; i < HEAP_BITMAP_WORDS; i++)
        heap_bitmap[i] = 0;

    heap_used_pages = 0;
}

void *kmalloc(uint32_t size)
{
    if (size == 0)
        size = 1;

    if (size > HEAP_MAX_PAGES * PAGE_SIZE - HEAP_HEADER_SIZE)
        return 0;

    uint32_t required =
        (size + HEAP_HEADER_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;

    int start = find_free_pages(required);

    if (start < 0)
        return 0;

    uint32_t first_page = (uint32_t)start;

    uint32_t mapped = 0;

    for (; mapped < required; mapped++) {

        uint32_t address =
            HEAP_START + (first_page + mapped) * PAGE_SIZE;

        if (!vmm_alloc_page(address, VMM_PAGE_WRITABLE))
            break;
    }

    if (mapped != required) {

        for (uint32_t i = 0; i < mapped; i++) {

            uint32_t address =
                HEAP_START + (first_page + i) * PAGE_SIZE;

            vmm_free_page(address);
        }

        return 0;
    }

    for (uint32_t i = 0; i < required; i++)
        mark_page_used(first_page + i);

    heap_used_pages += required;

    uint8_t *base =
        (uint8_t *)(HEAP_START + first_page * PAGE_SIZE);

    for (uint32_t i = 0; i < required * PAGE_SIZE; i++)
        base[i] = 0;

    struct heap_header *header =
        (struct heap_header *)base;

    header->magic = HEAP_MAGIC;
    header->page_count = required;
    header->first_page = first_page;
    header->requested_size = size;

    return base + HEAP_HEADER_SIZE;
}

void kfree(void *pointer)
{
    if (!pointer)
        return;

    uint32_t address = (uint32_t)pointer;

    if (address < HEAP_START + HEAP_HEADER_SIZE ||
        address >= HEAP_START + HEAP_MAX_PAGES * PAGE_SIZE)
        return;

    uint32_t base_address = address - HEAP_HEADER_SIZE;

    if ((base_address - HEAP_START) % PAGE_SIZE != 0)
        return;

    struct heap_header *header =
        (struct heap_header *)base_address;

    if (header->magic != HEAP_MAGIC)
        return;

    uint32_t first = header->first_page;
    uint32_t count = header->page_count;

    if (count == 0 ||
        first >= HEAP_MAX_PAGES ||
        count > HEAP_MAX_PAGES - first)
        return;

    if (first * PAGE_SIZE + HEAP_START != base_address)
        return;

    for (uint32_t i = 0; i < count; i++) {

        if (!page_is_used(first + i))
            return;

        if (vmm_get_physical_address(
                HEAP_START + (first + i) * PAGE_SIZE) == 0)
            return;
    }

    header->magic = 0;

    for (uint32_t i = 0; i < count; i++) {

        uint32_t virtual_address =
            HEAP_START + (first + i) * PAGE_SIZE;

        vmm_free_page(virtual_address);

        mark_page_free(first + i);
    }

    heap_used_pages -= count;
}

uint32_t heap_get_used_pages(void)
{
    return heap_used_pages;
}

uint32_t heap_get_total_pages(void)
{
    return HEAP_MAX_PAGES;
}
