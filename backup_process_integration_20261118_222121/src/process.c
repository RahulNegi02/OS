#include <stdint.h>

#include "process.h"
#include "heap.h"

static process_t *process_list = 0;
static process_t *current_process = 0;

static uint32_t next_pid = 1;
static uint32_t process_count = 0;

int process_init(void)
{
    process_list = 0;
    current_process = 0;

    next_pid = 1;
    process_count = 0;

    return 1;
}

process_t *process_create(void (*entry_point)(void))
{
    if (!entry_point)
        return 0;

    process_t *process =
        (process_t *)kmalloc(sizeof(process_t));

    if (!process)
        return 0;

    uint8_t *stack =
        (uint8_t *)kmalloc(PROCESS_STACK_SIZE);

    if (!stack) {
        kfree(process);
        return 0;
    }

    process->pid = next_pid++;

    if (next_pid == 0)
        next_pid = 1;

    process->state = PROCESS_NEW;

    process->entry_point = entry_point;

    process->kernel_stack = stack;

    process->kernel_stack_size =
        PROCESS_STACK_SIZE;

    process->kernel_stack_top =
        ((uint32_t)(stack + PROCESS_STACK_SIZE)) & ~0x0F;

    /*
     * Initial stack pointer.
     * Context-switching frame will be prepared
     * in the next process-management phase.
     */

    process->saved_esp =
        process->kernel_stack_top;

    process->next = 0;

    /* Add process to the linked list. */

    if (!process_list) {

        process_list = process;

    } else {

        process_t *current = process_list;

        while (current->next)
            current = current->next;

        current->next = process;
    }

    process->state = PROCESS_READY;

    process_count++;

    return process;
}

int process_destroy(uint32_t pid)
{
    process_t *current = process_list;
    process_t *previous = 0;

    while (current) {

        if (current->pid == pid) {

            if (current == current_process)
                return 0;

            if (previous)
                previous->next = current->next;
            else
                process_list = current->next;

            current->state = PROCESS_TERMINATED;

            if (current->kernel_stack)
                kfree(current->kernel_stack);

            kfree(current);

            if (process_count > 0)
                process_count--;

            return 1;
        }

        previous = current;
        current = current->next;
    }

    return 0;
}

process_t *process_get_first(void)
{
    return process_list;
}

process_t *process_get_current(void)
{
    return current_process;
}

uint32_t process_get_count(void)
{
    return process_count;
}
