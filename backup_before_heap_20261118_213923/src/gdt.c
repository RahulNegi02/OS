
#include <stdint.h>
#include "gdt.h"

struct __attribute__((packed)) gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
};

struct __attribute__((packed)) gdt_pointer {
    uint16_t limit;
    uint32_t base;
};

/* 32-bit Task State Segment */
struct __attribute__((packed)) tss_entry {
    uint32_t prev_tss;

    uint32_t esp0;
    uint32_t ss0;

    uint32_t esp1;
    uint32_t ss1;

    uint32_t esp2;
    uint32_t ss2;

    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;

    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;

    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;

    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;

    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
};

static struct gdt_entry gdt[5];
static struct gdt_pointer gdtr;

static struct tss_entry kernel_tss;
static struct tss_entry double_fault_tss;

static uint8_t double_fault_stack[8192]
    __attribute__((aligned(16)));

extern void gdt_flush(uint32_t address);
extern void double_fault_entry(void);

static void gdt_set_gate(
    int index,
    uint32_t base,
    uint32_t limit,
    uint8_t access,
    uint8_t granularity
) {
    gdt[index].base_low = base & 0xFFFF;
    gdt[index].base_middle = (base >> 16) & 0xFF;
    gdt[index].base_high = (base >> 24) & 0xFF;

    gdt[index].limit_low = limit & 0xFFFF;

    gdt[index].granularity =
        ((limit >> 16) & 0x0F) | (granularity & 0xF0);

    gdt[index].access = access;
}

static void tss_set_gate(int index, struct tss_entry *tss)
{
    uint32_t base = (uint32_t)tss;
    uint32_t limit = sizeof(struct tss_entry) - 1;

    gdt_set_gate(index, base, limit, 0x89, 0x00);
}

void gdt_init(void)
{
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uint32_t)&gdt;

    /* Null descriptor */
    gdt_set_gate(0, 0, 0, 0, 0);

    /* Kernel code segment */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    /* Kernel data segment */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    /* Initialize kernel TSS */
    kernel_tss.ss0 = 0x10;
    kernel_tss.iomap_base = sizeof(struct tss_entry);

    /* Initialize Double Fault TSS descriptor */
    double_fault_tss.iomap_base = sizeof(struct tss_entry);

    /* Add both TSS descriptors */
    tss_set_gate(3, &kernel_tss);
    tss_set_gate(4, &double_fault_tss);

    /* Load GDT */
    gdt_flush((uint32_t)&gdtr);

    /* Load current kernel TSS into task register */
    uint16_t selector = 0x18;

    __asm__ volatile (
        "ltr %0"
        :
        : "r"(selector)
        : "memory"
    );
}

/* Initialize the dedicated Double Fault task */
void double_fault_tss_init(void)
{
    uint32_t cr3;
    uint32_t stack_top =
        (uint32_t)(double_fault_stack + sizeof(double_fault_stack));

    __asm__ volatile (
        "mov %%cr3, %0"
        : "=r"(cr3)
    );

    double_fault_tss.cr3 = cr3;
    double_fault_tss.eip = (uint32_t)double_fault_entry;
    double_fault_tss.eflags = 0x02;

    double_fault_tss.esp = stack_top;
    double_fault_tss.ebp = stack_top;

    double_fault_tss.cs = 0x08;
    double_fault_tss.ss = 0x10;
    double_fault_tss.ds = 0x10;
    double_fault_tss.es = 0x10;
    double_fault_tss.fs = 0x10;
    double_fault_tss.gs = 0x10;

    double_fault_tss.ss0 = 0x10;
    double_fault_tss.esp0 = stack_top;

    double_fault_tss.iomap_base = sizeof(struct tss_entry);
}
