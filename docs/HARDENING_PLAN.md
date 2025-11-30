# Kernel Hardening Plan

**Goal:** Build a verified, robust kernel that only breaks in extreme cases, never from normal task behavior.

**Principle:** Defense in depth - validate everything, trust nothing, fail safely.

---

## Core Principles

1. **Never crash from task misbehavior** - bad tasks should be killed, not crash the kernel
2. **Always validate inputs** - check all pointers, ranges, states
3. **Check all allocations** - never assume pmm_alloc_page() succeeds
4. **Maintain invariants** - document and verify critical invariants at runtime
5. **Fail safely** - panic with diagnostics rather than continue with corrupted state
6. **No silent corruption** - detect and report all errors

---

## Phase 1: Critical Infrastructure (IMMEDIATE)

### 1.1 Assertion Infrastructure

**File:** `include/kernel/assert.h` (already exists but needs enhancement)

Add runtime assertion macros:
```c
#define KASSERT(expr, msg) \
    do { if (!(expr)) kernel_panic("Assertion failed: " msg); } while(0)

#define KASSERT_EQ(a, b, msg) \
    do { if ((a) != (b)) kernel_panic("Assertion failed: " msg); } while(0)

#define KASSERT_NOT_NULL(ptr, msg) \
    do { if ((ptr) == NULL) kernel_panic("Null pointer: " msg); } while(0)

#define KASSERT_IN_RANGE(val, min, max, msg) \
    do { if ((val) < (min) || (val) > (max)) kernel_panic("Out of range: " msg); } while(0)
```

**Action:**
- [ ] Enhance assert.h with comprehensive macros
- [ ] Implement kernel_panic() with register dump
- [ ] Add stack unwinding for panic diagnostics

### 1.2 Enhanced Kernel Panic

**File:** `core/panic.c` (NEW)

Implement proper panic handler:
```c
void kernel_panic(const char* message) {
    // Disable interrupts
    hal->irq_disable();

    // Red screen
    vga->set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    vga->clear();

    // Print panic message
    kprintf("*** KERNEL PANIC ***\n\n");
    kprintf("%s\n\n", message);

    // Print CPU state
    kprintf("CPU State:\n");
    // TODO: Print EIP, ESP, registers

    // Print current task info
    task_t* current = task_current();
    if (current) {
        kprintf("Current task: %s (ID: %u, priority: %u)\n",
                current->name, current->task_id, current->priority);
    }

    // Halt forever
    while (1) hal->cpu_halt();
}
```

**Action:**
- [ ] Create core/panic.c
- [ ] Implement kernel_panic() with full diagnostics
- [ ] Add panic from interrupt context support

---

## Phase 2: Task Subsystem Hardening

### 2.1 Task Creation Validation

**File:** `core/task.c` - Harden `task_create_kernel_thread()`

**Current risks:**
- No validation of entry_point != NULL (checked but could add assert)
- No validation of name != NULL
- Silent failure if allocation fails
- No check if priority is valid
- Stack allocation failure only prints message

**Hardening:**
```c
task_t* task_create_kernel_thread(...) {
    // Validate ALL inputs
    KASSERT_NOT_NULL(entry_point, "task entry point is NULL");
    KASSERT_NOT_NULL(name, "task name is NULL");
    KASSERT_IN_RANGE(priority, 0, 255, "task priority out of range");
    KASSERT_EQ(stack_size, 4096, "stack_size must be 4096");

    // Allocate task struct
    task_t* task = (task_t*)pmm_alloc_page();
    if (!task) {
        kprintf("[TASK] FATAL: Failed to allocate task struct\n");
        return NULL;  // Caller must check
    }

    // Allocate stack
    task->kernel_stack = (void*)pmm_alloc_page();
    if (!task->kernel_stack) {
        kprintf("[TASK] FATAL: Failed to allocate stack\n");
        pmm_free_page((phys_addr_t)task);
        return NULL;  // Clean up before returning
    }

    // Add stack canary for overflow detection
    task->stack_canary = STACK_CANARY_VALUE;
    *(uint32_t*)(task->kernel_stack) = STACK_CANARY_VALUE;

    // Validate stack alignment
    KASSERT(((uintptr_t)task->kernel_stack & 0xFFF) == 0,
            "stack not page-aligned");

    // ... rest of setup ...

    // Final validation before returning
    KASSERT(task->context.esp >= (uint32_t)task->kernel_stack,
            "ESP below stack base");
    KASSERT(task->context.esp < (uint32_t)task->kernel_stack + stack_size,
            "ESP above stack top");

    return task;
}
```

