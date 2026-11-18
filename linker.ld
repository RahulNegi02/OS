ENTRY(_start)

SECTIONS
{
    . = 1M;

<<<<<<< HEAD
    .text ALIGN(4K) :
    {
        KEEP(*(.multiboot))
        *(.text*)
    }

    .rodata ALIGN(4K) :
=======
    .multiboot :
    {
        KEEP(*(.multiboot))
    }

    .text :
    {
        *(.text*)
    }

    .rodata :
>>>>>>> origin/subham
    {
        *(.rodata*)
    }

<<<<<<< HEAD
    .data ALIGN(4K) :
=======
    .data :
>>>>>>> origin/subham
    {
        *(.data*)
    }

<<<<<<< HEAD
    .bss ALIGN(4K) :
=======
    .bss :
>>>>>>> origin/subham
    {
        *(COMMON)
        *(.bss*)
    }
}