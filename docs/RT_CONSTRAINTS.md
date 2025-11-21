# Real-Time Constraints

This document defines the hard real-time constraints for the kernel. All code in the kernel must satisfy these constraints to provide deterministic behavior.

## Core Principles

1. **Bounded Execution Time:** Every kernel operation must complete in bounded time
2. **No Dynamic Allocation in RT Paths:** Pre-allocate all data structures
3. **O(1) Operations:** Critical paths must be O(1), not O(n)
4. **Priority Inversion Prevention:** Use priority inheritance/ceiling protocols
5. **Interrupt Latency:** Bounded and measurable interrupt response time

## Timing Budgets

### Interrupt Handling
- **IRQ dispatch:** < 100 cycles (< 50ns @ 2GHz)
- **Exception handler:** < 100 cycles
- **Timer interrupt:** < 200 cycles
- **Total interrupt latency:** < 10µs worst-case

### Scheduling
- **Context switch:** < 200 cycles (< 1µs @ 2GHz)
- **Pick next task:** O(1), < 100 cycles
- **Priority change:** O(1), < 50 cycles
- **Task enqueue/dequeue:** O(1), < 50 cycles

### IPC
- **Send message (small):** < 500ns
- **Receive message:** < 500ns
- **Synchronous call (call+reply):** < 2µs
- **Capability check:** O(1), < 20 cycles

### Memory Management
- **Page allocation:** O(1), < 200 cycles
- **Page free:** O(1), < 100 cycles
- **TLB flush (single page):** Hardware-dependent, < 50 cycles
- **Address space switch:** < 100 cycles (CR3 reload)

## Data Structure Requirements

### Scheduler

```c
// ✅ GOOD: O(1) priority queue using bitmap
struct scheduler {
    struct task_queue ready[256];  // One queue per priority
    uint32_t priority_bitmap[8];   // Bitmap for O(1) search
};

// Find highest priority with ready tasks: O(1)
static inline uint8_t find_highest_priority(uint32_t bitmap[8]) {
    for (int i = 7; i >= 0; i--) {
        if (bitmap[i]) {
            return i * 32 + __builtin_clz(bitmap[i]);
        }
    }
    return 0;  // Idle priority
}

// ❌ BAD: O(n) search through all tasks
void schedule(void) {
    struct task* highest = NULL;
    for (struct task* t = all_tasks; t; t = t->next) {  // O(n) - NOT ALLOWED!
        if (!highest || t->priority > highest->priority) {
            highest = t;
        }
    }
}
```

### IPC Message Queues

```c
// ✅ GOOD: Bounded queue with fixed size
#define IPC_QUEUE_DEPTH 16

struct ipc_port {
    struct ipc_message queue[IPC_QUEUE_DEPTH];  // Pre-allocated
    uint32_t head;
    uint32_t tail;
    uint32_t count;
};

// Enqueue: O(1), fails if full (deterministic)
int ipc_enqueue(struct ipc_port* port, struct ipc_message* msg) {
    if (port->count >= IPC_QUEUE_DEPTH) {
        return -EWOULDBLOCK;  // Deterministic failure
    }
    port->queue[port->tail] = *msg;
    port->tail = (port->tail + 1) % IPC_QUEUE_DEPTH;
    port->count++;
    return 0;
}

// ❌ BAD: Dynamic allocation
int ipc_enqueue(struct ipc_port* port, struct ipc_message* msg) {
    struct ipc_node* node = kmalloc(sizeof(*node));  // FORBIDDEN in RT path!
    if (!node) return -ENOMEM;
    // ... heap allocation is unbounded time
}
```

### Memory Allocator

```c
// ✅ GOOD: Per-CPU free lists, O(1) allocation
struct per_cpu_allocator {
    phys_addr_t free_pages[256];  // Pre-populated free list
    uint32_t count;
};

phys_addr_t pmm_alloc_page_percpu(void) {
    struct per_cpu_allocator* alloc = &this_cpu()->page_allocator;
    if (alloc->count == 0) {
        // Refill from global pool (rare, may block)
        return pmm_alloc_page_slow();
    }
    return alloc->free_pages[--alloc->count];  // O(1)
}

// ❌ BAD: Search for free pages
phys_addr_t pmm_alloc_page(void) {
    for (size_t i = 0; i < total_pages; i++) {  // O(n) - NOT ALLOWED!
        if (page_is_free(i)) {
            return i * PAGE_SIZE;
        }
    }
    return 0;
}
```

## Forbidden Operations in RT Paths

### Never Do These in Interrupt/Syscall/IPC Handlers:

1. **Dynamic Memory Allocation**
   ```c
   void* ptr = kmalloc(size);  // ❌ Unbounded time
   ```

2. **Unbounded Loops**
   ```c
   while (condition) { /* ... */ }  // ❌ May never terminate
   for (;;) { /* ... */ }           // ❌ Infinite loop
   ```

3. **Global List Traversal**
   ```c
   for (task = all_tasks; task; task = task->next) { /* ... */ }  // ❌ O(n)
   ```

4. **Sleeping/Blocking**
   ```c
   sleep(1000);  // ❌ Never sleep in kernel
   wait_for_completion(&completion);  // ❌ Unbounded wait
   ```

5. **I/O Operations**
   ```c
   disk_read(sector, buffer);  // ❌ Unbounded I/O time
   ```

6. **String Operations on User Data**
   ```c
   strlen(user_string);  // ❌ May be unbounded
   strcpy(dest, user_src);  // ❌ Use strlcpy with max length
   ```

## Priority Inheritance

### Problem: Priority Inversion