**Action:**
- [ ] Add all input validation
- [ ] Add stack canary support
- [ ] Clean up allocations on failure
- [ ] Add final state validation

### 2.2 Stack Overflow Detection

**Add to task_t:**
```c
typedef struct task {
    // ... existing fields ...
    uint32_t stack_canary;  // Magic value to detect corruption
} task_t;

#define STACK_CANARY_VALUE 0xDEADBEEF
```

**Check on every task switch:**
```c
void validate_task_stack(task_t* task) {
    KASSERT_NOT_NULL(task, "task is NULL");

    // Check canary in task struct
    if (task->stack_canary != STACK_CANARY_VALUE) {
        kernel_panic("Task stack canary corrupted!");
    }

    // Check canary at bottom of stack
    uint32_t* stack_bottom = (uint32_t*)task->kernel_stack;
    if (*stack_bottom != STACK_CANARY_VALUE) {
        kernel_panic("Task stack overflow detected!");
    }
}
```

**Action:**
- [ ] Add stack_canary to task_t
- [ ] Initialize canaries in task creation
- [ ] Check canaries before context switch

### 2.3 Task Lifecycle Validation

**Add state transition checks:**
```c
void task_set_state(task_t* task, task_state_t new_state) {
    KASSERT_NOT_NULL(task, "task is NULL");

    // Validate state transitions
    task_state_t old_state = task->state;

    // Check valid transitions
    switch (old_state) {
        case TASK_STATE_READY:
            KASSERT(new_state == TASK_STATE_RUNNING ||
                    new_state == TASK_STATE_BLOCKED ||
                    new_state == TASK_STATE_ZOMBIE,
                    "Invalid READY transition");
            break;
        case TASK_STATE_RUNNING:
            // Can transition to any state
            break;
        case TASK_STATE_BLOCKED:
            KASSERT(new_state == TASK_STATE_READY ||
                    new_state == TASK_STATE_ZOMBIE,
                    "Invalid BLOCKED transition");
            break;
        case TASK_STATE_ZOMBIE:
            // Zombies can't transition
            KASSERT(false, "Zombie task cannot change state");
            break;
    }

    task->state = new_state;
}
```

**Action:**
- [ ] Add task_set_state() helper
- [ ] Use it everywhere instead of direct assignment
- [ ] Document valid state transitions

---

## Phase 3: Scheduler Hardening

### 3.1 Queue Consistency Checks

**Add to scheduler:**
```c
void scheduler_validate_queues(void) {
    for (int priority = 0; priority < 256; priority++) {
        task_queue_t* queue = &g_scheduler.ready[priority];

        // Count should match actual list length
        uint32_t actual_count = 0;
        task_t* task = queue->head;
        task_t* prev = NULL;

        while (task) {
            actual_count++;

            // Check back-pointer
            KASSERT(task->prev == prev, "Queue back-pointer corrupted");

            // Check priority matches
            KASSERT(task->priority == priority, "Task in wrong priority queue");

            // Check state is READY
            KASSERT(task->state == TASK_STATE_READY, "Non-READY task in queue");

            // Prevent infinite loop
            KASSERT(actual_count <= 1000, "Queue loop detected");

            prev = task;
            task = task->next;
        }

        // Count matches
        KASSERT(queue->count == actual_count, "Queue count mismatch");

        // Tail pointer correct
        KASSERT(queue->tail == prev, "Queue tail pointer wrong");

        // Bitmap matches
        bool has_tasks = (queue->count > 0);
        bool bit_set = /* check bitmap */;
        KASSERT(has_tasks == bit_set, "Bitmap doesn't match queue");
    }
}
```

