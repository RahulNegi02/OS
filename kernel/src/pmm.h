
#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stdbool.h>

#define PMM_PAGE_SIZE 4096ULL

// Initialize the physical memory manager using Limine.
bool pmm_init(void);

// Allocate one 4 KiB physical frame.
// Returns its physical address, or 0 on failure.
uint64_t pmm_alloc_frame(void);

// Release a frame previously allocated by the PMM.
bool pmm_free_frame(uint64_t physical_address);

// Return the number of free frames.
uint64_t pmm_get_free_frames(void);

void pmm_test_fail_allocation_after(uint64_t successful_allocations);
#endif
