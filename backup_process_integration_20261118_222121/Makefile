NASM = nasm
CC = gcc
LD = ld

CFLAGS = -m32 -ffreestanding -fno-pie -fno-pic \
         -fno-stack-protector -fno-asynchronous-unwind-tables \
         -fno-builtin -O2 -Wall -Wextra -Iinclude

LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

.PHONY: all clean run

all: CampusOS.iso

boot.o: src/boot.asm
	$(NASM) -f elf32 $< -o $@

gdt_flush.o: src/gdt_flush.asm
	$(NASM) -f elf32 $< -o $@

gdt.o: src/gdt.c include/gdt.h
	$(CC) $(CFLAGS) -c $< -o $@

vmm.o: src/vmm.c include/vmm.h include/pmm.h
	$(CC) $(CFLAGS) -c $< -o $@

pmm.o: src/pmm.c include/pmm.h include/multiboot.h
	$(CC) $(CFLAGS) -c $< -o $@

heap.o: src/heap.c include/heap.h include/vmm.h
	$(CC) $(CFLAGS) -c $< -o $@

interrupts.o: src/interrupts.asm
	$(NASM) -f elf32 $< -o $@

idt.o: src/idt.c include/interrupts.h
	$(CC) $(CFLAGS) -c $< -o $@

exceptions.o: src/exceptions.c include/interrupts.h
	$(CC) $(CFLAGS) -c $< -o $@

irq.o: src/irq.asm
	$(NASM) -f elf32 $< -o $@

irq_c.o: src/irq.c include/interrupts.h
	$(CC) $(CFLAGS) -c $< -o $@

keyboard.o: drivers/keyboard/keyboard.c include/keyboard.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel.o: src/kernel.c include/gdt.h include/interrupts.h include/vmm.h include/keyboard.h include/pmm.h include/multiboot.h include/heap.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel.elf: boot.o gdt_flush.o gdt.o vmm.o pmm.o heap.o interrupts.o idt.o exceptions.o irq.o irq_c.o keyboard.o kernel.o linker.ld
	$(LD) $(LDFLAGS) -o $@ boot.o gdt_flush.o gdt.o vmm.o pmm.o heap.o interrupts.o idt.o exceptions.o irq.o irq_c.o keyboard.o kernel.o

CampusOS.iso: kernel.elf iso/boot/grub/grub.cfg
	grub-file --is-x86-multiboot kernel.elf
	mkdir -p iso/boot
	cp kernel.elf iso/boot/kernel.elf
	grub-mkrescue -o $@ iso

run: CampusOS.iso
	qemu-system-i386 -cdrom CampusOS.iso

clean:
	rm -f boot.o gdt_flush.o gdt.o vmm.o pmm.o heap.o interrupts.o idt.o exceptions.o irq.o irq_c.o keyboard.o kernel.o kernel.elf CampusOS.iso iso/boot/kernel.elf
