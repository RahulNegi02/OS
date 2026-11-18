
NASM = nasm
CC = gcc
LD = ld

CFLAGS = -m32 -ffreestanding -fno-pie -fno-pic \
         -fno-stack-protector -fno-asynchronous-unwind-tables \
         -fno-builtin -O2 -Wall -Wextra -Iinclude

LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

.PHONY: all clean run

all: CampusOS.iso

# Bootloader
boot.o: src/boot.asm
	$(NASM) -f elf32 $< -o $@

# GDT
gdt_flush.o: src/gdt_flush.asm
	$(NASM) -f elf32 $< -o $@

gdt.o: src/gdt.c include/gdt.h
	$(CC) $(CFLAGS) -c $< -o $@

# Paging
paging.o: src/paging.c include/paging.h
	$(CC) $(CFLAGS) -c $< -o $@

# CPU exceptions and IDT
interrupts.o: src/interrupts.asm
	$(NASM) -f elf32 $< -o $@

idt.o: src/idt.c include/interrupts.h
	$(CC) $(CFLAGS) -c $< -o $@

exceptions.o: src/exceptions.c include/interrupts.h
	$(CC) $(CFLAGS) -c $< -o $@

# Hardware IRQ and timer
irq.o: src/irq.asm
	$(NASM) -f elf32 $< -o $@

irq_c.o: src/irq.c include/interrupts.h
	$(CC) $(CFLAGS) -c $< -o $@

# Keyboard driver
keyboard.o: drivers/keyboard/keyboard.c include/keyboard.h
	$(CC) $(CFLAGS) -c $< -o $@

# Main kernel
kernel.o: src/kernel.c include/gdt.h include/interrupts.h include/paging.h include/keyboard.h
	$(CC) $(CFLAGS) -c $< -o $@

# Link all kernel modules
kernel.elf: boot.o gdt_flush.o gdt.o paging.o interrupts.o idt.o exceptions.o irq.o irq_c.o keyboard.o kernel.o linker.ld
	$(LD) $(LDFLAGS) -o $@ boot.o gdt_flush.o gdt.o paging.o interrupts.o idt.o exceptions.o irq.o irq_c.o keyboard.o kernel.o

# Create bootable ISO
CampusOS.iso: kernel.elf iso/boot/grub/grub.cfg
	grub-file --is-x86-multiboot kernel.elf
	mkdir -p iso/boot
	cp kernel.elf iso/boot/kernel.elf
	grub-mkrescue -o $@ iso

# Run CampusOS in QEMU
run: CampusOS.iso
	qemu-system-i386 -cdrom CampusOS.iso

# Clean generated files
clean:
	rm -f boot.o gdt_flush.o gdt.o paging.o interrupts.o idt.o exceptions.o irq.o irq_c.o keyboard.o kernel.o kernel.elf CampusOS.iso iso/boot/kernel.elf
