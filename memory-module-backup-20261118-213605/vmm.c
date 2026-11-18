
#include <stdint.h>
#include "vmm.h"
#include "pmm.h"

#define PAGE_SIZE 4096U
#define LARGE_PAGE_SIZE 0x400000U
#define PAGE_MASK 0xFFFFF000U
#define LARGE_PAGE_MASK 0xFFC00000U
#define PAGE_DIRECTORY_ENTRIES 1024
#define PAGE_TABLE_ENTRIES 1024
#define PAGE_TABLE_POOL_SIZE 128
#define IDENTITY_MAP_SIZE 0x04000000U

#define PAGE_FLAG_PRESENT 0x001U
#define PAGE_FLAG_WRITABLE 0x002U
#define PAGE_FLAG_USER 0x004U
#define PAGE_FLAG_LARGE 0x080U

static uint32_t page_directory[PAGE_DIRECTORY_ENTRIES]
    __attribute__((aligned(4096)));

static uint32_t page_tables[PAGE_TABLE_POOL_SIZE][PAGE_TABLE_ENTRIES]
    __attribute__((aligned(4096)));

static uint8_t page_table_used[PAGE_TABLE_POOL_SIZE];

static uint32_t used_page_tables = 0;

static void flush_tlb(void)
{
    uint32_t cr3;

    __asm__ volatile (
        "mov %%cr3, %0"
        : "=r"(cr3)
    );

    __asm__ volatile (
        "mov %0, %%cr3"
        :
        : "r"(cr3)
        : "memory"
    );
}

static void invalidate_page(uint32_t address)
{
    __asm__ volatile (
        "invlpg (%0)"
        :
        : "r"(address)
        : "memory"
    );
}

static uint32_t *allocate_page_table(void)
{
    for (uint32_t i = 0; i < PAGE_TABLE_POOL_SIZE; i++) {

        if (!page_table_used[i]) {

            page_table_used[i] = 1;
            used_page_tables++;

            for (uint32_t j = 0; j < PAGE_TABLE_ENTRIES; j++)
                page_tables[i][j] = 0;

            return page_tables[i];
        }
    }

    return 0;
}

static uint32_t *get_page_table(uint32_t virtual_address,
                                int create)
{
    uint32_t directory_index = virtual_address >> 22;

    uint32_t directory_entry =
        page_directory[directory_index];

    if (directory_entry & PAGE_FLAG_PRESENT) {

        /* Existing 4 KB page table. */
        if (!(directory_entry & PAGE_FLAG_LARGE)) {
            return (uint32_t *)
                (directory_entry & PAGE_MASK);
        }

        /* Convert an existing 4 MB identity mapping
           into a 4 KB page table. */

        if (!create)
            create = 1;

        uint32_t *table = allocate_page_table();

        if (!table)
            return 0;

        uint32_t base =
            directory_entry & LARGE_PAGE_MASK;

        for (uint32_t i = 0; i < PAGE_TABLE_ENTRIES; i++) {

            table[i] =
                (base + i * PAGE_SIZE) |
                PAGE_FLAG_PRESENT |
                PAGE_FLAG_WRITABLE;
        }

        page_directory[directory_index] =
            ((uint32_t)table & PAGE_MASK) |
            PAGE_FLAG_PRESENT |
            PAGE_FLAG_WRITABLE;

        flush_tlb();

        return table;
    }

    if (!create)
        return 0;

    uint32_t *table = allocate_page_table();

    if (!table)
        return 0;

    page_directory[directory_index] =
        ((uint32_t)table & PAGE_MASK) |
        PAGE_FLAG_PRESENT |
        PAGE_FLAG_WRITABLE;

    flush_tlb();

    return table;
}

