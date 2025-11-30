#ifndef KERNEL_TASK_H
#define KERNEL_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include <kernel/types.h>
#include <kernel/mmu.h>

/**
 * Task/Thread Management
 *
 * This is the fundamental execution unit for Phase 3.
 * Initially: kernel-only threads (no user mode)
 * Future: full userspace support via units model (see VISION.md)
 *
 * RT Constraints (from RT_CONSTRAINTS.md):
 * - Context switch: < 200 cycles
 * - Task create/destroy: O(1)
 * - Priority-based scheduling with O(1) pick
 */

// Forward declarations
struct task;
typedef struct task task_t;

/**
 * Task state
 */
typedef enum {
    TASK_STATE_READY,       // Ready to run
    TASK_STATE_RUNNING,     // Currently executing
    TASK_STATE_BLOCKED,     // Waiting for event
    TASK_STATE_ZOMBIE,      // Exited, waiting for cleanup
} task_state_t;

/**
 * CPU context for context switching
 *
 * This is the register state saved during context switch.
 * Must match the layout expected by context_switch() in arch/x86/context.s
 */
typedef struct {
    // General-purpose registers (saved by context_switch)
    uint32_t edi;
    uint32_t esi;
    uint32_t ebx;
    uint32_t ebp;
    uint32_t esp;           // Stack pointer

    // Instruction pointer
    uint32_t eip;           // Where to resume execution

    // Segment registers (for future user mode)
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t es;
    uint32_t fs;
    uint32_t gs;

    // EFLAGS
    uint32_t eflags;
} __attribute__((packed)) cpu_context_t;

/**
 * Task Control Block (TCB)
 *
 * Represents a single thread of execution.
 * For now: kernel threads only (no user mode, no separate address spaces)
 */
struct task {
    // Identity
    uint32_t        task_id;            // Unique task ID
    char            name[32];           // Human-readable name (for debugging)

    // State
    task_state_t    state;              // Current state
    int             exit_code;          // Exit code (if ZOMBIE)

    // CPU context
    cpu_context_t   context;            // Saved CPU state

    // Memory
    page_table_t*   address_space;      // Address space (for now: kernel's)
    void*           kernel_stack;       // Kernel stack pointer
    size_t          kernel_stack_size;  // Stack size

    // Scheduling
    uint8_t         priority;           // 0 (lowest) - 255 (highest)
    uint64_t        cpu_time_ticks;     // Total CPU time in timer ticks
    uint64_t        last_run_tick;      // When last scheduled

    // Scheduler linkage
    struct task*    next;               // Next in run queue
    struct task*    prev;               // Previous in run queue

    // Future: capability table, unit membership, etc.
};

/**
 * Initialize task subsystem
 *
 * Must be called once during kernel initialization.
 */
void task_init(void);

/**
 * Allocate a new task structure
 *
 * @return  Pointer to allocated task, or NULL on failure
 *
 * RT: O(1) - single page allocation
 */
task_t* task_alloc(void);

/**
 * Create a new kernel thread
 *
 * @param name          Human-readable name
 * @param entry_point   Function to execute
 * @param arg           Argument to pass to entry_point
 * @param priority      Priority (0-255, higher = more important)
 * @param stack_size    Stack size in bytes (must be page-aligned)
 * @return              New task, or NULL on failure
 *
 * RT: O(1) - allocates fixed structures, no loops
 */
task_t* task_create_kernel_thread(const char* name,
                                   void (*entry_point)(void* arg),
                                   void* arg,
                                   uint8_t priority,
                                   size_t stack_size);

/**
 * Destroy a task
 *
 * Frees task resources and removes from scheduler.
 * Must NOT be called on currently running task.
 *
 * @param task  Task to destroy
 *
 * RT: O(1) - no unbounded operations
 */
void task_destroy(task_t* task);

/**
 * Exit current task
 *
 * Marks task as ZOMBIE and yields to scheduler.
 * Does not return.
 *
 * @param exit_code  Exit code
 */
void task_exit(int exit_code) __attribute__((noreturn));

/**
 * Yield CPU to scheduler
 *
 * Current task remains READY but scheduler may pick another task.
 *
 * RT: < 200 cycles total (includes context switch)
 */
void task_yield(void);

/**
 * Get current task
 *
 * Returns the currently executing task.
 *
 * @return  Current task pointer
 *
 * RT: O(1), < 10 cycles
 */
task_t* task_current(void);

/**
 * Get idle task
 *
 * Returns the idle task (always schedulable, lowest priority).
 *
 * @return  Idle task pointer
 */
task_t* task_get_idle(void);

#endif // KERNEL_TASK_H
