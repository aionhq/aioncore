ENTRY(_start)

SECTIONS
{
    . = 1M;

    /* Kernel start address */
    _kernel_start = .;

    .multiboot ALIGN(4K) :
    {
        *(.multiboot)
    }

    .text ALIGN(4K) :
    {
        *(.text)
    }

    .rodata ALIGN(4K) :
    {
        *(.rodata)

        /* Kernel test section */
        . = ALIGN(8);
        __start_ktests = .;
        *(.ktests)
        __stop_ktests = .;
    }

    .data ALIGN(4K) :
    {
        *(.data)
    }

    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss)
    }

    /* Kernel end address */
    _kernel_end = .;

    /DISCARD/ :
    {
        *(.note.gnu.build-id)
        *(.eh_frame)
        *(.comment)
    }
}
