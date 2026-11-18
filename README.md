# CampusOS

### A Custom 32-bit Operating System Kernel for College Laboratory Environments

**Version:** 1.0  
**Architecture:** x86 (32-bit)  
**Language:** C and x86 Assembly  
**Boot Protocol:** Multiboot  
**Development Environment:** Ubuntu Linux, VS Code and QEMU

---

## 1. Introduction

CampusOS is a custom 32-bit operating system kernel developed as a college project. Its initial goal is to build a simple, functional operating system kernel with basic hardware interaction, memory management, interrupt handling and process management.

The project is built from the ground up using C and x86 Assembly. It uses the GRUB bootloader to load the kernel and QEMU to test and run the operating system in a virtual machine.

The current version provides:

- Kernel initialization and booting.
- CPU descriptor and interrupt management.
- Hardware timer and keyboard input.
- Physical and virtual memory management.
- Dynamic kernel memory allocation.
- Basic process management.
- CPU exception handling.
- An interactive command-line shell.
- Memory and kernel subsystem diagnostic tests.

CampusOS is currently focused on providing a stable and functional kernel foundation.

## 2. Project Objectives

The main objectives of CampusOS are:

1. Understand operating system internals through practical implementation.
2. Develop a bootable 32-bit x86 kernel.
3. Implement basic CPU interrupt and exception handling.
4. Manage physical and virtual memory.
5. Provide dynamic memory allocation inside the kernel.
6. Implement basic process creation and destruction.
7. Support keyboard input and timer interrupts.
8. Develop an interactive command shell.
9. Test kernel components independently.

## 3. System Architecture

CampusOS follows a modular kernel architecture.

### Architecture Overview

```text
              GRUB Bootloader
                     |
                     v
              Kernel Entry Point
                     |
                     v
              Kernel Initialization
                     |
       +-------------+-------------+
       |             |             |
       v             v             v
      GDT           IDT       Memory Management
       |             |             |
       |        +----+----+        |
       |        |         |        |
       |        v         v        v
       |       PIC       CPU       PMM
       |        |      Exceptions  |
       |        v                   v
       |       IRQ                  VMM
       |        |                   |
       |        v                   v
       |       PIT              Kernel Heap
       |        |
       |        v
       |     Keyboard
       |        |
       +--------+------------------+
                     |
                     v
              Kernel Command Shell
                     |
                     v
               User Commands
```

The diagram represents the major implemented subsystems and their relationships. It does not imply that every component is an independent kernel process.

## 4. Development Technologies

| Technology | Purpose |
|---|---|
| C | Kernel implementation |
| x86 Assembly | Low-level CPU operations |
| GRUB | Kernel bootloader |
| Multiboot | Kernel loading information |
| QEMU | Kernel emulation and testing |
| Ubuntu | Development environment |
| GCC cross-compiler/toolchain | Kernel compilation |
| Make | Build automation |
| Git and GitHub | Version control |

## 5. Boot Process

CampusOS uses GRUB to load its kernel.

The boot process follows these steps:

1. QEMU starts the virtual machine.
2. GRUB loads the CampusOS kernel.
3. The Multiboot boot information is passed to the kernel.
4. The kernel validates the Multiboot magic value and boot information address.
5. The Global Descriptor Table is initialized.
6. The Interrupt Descriptor Table is initialized.
7. Physical memory management is initialized.
8. Virtual memory management is initialized.
9. The kernel heap is initialized.
10. Process management is initialized.
11. The required hardware interrupt controllers and drivers are initialized.
12. The keyboard and timer become available.
13. The kernel enables interrupts.
14. The interactive command shell starts.

The kernel checks the Multiboot information before proceeding. If the information is invalid, it displays an error and halts.

## 6. Kernel Components

### 6.1 Global Descriptor Table (GDT)

The GDT defines the memory segments used by the processor.

CampusOS initializes its GDT during kernel startup.

Its purpose is to establish the processor's segmentation configuration before the kernel continues initialization.

### 6.2 Interrupt Descriptor Table (IDT)

The IDT stores the addresses of interrupt and exception handlers.

It allows the CPU to transfer control to the appropriate handler when an interrupt or exception occurs.

CampusOS uses the IDT for CPU exception handling and hardware interrupt handling.

### 6.3 Programmable Interrupt Controller (PIC)

The PIC manages hardware interrupt requests.

It is initialized to allow the kernel to receive hardware interrupts, including timer and keyboard interrupts.

### 6.4 Programmable Interval Timer (PIT)

The PIT generates periodic timer interrupts.

CampusOS initializes the timer at 100 Hz.