**Action:**
- [ ] Implement scheduler_validate_queues()
- [ ] Call periodically in debug builds
- [ ] Add compile-time flag to enable/disable

### 3.2 Scheduler Pick Safety

**Harden scheduler_pick_next():**
```c
task_t* scheduler_pick_next(void) {
    uint8_t priority = find_highest_priority();
    task_queue_t* queue = &g_scheduler.ready[priority];
    task_t* next = queue->head;

    // NEVER return NULL - idle task is always fallback
    if (!next) {
        // This should never happen, but be defensive
        kprintf("[SCHED] WARNING: No task at priority %u, using idle\n", priority);
        next = task_get_idle();
        KASSERT_NOT_NULL(next, "Idle task is NULL!");
    }

    // Validate task before returning
    KASSERT(next->state == TASK_STATE_READY, "Picked non-READY task");
    KASSERT_NOT_NULL(next->kernel_stack, "Task has no stack");
    KASSERT(next->context.esp != 0, "Task ESP is zero");

    return next;
}
```

**Action:**
- [ ] Add fallback to idle task
- [ ] Validate task state before returning
- [ ] Never return NULL

### 3.3 Enqueue/Dequeue Safety

**Prevent double-enqueue:**
```c
void scheduler_enqueue(task_t* task) {
    KASSERT_NOT_NULL(task, "enqueue NULL task");
    KASSERT(task->state == TASK_STATE_READY, "enqueue non-READY task");

    // Prevent double-enqueue
    KASSERT(task->next == NULL && task->prev == NULL,
            "Task already in a queue");

    // ... rest of enqueue ...
}

void scheduler_dequeue(task_t* task) {
    KASSERT_NOT_NULL(task, "dequeue NULL task");

    uint8_t priority = task->priority;
    task_queue_t* queue = &g_scheduler.ready[priority];

    // Only dequeue if actually in a queue
    if (task->next == NULL && task->prev == NULL &&
        queue->head != task) {
        // Not in queue, skip
        return;
    }

    // ... rest of dequeue ...
}
```

**Action:**
- [ ] Add double-enqueue protection
- [ ] Skip dequeue if not in queue
- [ ] Add assertions for all invariants

---

## Phase 4: Context Switch Hardening

### 4.1 Validate Context Before Switch

**File:** `core/scheduler.c` - Before calling `context_switch()`

```c
void schedule(void) {
    uint32_t flags = hal->irq_disable();

    task_t* current = g_scheduler.current_task;
    task_t* next = scheduler_pick_next();

    // Validate current task
    KASSERT_NOT_NULL(current, "current task is NULL");
    validate_task_stack(current);

    // Validate next task
    KASSERT_NOT_NULL(next, "next task is NULL");
    validate_task_stack(next);

    // Validate ESP is in valid range
    KASSERT(next->context.esp >= (uint32_t)next->kernel_stack,
            "next ESP below stack");
    KASSERT(next->context.esp < (uint32_t)next->kernel_stack + next->kernel_stack_size,
            "next ESP above stack");

    // Validate EIP is not NULL
    KASSERT(next->context.eip != 0, "next EIP is zero");

    // ... rest of schedule ...
}
```

**Action:**
- [ ] Add pre-switch validation
- [ ] Check stack canaries
- [ ] Validate ESP and EIP ranges

### 4.2 Post-Switch Validation

**Add after context_switch returns:**
```c
    // Context switch happened
    context_switch(&current->context, &next->context);

    // When we return, we're back in this task
    // Validate we're sane
    task_t* now_current = g_scheduler.current_task;
    KASSERT_NOT_NULL(now_current, "current task NULL after switch");
    validate_task_stack(now_current);

    hal->irq_restore(flags);
}
```

**Action:**
- [ ] Add post-switch validation
- [ ] Check we returned to correct task

---

## Phase 5: Memory Allocation Safety

### 5.1 Never Assume Allocation Succeeds

**Pattern to follow everywhere:**
```c
void* ptr = pmm_alloc_page();
if (!ptr) {
    kprintf("[SUBSYSTEM] FATAL: Out of memory\n");
    // Either:
    // 1. Return error to caller
    // 2. Try to free some memory
    // 3. Panic if critical
    kernel_panic("Critical allocation failed");
}
```

