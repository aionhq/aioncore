/**
 * O(1) Real-Time Scheduler
 *
 * Priority-based scheduler with 256 priority levels.
 * Uses bitmap for O(1) highest-priority task selection.
 *
 * RT Constraints:
 * - Pick next task: O(1), < 100 cycles
 * - Enqueue/dequeue: O(1), < 50 cycles
 * - Context switch: < 200 cycles total
 */

#include <kernel/scheduler.h>
#include <kernel/task.h>
#include <kernel/hal.h>
#include <drivers/vga.h>
#include <lib/string.h>

// Global scheduler state
// Future: one per CPU for SMP
scheduler_t g_scheduler;

// Bootstrap task representing the code running before scheduler takes over.
// It is never enqueued; we treat it as already "dead" so it won't be rescheduled.
static task_t bootstrap_task;

// Forward declaration for context switch (in arch/x86/context.s)
extern void context_switch(cpu_context_t* old_ctx, cpu_context_t* new_ctx);

/**
 * Set priority bit in bitmap
 *
 * RT: O(1), < 10 cycles
 */
static inline void set_priority_bit(uint8_t priority) {
    uint32_t word_idx = priority / 32;
    uint32_t bit_idx = priority % 32;
    g_scheduler.priority_bitmap[word_idx] |= (1u << bit_idx);
}

/**
 * Clear priority bit in bitmap
 *
 * RT: O(1), < 10 cycles
 */
static inline void clear_priority_bit(uint8_t priority) {
    uint32_t word_idx = priority / 32;
    uint32_t bit_idx = priority % 32;
    g_scheduler.priority_bitmap[word_idx] &= ~(1u << bit_idx);
}

/**
 * Find highest priority with ready tasks
 *
 * Uses __builtin_clz (count leading zeros) for O(1) search.
 *
 * @return  Highest priority (0-255), or 0 if no tasks ready
 *
 * RT: O(1), < 50 cycles
 */
static uint8_t find_highest_priority(void) {
    // Search from highest to lowest (index 7 down to 0)
    for (int i = 7; i >= 0; i--) {
        if (g_scheduler.priority_bitmap[i]) {
            // Found a non-zero word, find highest bit
            // __builtin_clz counts leading zeros from MSB
            int bit = 31 - __builtin_clz(g_scheduler.priority_bitmap[i]);
            return (uint8_t)(i * 32 + bit);
        }
    }
    return SCHED_IDLE_PRIORITY;  // No tasks ready
}

/**
 * Initialize scheduler
 */
void scheduler_init(void) {
    kprintf("[SCHED] Initializing O(1) scheduler...\n");

    // Zero out scheduler state
    memset(&g_scheduler, 0, sizeof(g_scheduler));
    memset(&bootstrap_task, 0, sizeof(bootstrap_task));

    strlcpy(bootstrap_task.name, "bootstrap", sizeof(bootstrap_task.name));
    bootstrap_task.task_id = 0xFFFFFFFF;  // Sentinel ID
    bootstrap_task.state = TASK_STATE_ZOMBIE;  // Never reschedule this context
    bootstrap_task.priority = SCHED_IDLE_PRIORITY;
    bootstrap_task.address_space = mmu_get_kernel_address_space();
    g_scheduler.current_task = &bootstrap_task;

    // Get idle task from task subsystem
    task_t* idle = task_get_idle();
    if (!idle) {
        kprintf("[SCHED] FATAL: No idle task\n");
        return;
    }

    // Enqueue idle task as READY so it appears in the bitmap/queues
    idle->state = TASK_STATE_READY;
    scheduler_enqueue(idle);

    kprintf("[SCHED] Scheduler initialized (idle task: %s)\n", idle->name);
}

/**
 * Enqueue a task in the ready queue
 *
 * RT: O(1), < 50 cycles
 */