This provides approximately 100 timer ticks per second.

The shell displays the current timer tick count through the `ticks` command and updates the timer information on screen.

### 6.5 CPU Exception Handling

CampusOS includes exception handling for CPU errors.

The exception handler displays diagnostic information such as:

- Exception number.
- Exception name.
- CPU error code.
- Instruction address.
- Page fault address.
- Page fault access information.

A dedicated Double Fault handler is also implemented using a separate Task State Segment (TSS) configuration.

These handlers help identify critical processor errors during kernel execution.

### 6.6 Keyboard Driver

The keyboard driver reads keyboard input through the x86 keyboard controller ports.

It currently supports:

- Number keys.
- Lowercase alphabetic keys.
- Space.
- Enter.
- Backspace.

The shell uses keyboard input to receive and execute commands.

### 6.7 Physical Memory Manager (PMM)

The Physical Memory Manager handles physical memory frame allocation and deallocation.

Its responsibilities include:

- Allocating physical memory frames.
- Releasing allocated frames.
- Tracking total physical frames.
- Tracking available physical frames.

The kernel uses PMM functions when memory is required by other subsystems.

### 6.8 Virtual Memory Manager (VMM)

The Virtual Memory Manager handles virtual memory mapping.

Its responsibilities include:

- Allocating virtual memory pages.
- Mapping virtual addresses to physical frames.
- Translating virtual addresses into physical addresses.
- Releasing allocated pages.
- Managing page tables.
- Supporting page protection.

The kernel uses virtual memory to access allocated memory through virtual addresses.

### 6.9 Kernel Heap

The kernel heap provides dynamic memory allocation.

CampusOS provides:

- `kmalloc()` for allocating memory.
- `kfree()` for releasing allocated memory.

The heap uses the underlying memory management subsystem to obtain memory.

It also provides allocation statistics, including total and used heap pages and allocation counts.

### 6.10 Process Management

CampusOS includes basic process management.

The implemented process management functionality includes:

- Process initialization.
- Process creation.
- Process identifiers (PIDs).
- Process state tracking.
- Kernel stack allocation.
- Process lookup.
- Process destruction.
- Process count tracking.

Each created process receives its own process record and kernel stack.

The current implementation focuses on process creation, management and cleanup. It does not claim to provide a complete multitasking scheduler or CPU context switching.

## 7. Interactive Command Shell

CampusOS includes a basic command-line shell that accepts commands through the keyboard.

The shell uses VGA text-mode output to display information.

The prompt is:

```text
CampusOS>
```

### Available Commands

| Command | Description |
|---|---|
| `help` | Displays available commands |
| `clear` | Clears the shell screen |
| `ticks` | Displays the current timer tick count |
| `about` | Displays CampusOS information |
| `memory` | Displays physical memory statistics |
| `memtest` | Tests physical memory allocation |
| `vmtest` | Tests virtual memory mapping |
| `heap` | Displays kernel heap statistics |
| `heaptest` | Tests kernel heap allocation |

### Example Shell Session

```text
CampusOS: Kernel initialized

Timer ticks: 125
CampusOS Command Shell

CampusOS> about

CampusOS - College Lab Operating System
Architecture: x86 32-bit
Kernel: Custom freestanding kernel

CampusOS> memory

Total frames:  ...
Free frames:   ...
Used frames:   ...
Page tables:   ...

CampusOS> ticks

Current timer ticks: ...

CampusOS> help

Available commands:
help - Show commands
clear - Clear screen
ticks - Show timer
about - OS information
memory - RAM statistics
memtest - Test PMM
vmtest - Test VMM
heap - Heap statistics
heaptest - Test heap
```

The numbers in this example are placeholders. Actual values depend on the virtual machine's memory configuration and runtime.

## 8. Testing and Validation

CampusOS includes diagnostic tests for its major kernel subsystems.

### 8.1 Physical Memory Test

The `memtest` command checks:

- Physical frame allocation.
- Allocation of distinct frames.
- Frame deallocation.
- Restoration of the available frame count.

### 8.2 Virtual Memory Test

The `vmtest` command checks:

- Virtual page allocation.
- Memory write and read operations.
- Virtual-to-physical address translation.
- Page deallocation.
- Restoration of memory statistics.

### 8.3 Kernel Heap Test

The `heaptest` command checks:

- Small memory allocation.
- Medium memory allocation.
- Large memory allocation.
- Memory write and read integrity.
- Memory deallocation.
- Restoration of allocation counts.

### 8.4 Memory Stress Test

The kernel includes a memory stress test that checks:

