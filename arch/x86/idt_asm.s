# x86 IDT Assembly Stubs
# These are the first-level interrupt handlers that save state
# and call the C handlers

.section .text

# Macro for ISRs that don't push error code
.macro ISR_NOERRCODE num
.global isr\num
isr\num:
    pushl $0                    # Push dummy error code
    pushl $\num                 # Push interrupt number
    jmp isr_common_stub
.endm

# Macro for ISRs that push error code
.macro ISR_ERRCODE num
.global isr\num
isr\num:
    pushl $\num                 # Push interrupt number (error code already pushed by CPU)
    jmp isr_common_stub
.endm

# Macro for IRQs
.macro IRQ num, irq_num
.global irq\irq_num
irq\irq_num:
    pushl $0                    # Push dummy error code
    pushl $\num                 # Push interrupt number
    jmp irq_common_stub
.endm

# CPU exception handlers (0-31)
ISR_NOERRCODE 0                 # Divide by zero
ISR_NOERRCODE 1                 # Debug
ISR_NOERRCODE 2                 # NMI
ISR_NOERRCODE 3                 # Breakpoint
ISR_NOERRCODE 4                 # Overflow
ISR_NOERRCODE 5                 # Bound range exceeded
ISR_NOERRCODE 6                 # Invalid opcode
ISR_NOERRCODE 7                 # Device not available
ISR_ERRCODE 8                   # Double fault
ISR_NOERRCODE 9                 # Coprocessor segment overrun
ISR_ERRCODE 10                  # Invalid TSS
ISR_ERRCODE 11                  # Segment not present
ISR_ERRCODE 12                  # Stack-segment fault
ISR_ERRCODE 13                  # General protection fault
ISR_ERRCODE 14                  # Page fault
ISR_NOERRCODE 15                # Reserved
ISR_NOERRCODE 16                # x87 FPU error
ISR_ERRCODE 17                  # Alignment check
ISR_NOERRCODE 18                # Machine check
ISR_NOERRCODE 19                # SIMD FP exception
ISR_NOERRCODE 20                # Virtualization exception
ISR_NOERRCODE 21                # Reserved
ISR_NOERRCODE 22                # Reserved
ISR_NOERRCODE 23                # Reserved
ISR_NOERRCODE 24                # Reserved
ISR_NOERRCODE 25                # Reserved
ISR_NOERRCODE 26                # Reserved
ISR_NOERRCODE 27                # Reserved
ISR_NOERRCODE 28                # Reserved
ISR_NOERRCODE 29                # Reserved
ISR_ERRCODE 30                  # Security exception
ISR_NOERRCODE 31                # Reserved

# IRQ handlers (32-47, remapped from IRQ 0-15)
IRQ 32, 0                       # Timer
IRQ 33, 1                       # Keyboard
IRQ 34, 2                       # Cascade
IRQ 35, 3                       # COM2
IRQ 36, 4                       # COM1
IRQ 37, 5                       # LPT2
IRQ 38, 6                       # Floppy
IRQ 39, 7                       # LPT1
IRQ 40, 8                       # RTC
IRQ 41, 9                       # Free
IRQ 42, 10                      # Free
IRQ 43, 11                      # Free
IRQ 44, 12                      # PS/2 Mouse
IRQ 45, 13                      # FPU
IRQ 46, 14                      # Primary ATA
IRQ 47, 15                      # Secondary ATA

# Common ISR stub - saves all registers and calls C handler
isr_common_stub:
    # Save all general-purpose registers in the order our struct expects
    # struct interrupt_frame expects: ds, edi, esi, ebp, esp, ebx, edx, ecx, eax
    # So we push in REVERSE order (last field first)
    pushl %eax
    pushl %ecx
    pushl %edx
    pushl %ebx

    # Save original ESP (before we pushed anything)
    # At this point: ESP points here, we've pushed eax,ecx,edx,ebx (16 bytes)
    # Before that: int_no (4) and err_code (4) were pushed
    # So original ESP = current ESP + 16 + 8 = ESP + 24
    movl %esp, %eax
    addl $24, %eax
    pushl %eax

    pushl %ebp
    pushl %esi
    pushl %edi

    # Save data segment
    movw %ds, %ax
    pushl %eax

    # Load kernel data segment
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    # Call C handler
    pushl %esp                  # Push pointer to interrupt_frame
    call isr_handler
    addl $4, %esp               # Clean up pushed ESP

    # Restore data segment
    popl %eax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    # Restore general-purpose registers (reverse order of push)
    popl %edi
    popl %esi
    popl %ebp
    addl $4, %esp               # Skip saved ESP
    popl %ebx
    popl %edx
    popl %ecx
    popl %eax

    # Clean up error code and interrupt number
    addl $8, %esp

    # Return from interrupt
    iret

# Common IRQ stub - saves all registers and calls C handler
irq_common_stub:
    # Save all general-purpose registers (same as ISR stub)
    pushl %eax
    pushl %ecx
    pushl %edx
    pushl %ebx

    # Save original ESP
    movl %esp, %eax
    addl $24, %eax
    pushl %eax

    pushl %ebp
    pushl %esi
    pushl %edi

    # Save data segment
    movw %ds, %ax
    pushl %eax

    # Load kernel data segment
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    # Call C handler
    pushl %esp
    call irq_handler
    addl $4, %esp

    # Restore data segment
    popl %eax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    # Restore general-purpose registers
    popl %edi
    popl %esi
    popl %ebp
    addl $4, %esp               # Skip saved ESP
    popl %ebx
    popl %edx
    popl %ecx
    popl %eax

    # Clean up error code and interrupt number
    addl $8, %esp

    # Return from interrupt
    iret
