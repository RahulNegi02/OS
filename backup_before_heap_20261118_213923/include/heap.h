#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>

void heap_init(void);

void *kmalloc(uint32_t size);
void kfree(void *pointer);

uint32_t heap_get_used_pages(void);
uint32_t heap_get_total_pages(void);

#endif
