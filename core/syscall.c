/**
 * System Call Dispatcher and Implementation
 *
 * Provides the syscall table and implementations of core syscalls.
 *
 * RT Constraints:
 * - sys_exit: O(1), < 500 cycles
 * - sys_yield: O(1), < 200 cycles (just calls schedule)
 * - sys_getpid: O(1), < 20 cycles (register read)
 * - sys_sleep_us: O(1) per call, but blocks task
 */

#include <kernel/syscall.h>
#include <kernel/task.h>
#include <kernel/scheduler.h>
#include <kernel/percpu.h>
#include <kernel/hal.h>
#include <kernel/idt.h>
#include <drivers/vga.h>

// Forward declarations of syscall implementations
static long sys_exit(long arg0, long arg1, long arg2, long arg3, long arg4);
static long sys_yield(long arg0, long arg1, long arg2, long arg3, long arg4);
static long sys_getpid(long arg0, long arg1, long arg2, long arg3, long arg4);
static long sys_sleep_us(long arg0, long arg1, long arg2, long arg3, long arg4);

/**
 * Syscall table
 *
 * Maps syscall numbers to implementation functions.
 * Entries are syscall_fn_t with uniform signature.
 */
static const syscall_fn_t syscall_table[MAX_SYSCALLS] = {
    [0] = NULL,                  // Reserved (invalid syscall)
    [SYS_EXIT] = sys_exit,       // Exit task
    [SYS_YIELD] = sys_yield,     // Yield CPU
    [SYS_GETPID] = sys_getpid,   // Get task ID
    [SYS_SLEEP_US] = sys_sleep_us, // Sleep microseconds
    // Rest are NULL (not implemented)
};

/**
 * Syscall dispatcher (called from INT 0x80 handler)
 *
 * Validates syscall number and dispatches to implementation.
 *
 * @param syscall_num  Syscall number (from EAX)
 * @param arg0-arg4    Arguments (from EBX, ECX, EDX, ESI, EDI)
 * @return             Return value (placed in EAX), or -ENOSYS if invalid
 */
long syscall_handler(uint32_t syscall_num, long arg0, long arg1,
                     long arg2, long arg3, long arg4) {
    // Validate syscall number
    if (syscall_num >= MAX_SYSCALLS) {
        // Note: kprintf removed from hot path (can reenter console from IRQ)
        return -ENOSYS;
    }

    // Look up syscall function
    syscall_fn_t syscall = syscall_table[syscall_num];
    if (!syscall) {
        // Note: kprintf removed from hot path
        return -ENOSYS;
    }

    // Call syscall implementation
    return syscall(arg0, arg1, arg2, arg3, arg4);
}

/**
 * sys_exit - Exit current task
 *
 * @param arg0  Exit status code
 * @return      Does not return
 *
 * RT: O(1), < 500 cycles
 */
static long sys_exit(long arg0, long arg1, long arg2, long arg3, long arg4) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4;
    int status = (int)arg0;

    // Use scheduler's current_task (not per-CPU, which may not be set yet)
    extern scheduler_t g_scheduler;
    task_t* current = g_scheduler.current_task;
    if (current) {
        kprintf("[SYSCALL] sys_exit(%d) from task '%s'\n", status, current->name);
    } else {
        kprintf("[SYSCALL] sys_exit(%d) from unknown task\n", status);
    }

    task_exit(status);
    // Never returns
    return 0;
}

/**
 * sys_yield - Yield CPU to another task
 *
 * Voluntarily gives up CPU to allow other tasks to run.
 * Current task remains READY and will be rescheduled later.
 *
 * @return  Always returns 0
 *
 * RT: O(1), < 200 cycles (calls scheduler)
 */
static long sys_yield(long arg0, long arg1, long arg2, long arg3, long arg4) {
    (void)arg0; (void)arg1; (void)arg2; (void)arg3; (void)arg4;
    task_yield();
    return 0;
}

/**
 * sys_getpid - Get current task ID
 *
 * @return  Current task's ID
 *
 * RT: O(1), < 20 cycles (register read)
 */
static long sys_getpid(long arg0, long arg1, long arg2, long arg3, long arg4) {
    (void)arg0; (void)arg1; (void)arg2; (void)arg3; (void)arg4;

    // Use scheduler's current_task (not per-CPU, which may not be set yet)
    extern scheduler_t g_scheduler;
    task_t* current = g_scheduler.current_task;
    if (!current) {
        kprintf("[SYSCALL] sys_getpid: current_task is NULL!\n");
        return -1;  // No current task (shouldn't happen)
    }

    return (long)current->task_id;
}

/**
 * sys_sleep_us - Sleep for microseconds (STUB)
 *
 * **IMPORTANT:** This is currently a STUB implementation that just yields once.
 * It does NOT actually sleep for the requested duration.
 *
 * The full implementation will be added in Phase 4 with:
 * - Sleep queue for blocked tasks
 * - Timer-based wakeup mechanism
 * - Proper accounting of elapsed time
 *
 * For now, this just proves the syscall mechanism works.
 *
 * @param arg0  Duration in microseconds (currently IGNORED)
 * @return      0 on success
 *
 * RT: O(1) - single yield, does not wait for duration
 */
static long sys_sleep_us(long arg0, long arg1, long arg2, long arg3, long arg4) {
    (void)arg0;  // Duration is ignored - not implemented
    (void)arg1; (void)arg2; (void)arg3; (void)arg4;

    // Not implemented yet
    // TODO Phase 4: Implement sleep queue with timer-based wakeup
    return -ENOSYS;
}

/**
 * Initialize syscall subsystem
 *
 * Registers INT 0x80 handler in IDT.
 * Must be called after IDT initialization.
 */
void syscall_init(void) {
    // INT 0x80 is registered in IDT during idt_init()
    // with DPL=3 to allow userspace calls
    // Nothing to do here for now

    kprintf("[SYSCALL] Syscall subsystem initialized (INT 0x80)\n");
}
