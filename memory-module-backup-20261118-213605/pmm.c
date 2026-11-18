#include <stdint.h>
#include "pmm.h"
#include "multiboot.h"

#define PAGE_SIZE 4096U
#define MAX_FRAMES 1048576U
#define BITMAP_WORDS (MAX_FRAMES / 32U)
#define FOUR_GB 0x100000000ULL

static uint32_t frame_bitmap[BITMAP_WORDS];

static uint32_t total_frames = 0;
static uint32_t free_frames = 0;

extern uint8_t __kernel_start;
extern uint8_t __kernel_end;

static int frame_is_used(uint32_t frame)
{
    return (frame_bitmap[frame / 32U] &
            (1U << (frame % 32U))) != 0;
}

static void mark_frame_free(uint32_t frame)
{
    if (frame >= MAX_FRAMES)
        return;

    uint32_t mask = 1U << (frame % 32U);

    if (frame_bitmap[frame / 32U] & mask) {
        frame_bitmap[frame / 32U] &= ~mask;
        free_frames++;
    }
}

static void reserve_frame(uint32_t frame)
{
    if (frame >= total_frames)
        return;

    uint32_t mask = 1U << (frame % 32U);

    if (!(frame_bitmap[frame / 32U] & mask)) {
        frame_bitmap[frame / 32U] |= mask;
        free_frames--;
    }
}

static void reserve_region(uint64_t start, uint64_t end)
{
    if (end <= start)
        return;

    uint64_t first = start / PAGE_SIZE;
    uint64_t last = (end + PAGE_SIZE - 1) / PAGE_SIZE;

    if (last > total_frames)
        last = total_frames;

    for (uint64_t i = first; i < last; i++)
        reserve_frame((uint32_t)i);
}

void pmm_init(uint32_t multiboot_info_addr)
{
    for (uint32_t i = 0; i < BITMAP_WORDS; i++)
        frame_bitmap[i] = 0xFFFFFFFFU;

    total_frames = 0;
    free_frames = 0;

    struct multiboot_info *info =
        (struct multiboot_info *)multiboot_info_addr;

    /* Read usable memory from the Multiboot memory map. */
    if (info->flags & MULTIBOOT_INFO_MEMORY_MAP) {

        uint32_t offset = 0;

        while (offset + sizeof(uint32_t) <= info->mmap_length) {

            struct multiboot_mmap_entry *entry =
                (struct multiboot_mmap_entry *)
                (info->mmap_addr + offset);

            uint32_t entry_bytes = entry->size + sizeof(uint32_t);

            if (entry->size < 20 ||
                entry_bytes > info->mmap_length - offset)
                break;

            if (entry->type == 1 && entry->len != 0) {

                uint64_t start = entry->addr;
                uint64_t end = start + entry->len;

                if (end < start)
                    end = FOUR_GB;

                if (start < FOUR_GB) {

                    if (end > FOUR_GB)
                        end = FOUR_GB;

                    uint64_t first =
                        (start + PAGE_SIZE - 1) / PAGE_SIZE;

                    uint64_t last = end / PAGE_SIZE;

                    if (last > MAX_FRAMES)
                        last = MAX_FRAMES;

                    if (last > total_frames)
                        total_frames = (uint32_t)last;

                    for (uint64_t i = first; i < last; i++)
                        mark_frame_free((uint32_t)i);
                }
            }

            offset += entry_bytes;
        }
    }
    else if (info->flags & MULTIBOOT_INFO_MEMORY) {

        /* Fallback when the memory map is unavailable. */
        uint64_t end =
            0x100000ULL + ((uint64_t)info->mem_upper * 1024ULL);

        if (end > FOUR_GB)
            end = FOUR_GB;

        total_frames = (uint32_t)(end / PAGE_SIZE);

        for (uint32_t i = 256; i < total_frames; i++)
            mark_frame_free(i);
    }

    /* Reserve the first 1 MB of physical memory. */
    reserve_region(0, 0x100000ULL);

    /* Reserve the complete kernel, including PMM bitmap. */
    reserve_region(
        (uint32_t)&__kernel_start,
        (uint32_t)&__kernel_end
    );

    /* Reserve Multiboot information and memory-map storage. */
    reserve_region(
        multiboot_info_addr,
        (uint64_t)multiboot_info_addr + sizeof(struct multiboot_info)
    );

    if (info->flags & MULTIBOOT_INFO_MEMORY_MAP) {
        reserve_region(
            info->mmap_addr,
            (uint64_t)info->mmap_addr + info->mmap_length
        );
    }
}

uint32_t pmm_alloc_frame(void)
{
    for (uint32_t i = 0; i < total_frames; i++) {

        if (!frame_is_used(i)) {

            frame_bitmap[i / 32U] |=
                (1U << (i % 32U));

            free_frames--;

            return i * PAGE_SIZE;
        }
    }

    return 0;
}

void pmm_free_frame(uint32_t address)
{
    if (address == 0 || (address & (PAGE_SIZE - 1)))
        return;

    uint32_t frame = address / PAGE_SIZE;

    if (frame >= total_frames)
        return;

    mark_frame_free(frame);
}

uint32_t pmm_get_free_frames(void)
{
    return free_frames;
}

uint32_t pmm_get_total_frames(void)
{
    return total_frames;
}
