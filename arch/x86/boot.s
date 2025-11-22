.section .multiboot, "a"
.align 4
multiboot_header:
    .long 0x1BADB002              # Multiboot magic number
    .long 0x00000000               # Flags (none - let GRUB provide defaults)
    .long -(0x1BADB002 + 0x00000000)    # Checksum

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

    # Save multiboot info pointer (EBX contains multiboot_info struct pointer)
    # EAX contains multiboot magic number
    pushl %ebx                # Push multiboot_info pointer as second argument
    pushl %eax                # Push multiboot magic as first argument

    # Call the C kernel main function
    # void kmain(uint32_t multiboot_magic, uint32_t multiboot_info_addr)
    call kmain

    # If kmain returns, halt the CPU
    cli
hang:
    hlt
    jmp hang

.size _start, . - _start