void scheduler_enqueue(task_t* task) {
    if (!task || task->state != TASK_STATE_READY) {
        return;
    }

    uint8_t priority = task->priority;
    task_queue_t* queue = &g_scheduler.ready[priority];

    // Add to end of queue (circular doubly-linked list)
    if (!queue->head) {
        // Empty queue
        queue->head = task;
        queue->tail = task;
        task->next = NULL;
        task->prev = NULL;
    } else {
        // Append to tail
        task->prev = queue->tail;
        task->next = NULL;
        queue->tail->next = task;
        queue->tail = task;
    }

    queue->count++;

    // Set priority bit in bitmap
    set_priority_bit(priority);
}

/**
 * Dequeue a task from the ready queue
 *
 * RT: O(1), < 50 cycles
 */
void scheduler_dequeue(task_t* task) {
    if (!task) {
        return;
    }

    uint8_t priority = task->priority;
    task_queue_t* queue = &g_scheduler.ready[priority];

    // Fast exit if queue empty or task clearly not linked here
    if (queue->count == 0) {
        return;  // Not enqueued
    }
    if (queue->head != task && queue->tail != task &&
        task->prev == NULL && task->next == NULL) {
        return;  // Task not in this queue
    }

    // Remove from queue
    if (task->prev) {
        task->prev->next = task->next;
    } else {
        // Removing head
        queue->head = task->next;
    }

    if (task->next) {
        task->next->prev = task->prev;
    } else {
        // Removing tail
        queue->tail = task->prev;
    }

    task->next = NULL;
    task->prev = NULL;
    queue->count--;

    // Clear priority bit if queue is now empty
    if (queue->count == 0) {
        clear_priority_bit(priority);
    }
}

/**
 * Pick next task to run
 *
 * Selects highest-priority READY task.
 * Never returns NULL (falls back to idle task).
 *
 * RT: O(1), < 100 cycles
 */
task_t* scheduler_pick_next(void) {
    // Find highest priority with ready tasks
    uint8_t priority = find_highest_priority();

    // Get head of that priority's queue
    task_queue_t* queue = &g_scheduler.ready[priority];
    task_t* next = queue->head;

    if (!next) {
        // Should never happen (idle task should always be ready)
        kprintf("[SCHED] WARNING: No tasks in priority %u queue!\n", priority);
        return task_get_idle();
    }

    return next;
}

/**
 * Schedule (main scheduler entry point)
 *
 * Saves current context, picks next task, switches to it.
 *
 * RT: < 200 cycles total (pick + context switch)
 */
void schedule(void) {
    // Disable interrupts during scheduling
    uint32_t flags = hal->irq_disable();

    task_t* current = g_scheduler.current_task;
    task_t* next = scheduler_pick_next();

    // If same task, nothing to do
    if (current == next) {
        g_scheduler.need_resched = false;
        hal->irq_restore(flags);
        return;
    }

    // Handle current task state
    if (current->state == TASK_STATE_RUNNING) {
        // Task is still runnable, move to READY
        current->state = TASK_STATE_READY;
    } else if (current->state == TASK_STATE_ZOMBIE) {
        // Task exited, remove from run queue
        scheduler_dequeue(current);
        // TODO: Add to zombie list for cleanup
    }

    // Dequeue next task and mark as RUNNING
    scheduler_dequeue(next);
    next->state = TASK_STATE_RUNNING;

    // Re-enqueue current task if still READY
    if (current->state == TASK_STATE_READY) {
        scheduler_enqueue(current);
    }

    // Update scheduler state
    g_scheduler.current_task = next;
    g_scheduler.context_switches++;
    g_scheduler.need_resched = false;

    // Context switch
    context_switch(&current->context, &next->context);

    // When we return here, we've been scheduled back in
    hal->irq_restore(flags);
}

/**
 * Timer tick callback
 *
 * Called from timer interrupt.
 * Updates accounting and sets need_resched for preemption.
 *
 * RT: < 100 cycles
 */
bool scheduler_tick(void) {
    g_scheduler.ticks++;

    task_t* current = g_scheduler.current_task;
    if (current) {
        current->cpu_time_ticks++;
    }

    // Simple round-robin within same priority:
    // If there are other tasks at current priority, set need_resched
    uint8_t priority = current->priority;
    if (g_scheduler.ready[priority].count > 0) {
        g_scheduler.need_resched = true;
        return true;
    }

    return false;
}
