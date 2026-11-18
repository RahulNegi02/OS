
<<<<<<< HEAD
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

# Paging and memory management
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

# Main kernel
kernel.o: src/kernel.c include/gdt.h include/interrupts.h
	$(CC) $(CFLAGS) -c $< -o $@

# Link all kernel modules
kernel.elf: boot.o gdt_flush.o gdt.o paging.o interrupts.o idt.o exceptions.o irq.o irq_c.o kernel.o linker.ld
	$(LD) $(LDFLAGS) -o $@ boot.o gdt_flush.o gdt.o paging.o interrupts.o idt.o exceptions.o irq.o irq_c.o kernel.o

# Create bootable ISO
CampusOS.iso: kernel.elf iso/boot/grub/grub.cfg
	grub-file --is-x86-multiboot kernel.elf
	mkdir -p iso/boot
	cp kernel.elf iso/boot/kernel.elf
	grub-mkrescue -o $@ iso

# Run CampusOS in QEMU
run: CampusOS.iso
	qemu-system-i386 -cdrom CampusOS.iso

# Remove generated build files
clean:
	rm -f boot.o gdt_flush.o gdt.o paging.o interrupts.o idt.o exceptions.o irq.o irq_c.o kernel.o kernel.elf CampusOS.iso iso/boot/kernel.elf
=======
CC = gcc
AS = as
LD = ld

CFLAGS = -m32 -ffreestanding -fno-pie -fno-stack-protector -Wall -Wextra
ASFLAGS = --32
LDFLAGS = -m elf_i386 -T linker.ld

BUILD = build

all: $(BUILD)/kernel.elf

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: boot/boot.s | $(BUILD)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/kernel.o: kernel/kernel.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/keyboard.o: drivers/keyboard/keyboard.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(BUILD)/boot.o $(BUILD)/kernel.o $(BUILD)/keyboard.o linker.ld
	$(LD) $(LDFLAGS) -o $@ $(BUILD)/boot.o $(BUILD)/kernel.o $(BUILD)/keyboard.o

clean:
	rm -rf $(BUILD) iso/boot/kernel.elf campusos.iso

.PHONY: all clean
>>>>>>> origin/subham
