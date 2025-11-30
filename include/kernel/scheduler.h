#ifndef KERNEL_SCHEDULER_H
#define KERNEL_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include <kernel/task.h>

/**
 * Real-Time Scheduler
 *
 * O(1) priority-based scheduler with 256 priority levels.
 * Uses bitmap for O(1) highest-priority lookup.
 *
 * RT Constraints (from RT_CONSTRAINTS.md):
 * - Pick next task: O(1), < 100 cycles
 * - Enqueue/dequeue: O(1), < 50 cycles
 * - Priority change: O(1), < 50 cycles
 *
 * Design (based on RT_CONSTRAINTS.md section 3.1):
 * - 256 run queues (one per priority level 0-255)
 * - 8 x 32-bit bitmap for O(1) priority search
 * - Per-CPU scheduler state (for future SMP)
 */

#define SCHED_NUM_PRIORITIES 256    // Priority levels: 0 (lowest) to 255 (highest)
#define SCHED_IDLE_PRIORITY  0      // Idle task priority
#define SCHED_DEFAULT_PRIORITY 128  // Default priority for new tasks

/**
 * Per-priority run queue
 *
 * Simple circular doubly-linked list of tasks at this priority.
 */
typedef struct {
    task_t*  head;      // Head of queue
    task_t*  tail;      // Tail of queue (for efficient append)
    uint32_t count;     // Number of tasks in queue
} task_queue_t;

/**
 * Scheduler state (per-CPU in future)
 *
 * For now: single global scheduler
 * Future: one per CPU for SMP
 */
typedef struct {
    // Priority queues
    task_queue_t ready[SCHED_NUM_PRIORITIES];   // One queue per priority

    // Bitmap for O(1) priority search
    // priority_bitmap[i] has bit j set if ready[i*32 + j] is non-empty
    uint32_t priority_bitmap[8];                // 8 * 32 = 256 bits

    // Current task
    task_t* current_task;                       // Currently running task

    // Statistics
    uint64_t context_switches;                  // Total context switches
    uint64_t ticks;                             // Scheduler ticks

    // Preemption flag
    bool need_resched;                          // Set by timer to request reschedule
} scheduler_t;

/**
 * Initialize scheduler
 *
 * Must be called once during kernel initialization, after task_init().
 * Creates the idle task.
 */
void scheduler_init(void);

/**
 * Enqueue a task in the ready queue
 *
 * Adds task to the appropriate priority queue and updates bitmap.
 * Task must be in READY state.
 *
 * @param task  Task to enqueue
 *
 * RT: O(1), < 50 cycles
 */
void scheduler_enqueue(task_t* task);

/**
 * Dequeue a task from the ready queue
 *
 * Removes task from its priority queue and updates bitmap.
 *
 * @param task  Task to dequeue
 *
 * RT: O(1), < 50 cycles
 */
void scheduler_dequeue(task_t* task);

/**
 * Pick next task to run
 *
 * Selects highest-priority READY task using bitmap.
 * If no tasks are ready, returns idle task.
 *
 * @return  Next task to run (never NULL)
 *
 * RT: O(1), < 100 cycles
 */
task_t* scheduler_pick_next(void);

/**
 * Schedule (invoke scheduler)
 *
 * Saves current task context, picks next task, and switches to it.
 * This is the main scheduler entry point.
 *
 * Called from:
 * - task_yield() - voluntary yield
 * - Interrupt handlers after setting need_resched flag
 *
 * RT: < 200 cycles total (pick + context switch)
 */
void schedule(void);

/**
 * Timer tick callback
 *
 * Called from timer interrupt handler.
 * Updates accounting and sets need_resched if preemption needed.
 *
 * @return  true if reschedule is needed, false otherwise
 *
 * RT: < 100 cycles
 */
bool scheduler_tick(void);

/**
 * Get current task
 *
 * Returns the currently running task.
 *
 * @return  Current task (never NULL)
 *
 * RT: O(1), < 10 cycles
 */
static inline task_t* scheduler_current(void) {
    extern scheduler_t g_scheduler;
    return g_scheduler.current_task;
}

/**
 * Request reschedule
 *
 * Sets the need_resched flag.
 * Actual rescheduling happens at next safe point.
 *
 * RT: O(1), < 5 cycles
 */
static inline void scheduler_set_need_resched(void) {
    extern scheduler_t g_scheduler;
    g_scheduler.need_resched = true;
}

/**
 * Check if reschedule is needed
 *
 * @return  true if need_resched flag is set
 *
 * RT: O(1), < 5 cycles
 */
bool scheduler_need_resched(void);

#endif // KERNEL_SCHEDULER_H