- Repeated allocation and deallocation cycles.
- Small memory block integrity.
- Large memory block integrity.
- Allocation failure handling.
- Memory cleanup.
- Memory leak indicators through allocation and page counts.

### 8.5 Process Management Test

The process management test checks:

- Process initialization.
- Creation of multiple processes.
- Unique process IDs.
- Separate kernel stacks.
- Process state initialization.
- Process destruction.
- Restoration of heap allocation counts.

### 8.6 Exception and Memory Protection Tests

The project includes CPU exception diagnostics and page fault testing code.

The advanced page fault recovery test should be enabled and verified separately before claiming that all memory protection tests pass in the final release.

### Testing Summary

The basic kernel components and process management tests have been reported as passing during development.

Testing should be repeated after any major code changes to ensure that existing functionality remains stable.

## 9. Project Directory Structure

The project is organized into source, header and driver directories.

The following is a representative structure; individual filenames may differ as the project develops.

```text
CampusOS/
│
├── src/
│   ├── kernel.c
│   ├── gdt.c
│   ├── idt.c
│   ├── irq.c
│   ├── exceptions.c
│   ├── pmm.c
│   ├── paging.c
│   ├── vmm.c
│   ├── heap.c
│   └── process.c
│
├── include/
│   ├── gdt.h
│   ├── interrupts.h
│   ├── keyboard.h
│   ├── pmm.h
│   ├── paging.h
│   ├── vmm.h
│   ├── heap.h
│   └── process.h
│
├── drivers/
│   └── keyboard/
│       └── keyboard.c
│
├── iso/
│   └── boot/
│       └── grub/
│           └── grub.cfg
│
├── Makefile
├── README.md
└── .gitignore
```

Some low-level assembly files and additional modules may exist outside this representative listing.

## 10. Build and Run Instructions

### Requirements

- Ubuntu Linux.
- GCC and required compilation tools.
- NASM.
- GRUB utilities.
- xorriso.
- QEMU.
- Make.

Install the commonly required tools on Ubuntu:

```bash
sudo apt update

sudo apt install build-essential nasm \
grub-pc-bin grub-common xorriso \
qemu-system-x86
```

The project may also require the configured 32-bit cross-compilation toolchain. Use the compiler specified in the project's Makefile.

### Clone the Repository

```bash
git clone <YOUR_GITHUB_REPOSITORY_URL>

cd CampusOS
```

Replace the placeholder with the actual repository URL.

### Build the Kernel

```bash
make clean
make
```

### Run CampusOS

```bash
make run
```

The `make run` command uses the QEMU configuration defined in the project's Makefile.

## 11. Current Features and Limitations

### Implemented

- 32-bit x86 kernel.
- GRUB Multiboot booting.
- GDT and IDT initialization.
- PIC and PIT initialization.
- Keyboard input.
- CPU exception handling.
- Double Fault handling.
- Physical memory management.
- Virtual memory management.
- Kernel heap allocation.
- Basic process management.
- VGA-based text output.
- Interactive command shell.
- Kernel diagnostic tests.

### Not Included in the Current Version

The following features are outside the scope of the current basic kernel release:

- Complete multitasking scheduler.
- CPU context switching.
- User-mode applications.
- System call interface.
- Filesystem.
- Disk storage driver.
- Networking.
- Graphical user interface.
- Inter-process communication.

These are not claimed as implemented features.

## 12. Future Scope

Possible future improvements include:

- Improved keyboard input and terminal support.
- Better terminal scrolling.
- Complete multitasking support.
- Filesystem implementation.
- Disk access.
- Networking support.
- User-mode execution.
- Additional device drivers.
- Centralized laboratory management features.

These improvements can be considered in later versions.

## 13. Team

CampusOS is developed as a collaborative college project.

| Team Member | Contribution |
|---|---|
| Rahul Singh Negi | Kernel core, interrupt handling, security and debugging |
| Subham Negi | Collaborative development and kernel integration |
| Pawan Kanwal | Collaborative development and kernel integration |

The exact module ownership can be expanded as the team finalizes its contribution records.

## 14. Version Information

**Project:** CampusOS  
**Release:** 1.0  
**Architecture:** x86 32-bit  
**Project Type:** Custom Operating System Kernel  
**Status:** Basic working kernel

---

## 15. Conclusion

CampusOS provides a working foundation for understanding and developing a custom operating system kernel.

The current implementation covers kernel booting, hardware interrupts, CPU exception handling, memory management, basic process management and an interactive command shell.

The project demonstrates how low-level kernel components work together to provide the basic functionality of an operating system.

The current release focuses on a simple and functional kernel rather than a feature-complete desktop operating system.

