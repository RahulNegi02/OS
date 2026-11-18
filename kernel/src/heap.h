
#ifndef HEAP_H
#define HEAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Initialize the kernel heap using the HHDM offset. */
bool heap_init(uint64_t hhdm_offset);

/* Allocate memory from the kernel heap. */
void *kmalloc(size_t size);

/* Free memory previously allocated by kmalloc. */
void kfree(void *ptr);

#endif
