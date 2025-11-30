/**
 * System Call Interface
 *
 * ABI: INT 0x80 (classic Linux-style syscalls)
 *
 * Register convention:
 * - EAX: syscall number
 * - EBX: arg0
 * - ECX: arg1
 * - EDX: arg2
 * - ESI: arg3
 * - EDI: arg4
 * - Return value: EAX
 *
 * Stack behavior:
 * - Ring 3 → Ring 0: CPU switches to TSS.esp0, pushes SS/ESP/EFLAGS/CS/EIP
 * - Ring 0 → Ring 0: CPU uses current stack, pushes EFLAGS/CS/EIP only
 *
 * INT 0x80 gate MUST be DPL=3 (type 0xEE) to allow userspace calls.
 */

#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include <kernel/types.h>

/**
 * Syscall error codes (negative return values)
 */
#define ENOSYS          38   // Function not implemented

/**
 * Syscall numbers
 *
 * Start at 1 (0 is reserved for "invalid syscall")
 *
 * **IMPORTANT:** SYS_SLEEP_US is NOT implemented (returns -ENOSYS).
 * Full implementation (sleep queues + timer wakeup) will be added in Phase 4.
 */
#define SYS_EXIT        1    // Exit current task
#define SYS_YIELD       2    // Yield CPU to another task
#define SYS_GETPID      3    // Get current task ID
#define SYS_SLEEP_US    4    // Sleep for microseconds (NOT IMPLEMENTED - returns -ENOSYS)

#define MAX_SYSCALLS    256  // Maximum number of syscalls

/**
 * Syscall function signature
 *
 * All syscalls take up to 5 long arguments and return long.
 * Actual syscalls may use fewer arguments.
 */
typedef long (*syscall_fn_t)(long arg0, long arg1, long arg2, long arg3, long arg4);

/**
 * Syscall dispatcher (called from INT 0x80 handler)
 *
 * @param syscall_num  Syscall number (from EAX)
 * @param arg0         First argument (from EBX)
 * @param arg1         Second argument (from ECX)
 * @param arg2         Third argument (from EDX)
 * @param arg3         Fourth argument (from ESI)
 * @param arg4         Fifth argument (from EDI)
 * @return             Return value (placed in EAX)
 *
 * RT: < 2µs for simple syscalls
 */
long syscall_handler(uint32_t syscall_num, long arg0, long arg1,
                     long arg2, long arg3, long arg4);

/**
 * Initialize syscall subsystem
 *
 * Registers INT 0x80 handler in IDT.
 * Must be called after IDT initialization.
 */
void syscall_init(void);

#endif // KERNEL_SYSCALL_H
