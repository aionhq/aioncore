/**
 * Task Management Implementation
 *
 * Implements task creation, destruction, and lifecycle management.
 * For Phase 3: kernel-only threads with shared kernel address space.
 */

#include <kernel/task.h>
#include <kernel/scheduler.h>
#include <kernel/pmm.h>
#include <kernel/mmu.h>
#include <kernel/hal.h>
#include <drivers/vga.h>
#include <lib/string.h>

// Next task ID (monotonically increasing)
static uint32_t next_task_id = 1;

// Idle task (always schedulable, never blocks)
static task_t* idle_task = NULL;

/**
 * Idle thread entry point
 *
 * Runs when no other tasks are ready.
 * Just halts CPU until next interrupt.
 */
static void idle_thread_entry(void* arg) {
    (void)arg;

    kprintf("[TASK] Idle thread started\n");

    while (1) {
        // Halt CPU until next interrupt
        // This saves power and lets interrupts wake us up
        hal->cpu_halt();
    }
}

/**
 * Initialize task subsystem
 */
void task_init(void) {
    kprintf("[TASK] Initializing task subsystem...\n");

    // Create idle task
    // Note: We can't use task_create_kernel_thread() yet because
    // the scheduler isn't initialized, so we manually create it
    idle_task = (task_t*)pmm_alloc_page();
    if (!idle_task) {
        kprintf("[TASK] FATAL: Failed to allocate idle task\n");
        return;
    }

    memset(idle_task, 0, sizeof(task_t));

    idle_task->task_id = 0;  // Idle task always has ID 0
    strlcpy(idle_task->name, "idle", sizeof(idle_task->name));
    idle_task->state = TASK_STATE_READY;
    idle_task->priority = SCHED_IDLE_PRIORITY;
    idle_task->address_space = mmu_get_kernel_address_space();

    // Allocate kernel stack for idle task
    idle_task->kernel_stack_size = 4096;
    idle_task->kernel_stack = (void*)pmm_alloc_page();
    if (!idle_task->kernel_stack) {
        kprintf("[TASK] FATAL: Failed to allocate idle task stack\n");
        return;
    }

    // Set up stack for idle_thread_entry(void*):
    // cdecl calling convention requires:
    // [esp]   = return address (unused, set to 0)
    // [esp+4] = first argument (NULL)
    // Stack grows DOWN, so push arg first (higher address), then return (lower address)
    uint8_t* stack_top = (uint8_t*)idle_task->kernel_stack + idle_task->kernel_stack_size;
    uint32_t* sp = (uint32_t*)stack_top;
    --sp;            // Push argument at higher address
    *sp = 0;         // arg = NULL (will be at [esp+4])
    --sp;            // Push return address at lower address
    *sp = 0;         // return = 0 (will be at [esp])

    idle_task->context.esp = (uint32_t)sp;
    idle_task->context.ebp = idle_task->context.esp;

    // Set entry point
    idle_task->context.eip = (uint32_t)idle_thread_entry;

    // Set up segment registers for kernel mode
    idle_task->context.cs = 0x08;  // Kernel code segment
    idle_task->context.ss = 0x10;  // Kernel data segment
    idle_task->context.ds = 0x10;
    idle_task->context.es = 0x10;
    idle_task->context.fs = 0x10;
    idle_task->context.gs = 0x10;

    // Set EFLAGS (IF=1 to enable interrupts)
    idle_task->context.eflags = 0x202;  // IF bit set

    kprintf("[TASK] Idle task created (ID: %u, stack: %p)\n",
            (unsigned int)idle_task->task_id, idle_task->kernel_stack);
}

/**
 * Get idle task
 */
task_t* task_get_idle(void) {
    return idle_task;
}

/**
 * Get current task
 */
task_t* task_current(void) {
    return scheduler_current();
}

/**
 * Task wrapper
 *
 * This function wraps the user's entry point and handles cleanup when
 * the thread exits.
 */
typedef struct {
    void (*entry)(void* arg);
    void* arg;
} task_wrapper_args_t;

static void task_wrapper(task_wrapper_args_t* wrapper_args) {
    // Call the actual entry point
    wrapper_args->entry(wrapper_args->arg);

    // If entry point returns, exit with code 0
    task_exit(0);
}

/**
 * Create a new kernel thread
 */