```
High-priority task H blocks waiting for resource held by low-priority task L.
Medium-priority task M preempts L.
Result: H waits for M, even though H > M > L in priority!
```

### Solution: Priority Inheritance Protocol

```c
void mutex_lock(struct mutex* lock) {
    struct task* current = this_cpu()->current_task;
    struct task* owner = lock->owner;

    if (owner && current->priority > owner->priority) {
        // Boost owner to current's priority
        owner->effective_priority = current->priority;
        scheduler_requeue(owner);  // Move to higher priority queue
    }

    // ... acquire lock ...
}

void mutex_unlock(struct mutex* lock) {
    struct task* current = this_cpu()->current_task;

    // Restore original priority
    current->effective_priority = current->base_priority;
    scheduler_requeue(current);

    // ... release lock ...
}
```

### IPC Priority Inheritance

```c
int ipc_send(cap_t port_cap, struct ipc_message* msg) {
    struct task* receiver = port->owner;
    struct task* sender = this_cpu()->current_task;

    if (sender->priority > receiver->priority) {
        // Boost receiver to sender's priority
        receiver->effective_priority = sender->priority;
        scheduler_wake(receiver);  // Will preempt lower-priority tasks
    }

    // ... send message ...
}
```

## Interrupt Latency Guarantees

### Sources of Interrupt Latency

1. **Interrupt disabled time:** Time with CLI
2. **Current interrupt handler:** If another IRQ is being handled
3. **Exception handling:** Page faults, etc.
4. **Context switch overhead:** Saving/restoring state

### Minimizing Latency

```c
// ✅ GOOD: Minimal interrupt handler
void timer_interrupt_handler(void) {
    this_cpu()->ticks++;           // Fast update
    scheduler_preempt_current();    // Set need_resched flag
    irq_eoi(TIMER_IRQ);            // End-of-interrupt
}
// Then actual scheduling happens at safe point

// ❌ BAD: Doing work in interrupt context
void timer_interrupt_handler(void) {
    // Process expired timers - this could be O(n)!
    for (timer = active_timers; timer; timer = timer->next) {
        if (timer->expires <= now) {
            timer->callback();  // May do unbounded work!
        }
    }
}
```

### Critical Sections

```c
// Keep IRQ-disabled sections SHORT
void critical_operation(void) {
    uint32_t flags = irq_disable();

    // Fast, bounded work only
    // < 100 cycles total
    this_cpu()->counter++;

    irq_restore(flags);
}

// ❌ BAD: Long critical section
void bad_critical_operation(void) {
    uint32_t flags = irq_disable();

    // Slow, unbounded work
    for (int i = 0; i < 1000000; i++) {  // Blocks interrupts for too long!
        do_work();
    }

    irq_restore(flags);
}
```

## Testing RT Constraints

### Worst-Case Execution Time (WCET) Testing

```c
// Measure worst-case latency
uint64_t measure_context_switch_wcet(void) {
    uint64_t worst = 0;

    for (int i = 0; i < 10000; i++) {
        uint64_t start = timer_read_tsc();
        context_switch(&task1, &task2);
        uint64_t end = timer_read_tsc();

        uint64_t cycles = end - start;
        if (cycles > worst) {
            worst = cycles;
        }
    }

    return worst;
}
```

### Latency Tracing

```c
// Trace all interrupt latencies
void irq_entry(uint8_t irq) {
    uint64_t now = timer_read_tsc();
    uint64_t latency = now - this_cpu()->last_irq_time;

    if (latency > MAX_IRQ_LATENCY_CYCLES) {
        trace_event(TRACE_IRQ_LATENCY_VIOLATION, irq, latency, 0, 0);
    }

    this_cpu()->last_irq_time = now;
}
```

## Verification Checklist

For each kernel function, verify:

- [ ] No unbounded loops (for, while, recursion)
- [ ] No dynamic memory allocation (kmalloc, kzalloc)
- [ ] No blocking operations (sleep, wait)
- [ ] No O(n) data structure operations
- [ ] IRQ-disabled time < 1µs
- [ ] Clear worst-case execution time
- [ ] Priority inheritance if holding resources
- [ ] All error paths are bounded

## Per-CPU Design for Lock-Free RT

```c
// ✅ GOOD: Per-CPU data, no locking needed
void per_cpu_increment(void) {
    this_cpu()->counter++;  // No lock needed, CPU-local
}

// ✅ GOOD: Atomic operations for shared state
void shared_increment(atomic_t* counter) {
    atomic_inc(counter);  // Lock-free, bounded time
}

// ❌ BAD: Global lock (may cause priority inversion)
void locked_increment(void) {
    spin_lock(&global_lock);  // May wait for lower-priority task!
    global_counter++;
    spin_unlock(&global_lock);
}
```

## Summary of Constraints

| Operation | Time Bound | Notes |
|-----------|------------|-------|
| Context switch | < 1µs | Hardware-dependent |
| Scheduler pick | O(1), < 100 cycles | Bitmap-based |
| IPC send/recv | < 500ns | Small messages only |
| Page alloc | O(1), < 200 cycles | Per-CPU free lists |
| Capability check | O(1), < 20 cycles | Array lookup |
| IRQ dispatch | < 100 cycles | Minimal handler |
| Priority change | O(1), < 50 cycles | Bitmap update |
| Interrupt latency | < 10µs | System-wide guarantee |

## References

- Real-Time Systems by Jane W. S. Liu
- seL4 whitepaper: https://sel4.systems/About/Performance/
- OSEK/VDX real-time OS specification
- Priority Inheritance Protocols: An Approach to Real-Time Synchronization
