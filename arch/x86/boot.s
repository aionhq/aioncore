.section .multiboot, "a"
.align 4
multiboot_header:
    .long 0x1BADB002              # Multiboot magic number
    .long 0x00                     # Flags
    .long -(0x1BADB002 + 0x00)    # Checksum

.section .bss
.align 16
stack_bottom:
    .skip 16384                    # 16 KB stack
stack_top:

.section .text
.global _start
.type _start, @function

_start:
    # Set up the stack
    movl $stack_top, %esp
    movl $stack_top, %ebp

    # Reset EFLAGS
    pushl $0
    popf

    # Call the C kernel main function
    call kmain

    # If kmain returns, halt the CPU
    cli
hang:
    hlt
    jmp hang

.size _start, . - _start
