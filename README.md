# CampusOS

CampusOS is a freestanding 32-bit x86 kernel project.

## Project layout

- `src/` contains the kernel entry point, descriptor tables, exceptions, IRQs, and paging.
- `include/` contains shared kernel interfaces.
- `iso/boot/grub/grub.cfg` configures the bootable GRUB image.
- `linker.ld` defines the kernel memory layout.
- `INTERRUPT_MODULE.md` documents the interrupt handling module.

## Build and run

The build requires GCC with 32-bit support, NASM, GNU binutils, GRUB utilities, and xorriso. QEMU is required for the optional `run` target.

```sh
make
make run
make clean
```

`make` produces `CampusOS.iso`. Build outputs are excluded from version control.