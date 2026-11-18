
#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stdbool.h>

// VMM permission flags.
#define VMM_READ      (1ULL << 0)
#define VMM_WRITE     (1ULL << 1)
#define VMM_USER      (1ULL << 2)
#define VMM_NO_EXEC   (1ULL << 3)

// Translate a virtual address to a physical address.
bool vmm_translate(
    uint64_t virtual_address,
    uint64_t hhdm_offset,
    uint64_t *physical_address
);

// Map a 4 KiB page with default permissions.
bool vmm_map_page(
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t hhdm_offset
);

// Map a 4 KiB page with specified permissions.
bool vmm_map_page_flags(
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t hhdm_offset,
    uint64_t flags
);

// Unmap a 4 KiB page.
bool vmm_unmap_page(
    uint64_t virtual_address,
    uint64_t hhdm_offset,
    uint64_t *physical_address
);

bool vmm_enable_nx(void);
#endif