**Action:**
- [ ] Audit ALL pmm_alloc_page() calls
- [ ] Add NULL checks everywhere
- [ ] Define policy: panic vs return error

### 5.2 Validate Freed Memory

```c
void pmm_free_page(phys_addr_t addr) {
    // Validate address is page-aligned
    KASSERT((addr & 0xFFF) == 0, "Freeing unaligned address");

    // Validate address is in valid RAM range
    KASSERT(addr >= pmm_start && addr < pmm_end,
            "Freeing address outside RAM");

    // Check not freeing kernel or reserved region
    KASSERT(!is_reserved_region(addr), "Freeing reserved memory");

    // ... rest of free ...
}
```

**Action:**
- [ ] Add validation to pmm_free_page()
- [ ] Prevent double-free
- [ ] Check address ranges

---

## Phase 6: Interrupt Handler Safety

### 6.1 Timer Interrupt Hardening

**File:** `arch/x86/timer.c`

```c
void timer_interrupt_handler(void) {
    // Validate we have a current CPU structure
    struct per_cpu_data* cpu = this_cpu();
    KASSERT_NOT_NULL(cpu, "per-CPU data is NULL in timer IRQ");

    cpu->ticks++;

    // Validate scheduler is initialized
    if (g_scheduler.current_task == NULL) {
        // Scheduler not ready yet, just send EOI
        hal->io_outb(0x20, 0x20);
        return;
    }

    // Call scheduler tick
    scheduler_tick();

    // Send EOI
    hal->io_outb(0x20, 0x20);
}
```

**Action:**
- [ ] Add NULL checks for per-CPU
- [ ] Handle pre-scheduler interrupts gracefully
- [ ] Validate all IRQ handlers

---

## Testing Strategy

### Unit Tests
- [ ] Test task creation with invalid inputs
- [ ] Test scheduler with corrupted queues
- [ ] Test double-enqueue scenarios
- [ ] Test allocation failures

### Integration Tests
- [ ] Run with low memory (stress allocator)
- [ ] Create many tasks (test limits)
- [ ] Intentionally corrupt stack (detect overflow)
- [ ] Invalid system calls (Phase 4)

### Fuzzing (Future)
- [ ] Fuzz task creation parameters
- [ ] Fuzz scheduler operations
- [ ] Fuzz memory allocations

---

## Documentation Requirements

Every critical function must document:
- **Preconditions:** What must be true on entry
- **Postconditions:** What is guaranteed on exit
- **Invariants:** What is always true
- **Error handling:** What happens on failure

Example:
```c
/**
 * Create a kernel thread
 *
 * PRECONDITIONS:
 * - entry_point must be non-NULL
 * - name must be non-NULL
 * - priority must be 0-255
 * - stack_size must be 4096
 *
 * POSTCONDITIONS (on success):
 * - Returns non-NULL task pointer
 * - Task is READY state
 * - Task has valid stack with canaries
 * - Task is NOT enqueued (caller must enqueue)
 *
 * ERROR HANDLING:
 * - Returns NULL if allocation fails
 * - Cleans up partial allocations
 * - Logs error message
 */
task_t* task_create_kernel_thread(...);
```

---

## Implementation Priority

**Week 1: Critical Infrastructure**
1. Enhanced assert.h
2. kernel_panic() implementation
3. Stack canaries
4. Task creation validation

**Week 2: Scheduler Hardening**
5. Queue consistency checks
6. Enqueue/dequeue safety
7. Scheduler pick safety

**Week 3: Context & Memory**
8. Context switch validation
9. Memory allocation checks
10. Interrupt handler safety

**Week 4: Testing & Documentation**
11. Unit tests for all failure modes
12. Integration tests
13. Document all invariants

---

## Success Criteria

The kernel is "robust" when:
- [ ] No task can crash the kernel (only itself)
- [ ] All allocations are checked
- [ ] All invariants are validated
- [ ] Panic gives useful diagnostics
- [ ] Unit tests cover failure modes
- [ ] Documentation specifies contracts
- [ ] Code review finds no unchecked pointers
