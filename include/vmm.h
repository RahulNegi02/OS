#ifndef VMM_H
#define VMM_H

#include <stdint.h>

#define VMM_PAGE_PRESENT  0x001U
#define VMM_PAGE_WRITABLE 0x002U
#define VMM_PAGE_USER     0x004U

void vmm_init(void);

int vmm_map_page(uint32_t virtual_address,
                 uint32_t physical_address,
                 uint32_t flags);

uint32_t vmm_unmap_page(uint32_t virtual_address);

uint32_t vmm_get_physical_address(uint32_t virtual_address);

int vmm_alloc_page(uint32_t virtual_address, uint32_t flags);

int vmm_free_page(uint32_t virtual_address);
int vmm_protect_page(uint32_t virtual_address, int writable);

uint32_t vmm_get_used_page_tables(void);

#endif
