#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define PROCESS_STACK_SIZE 4096

typedef enum {
    PROCESS_NEW = 0,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_TERMINATED
} process_state_t;

typedef struct process {
    uint32_t pid;

    process_state_t state;

    void (*entry_point)(void);

    uint8_t *kernel_stack;

    uint32_t kernel_stack_size;
    uint32_t kernel_stack_top;
    uint32_t saved_esp;

    struct process *next;

} process_t;

int process_init(void);

process_t *process_create(void (*entry_point)(void));

int process_destroy(uint32_t pid);

process_t *process_get_first(void);

process_t *process_get_current(void);

uint32_t process_get_count(void);

#endif
