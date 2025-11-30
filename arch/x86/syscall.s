/**
 * x86 Syscall Entry/Exit (INT 0x80)
 *
 * This is the low-level assembly for system call entry and exit.
 *
 * Stack behavior on INT 0x80:
 * - From ring 3: CPU switches to TSS.esp0, pushes SS, ESP, EFLAGS, CS, EIP
 * - From ring 0: CPU uses current stack, pushes EFLAGS, CS, EIP only
 *
 * We use interrupt gate (type 0xEE = DPL 3, clears IF):
 * - Interrupts disabled during syscall entry/exit
 * - Can be changed to trap gate (0xEF) later for preemptible syscalls
 *
 * Register convention (matches Linux i386):
 * - EAX: syscall number
 * - EBX: arg0
 * - ECX: arg1
 * - EDX: arg2
 * - ESI: arg3
 * - EDI: arg4
 * - Return: EAX
 */

.section .text
.global syscall_entry_int80

/**
 * INT 0x80 entry point
 *
 * CPU has already:
 * - Switched to kernel stack (if from ring 3)
 * - Pushed error code / segment registers / return address
 * - Disabled interrupts (we use interrupt gate 0xEE)
 */
syscall_entry_int80:
    # Save all general-purpose registers
    # Order: EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI (pusha order)
    pushal

    # Save segment registers
    push %ds
    push %es
    push %fs
    push %gs

    # Load kernel data segments
    movw $0x10, %ax         # Kernel data segment
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    # pusha pushes in this order: EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
    # After pusha + segment pushes, stack layout:
    # ESP+0:  DS
    # ESP+4:  ES
    # ESP+8:  FS
    # ESP+12: GS
    # ESP+16: EDI (arg4)
    # ESP+20: ESI (arg3)
    # ESP+24: EBP
    # ESP+28: ESP (orig)
    # ESP+32: EBX (arg0)
    # ESP+36: EDX (arg2)
    # ESP+40: ECX (arg1)
    # ESP+44: EAX (syscall_num)

    # Load arguments into temporaries BEFORE pushing
    # (Each push changes ESP, invalidating subsequent offsets)
    movl 44(%esp), %eax     # syscall_num (from saved EAX)
    movl 32(%esp), %ebx     # arg0 (from saved EBX)
    movl 40(%esp), %ecx     # arg1 (from saved ECX)
    movl 36(%esp), %edx     # arg2 (from saved EDX)
    # Note: ESI and EDI are already in registers, but we pushed them
    # Need to reload from stack
    movl 20(%esp), %esi     # arg3 (from saved ESI)
    movl 16(%esp), %edi     # arg4 (from saved EDI)

    # Now push arguments in reverse order for C calling convention
    pushl %edi              # arg4
    pushl %esi              # arg3
    pushl %edx              # arg2
    pushl %ecx              # arg1
    pushl %ebx              # arg0
    pushl %eax              # syscall_num

    # Call C handler
    call syscall_handler

    # Clean up arguments (6 * 4 = 24 bytes)
    addl $24, %esp

    # Return value is in EAX, we need to update the saved EAX on stack
    # Stack layout (ESP points here):
    # ESP+0:  DS
    # ESP+4:  ES
    # ESP+8:  FS
    # ESP+12: GS
    # ESP+16: EDI
    # ESP+20: ESI
    # ESP+24: EBP
    # ESP+28: ESP (orig)
    # ESP+32: EBX
    # ESP+36: EDX
    # ESP+40: ECX
    # ESP+44: EAX (we want to update this with return value)

    movl %eax, 44(%esp)     # Update saved EAX with return value

    # Restore segment registers
    pop %gs
    pop %fs
    pop %es
    pop %ds

    # Restore general-purpose registers (includes updated EAX)
    popal

    # Return to caller (restores EFLAGS, CS, EIP, and SS/ESP if from ring 3)
    iret

/**
 * syscall_int80 - Helper for testing INT 0x80 from C code
 *
 * C signature:
 *   long syscall_int80(long syscall_num, long arg0, long arg1,
 *                      long arg2, long arg3, long arg4);
 *
 * This is for Phase B testing (INT 0x80 from ring 0).
 * Loads arguments into registers and executes INT 0x80.
 */
.global syscall_int80
syscall_int80:
    # Save callee-saved registers
    pushl %ebx
    pushl %esi
    pushl %edi
    pushl %ebp

    # Load arguments from stack (after 4 pushes + return address)
    # Stack: [ebp][edi][esi][ebx][ret][num][arg0][arg1][arg2][arg3][arg4]
    movl 20(%esp), %eax     # syscall_num (offset 20 = 4*4 pushes + 4 ret addr)
    movl 24(%esp), %ebx     # arg0
    movl 28(%esp), %ecx     # arg1
    movl 32(%esp), %edx     # arg2
    movl 36(%esp), %esi     # arg3
    movl 40(%esp), %edi     # arg4

    # Execute INT 0x80
    int $0x80

    # Return value is already in EAX

    # Restore callee-saved registers
    popl %ebp
    popl %edi
    popl %esi
    popl %ebx

    ret