void vmm_init(void)
{
    for (uint32_t i = 0; i < PAGE_DIRECTORY_ENTRIES; i++)
        page_directory[i] = 0;

    for (uint32_t i = 0; i < PAGE_TABLE_POOL_SIZE; i++)
        page_table_used[i] = 0;

    used_page_tables = 0;

    /* Identity-map first 64 MB using 4 MB pages. */
    for (uint32_t i = 0;
         i < IDENTITY_MAP_SIZE / LARGE_PAGE_SIZE;
         i++) {

        page_directory[i] =
            (i * LARGE_PAGE_SIZE) |
            PAGE_FLAG_PRESENT |
            PAGE_FLAG_WRITABLE |
            PAGE_FLAG_LARGE;
    }

    /* Enable 4 MB pages through CR4.PSE. */
    uint32_t cr4;

    __asm__ volatile (
        "mov %%cr4, %0"
        : "=r"(cr4)
    );

    cr4 |= 0x10U;

    __asm__ volatile (
        "mov %0, %%cr4"
        :
        : "r"(cr4)
        : "memory"
    );

    /* Load page directory. */
    uint32_t pd_address = (uint32_t)page_directory;

    __asm__ volatile (
        "mov %0, %%cr3"
        :
        : "r"(pd_address)
        : "memory"
    );

    /* Enable paging and supervisor write protection. */
    uint32_t cr0;

    __asm__ volatile (
        "mov %%cr0, %0"
        : "=r"(cr0)
    );

    cr0 |= 0x80010000U;

    __asm__ volatile (
        "mov %0, %%cr0"
        :
        : "r"(cr0)
        : "memory"
    );
}

int vmm_map_page(uint32_t virtual_address,
                 uint32_t physical_address,
                 uint32_t flags)
{
    if ((virtual_address & (PAGE_SIZE - 1)) ||
        (physical_address & (PAGE_SIZE - 1)))
        return 0;

    uint32_t *table =
        get_page_table(virtual_address, 1);

    if (!table)
        return 0;

    uint32_t table_index =
        (virtual_address >> 12) & 0x3FFU;

    if (table[table_index] & PAGE_FLAG_PRESENT)
        return 0;

    table[table_index] =
        (physical_address & PAGE_MASK) |
        (flags & 0xFFFU) |
        PAGE_FLAG_PRESENT;

    if (flags & VMM_PAGE_USER) {
        page_directory[virtual_address >> 22] |=
            PAGE_FLAG_USER;
    }

    invalidate_page(virtual_address);

    return 1;
}

uint32_t vmm_unmap_page(uint32_t virtual_address)
{
    if (virtual_address & (PAGE_SIZE - 1))
        return 0;

    uint32_t *table =
        get_page_table(virtual_address, 1);

    if (!table)
        return 0;

    uint32_t table_index =
        (virtual_address >> 12) & 0x3FFU;

    uint32_t entry = table[table_index];

    if (!(entry & PAGE_FLAG_PRESENT))
        return 0;

    uint32_t physical_address =
        entry & PAGE_MASK;

    table[table_index] = 0;

    invalidate_page(virtual_address);

    return physical_address;
}

uint32_t vmm_get_physical_address(uint32_t virtual_address)
{
    uint32_t directory_index =
        virtual_address >> 22;

    uint32_t directory_entry =
        page_directory[directory_index];

    if (!(directory_entry & PAGE_FLAG_PRESENT))
        return 0;

    /* Handle 4 MB pages. */
    if (directory_entry & PAGE_FLAG_LARGE) {

        return (directory_entry & LARGE_PAGE_MASK) |
               (virtual_address & 0x003FFFFFU);
    }

    uint32_t *table =
        (uint32_t *)(directory_entry & PAGE_MASK);

    uint32_t table_index =
        (virtual_address >> 12) & 0x3FFU;

    uint32_t entry = table[table_index];

    if (!(entry & PAGE_FLAG_PRESENT))
        return 0;

    return (entry & PAGE_MASK) |
           (virtual_address & 0xFFFU);
}

int vmm_alloc_page(uint32_t virtual_address, uint32_t flags)
{
    uint32_t physical_address = pmm_alloc_frame();

    if (!physical_address)
        return 0;

    if (!vmm_map_page(
            virtual_address,
            physical_address,
            flags | VMM_PAGE_WRITABLE)) {

        pmm_free_frame(physical_address);
        return 0;
    }

    return 1;
}

int vmm_free_page(uint32_t virtual_address)
{
    uint32_t physical_address =
        vmm_unmap_page(virtual_address);

    if (!physical_address)
        return 0;

    pmm_free_frame(physical_address);

    return 1;
}

uint32_t vmm_get_used_page_tables(void)
{
    return used_page_tables;
}