task_t* task_create_kernel_thread(const char* name,
                                   void (*entry_point)(void* arg),
                                   void* arg,
                                   uint8_t priority,
                                   size_t stack_size) {
    // Validate inputs
    if (!entry_point) {
        return NULL;
    }

    // IMPORTANT: We only allocate ONE page (4096 bytes) for the stack.
    // Enforce stack_size == 4096 to prevent memory corruption.
    // TODO Phase 4: Allocate multiple pages for larger stacks
    if (stack_size != 4096) {
        kprintf("[TASK] ERROR: stack_size must be exactly 4096 bytes (requested: %u)\n",
                (unsigned int)stack_size);
        return NULL;
    }

    // Allocate task struct
    task_t* task = (task_t*)pmm_alloc_page();
    if (!task) {
        kprintf("[TASK] Failed to allocate task struct\n");
        return NULL;
    }

    memset(task, 0, sizeof(task_t));

    // Assign task ID
    task->task_id = next_task_id++;

    // Copy name
    strlcpy(task->name, name, sizeof(task->name));

    // Set initial state
    task->state = TASK_STATE_READY;
    task->priority = priority;
    task->address_space = mmu_get_kernel_address_space();  // Kernel address space

    // Allocate kernel stack
    task->kernel_stack_size = stack_size;
    task->kernel_stack = (void*)pmm_alloc_page();
    if (!task->kernel_stack) {
        kprintf("[TASK] Failed to allocate stack for task %s\n", name);
        pmm_free_page((phys_addr_t)task);
        return NULL;
    }

    // Set up stack layout for task_wrapper(wrapper_args*):
    // cdecl calling convention requires:
    // [esp]   = return address (what function would RET to)
    // [esp+4] = first argument (pointer to wrapper_args)
    // wrapper_args struct lives just above this area on the stack.
    uint8_t* stack_top_bytes = (uint8_t*)task->kernel_stack + stack_size;
    uint32_t* sp = (uint32_t*)stack_top_bytes;

    // Reserve space for wrapper_args struct
    sp = (uint32_t*)((uint8_t*)sp - sizeof(task_wrapper_args_t));
    task_wrapper_args_t* wrapper_args = (task_wrapper_args_t*)sp;
    wrapper_args->entry = entry_point;
    wrapper_args->arg = arg;

    // Push first argument (pointer to wrapper_args)
    // This will be at [esp+4] after we push return address
    sp--;
    *sp = (uint32_t)wrapper_args;

    // Push return address (unused, set to 0)
    // This will be at [esp] when task_wrapper starts
    sp--;
    *sp = 0x00000000;

    // Set up initial context
    task->context.esp = (uint32_t)sp;
    task->context.ebp = task->context.esp;
    task->context.eip = (uint32_t)task_wrapper;

    // Set up segment registers for kernel mode
    task->context.cs = 0x08;  // Kernel code segment
    task->context.ss = 0x10;  // Kernel data segment
    task->context.ds = 0x10;
    task->context.es = 0x10;
    task->context.fs = 0x10;
    task->context.gs = 0x10;

    // Set EFLAGS (IF=1 to enable interrupts)
    task->context.eflags = 0x202;

    kprintf("[TASK] Created task '%s' (ID: %u, priority: %u, stack: %p)\n",
            name, (unsigned int)task->task_id, (unsigned int)priority, task->kernel_stack);

    return task;
}

/**
 * Destroy a task
 */
void task_destroy(task_t* task) {
    if (!task) {
        return;
    }

    kprintf("[TASK] Destroying task '%s' (ID: %u)\n", task->name, (unsigned int)task->task_id);

    // Free kernel stack
    if (task->kernel_stack) {
        pmm_free_page((phys_addr_t)task->kernel_stack);
    }

    // Free task struct
    pmm_free_page((phys_addr_t)task);
}

/**
 * Exit current task
 */
void task_exit(int exit_code) {
    task_t* current = task_current();

    kprintf("[TASK] Task '%s' (ID: %u) exiting with code %d\n",
            current->name, (unsigned int)current->task_id, exit_code);

    // Mark as zombie
    current->state = TASK_STATE_ZOMBIE;
    current->exit_code = exit_code;

    // Yield to scheduler (will never return)
    schedule();

    // Should never reach here
    while (1) {
        hal->cpu_halt();
    }
}

/**
 * Yield CPU to scheduler
 */
void task_yield(void) {
    schedule();
}
