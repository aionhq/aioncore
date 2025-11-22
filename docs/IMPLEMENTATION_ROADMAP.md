# Implementation Roadmap - RT Microkernel

Building a real-time microkernel with aggressive userspace offloading.

## Philosophy

**Microkernel First:** Minimal kernel (address spaces, threads, IPC, capabilities) with all services in userspace.

**Real-Time Constraints:**
- O(1) operations in scheduler and IPC
- No allocations in RT-critical paths
- Bounded interrupt latency
- Priority inheritance throughout
- Pre-allocated kernel objects

**Verification:** Small trusted computing base (<10K LOC) for potential formal verification.

## Current State
- ✅ Basic boot (GRUB multiboot)
- ✅ HAL abstraction layer (x86)
- ✅ Per-CPU infrastructure
- ✅ Modular VGA driver
- ✅ Safe string library
- ✅ Cross-compilation toolchain

## Phase 1: Foundation & HAL (Week 1-2) ✅ DONE

### Completed:
- ✅ HAL interface (include/kernel/hal.h)
- ✅ Per-CPU infrastructure (core/percpu.c, include/kernel/percpu.h)
- ✅ Modular VGA driver (drivers/vga/)
- ✅ Safe string library (lib/string.c)
- ✅ x86 HAL implementation (arch/x86/hal.c)
- ✅ IDT and interrupt handling (arch/x86/idt.c, arch/x86/idt_asm.s)
- ✅ HAL/IDT integration (HAL is single abstraction layer)
- ✅ Interrupt frame layout (explicit register save/restore)
- ✅ Kernel initialization (core/init.c)

**Milestone:** Boots, prints to screen, HAL abstraction working, interrupts ready

**Critical Fixes (2025-11-21):**
- Fixed interrupt frame struct to match actual assembly push order
- Integrated IDT into HAL (hal->irq_register now works)
- HAL cpu_init calls idt_init automatically

## Phase 2: Basic Memory & Interrupts (Week 3-4) 🔨 IN PROGRESS

**Goal:** Minimal kernel infrastructure to support tasks and IPC

### 2.1 Physical Memory Manager (Simple Bitmap)
```c
// mm/pmm.c
void pmm_init(void);
phys_addr_t pmm_alloc_page(void);        // O(1) with bitmap + free list
void pmm_free_page(phys_addr_t page);
void pmm_reserve_region(phys_addr_t start, size_t size);

// RT constraint: Must be O(1), use per-CPU free lists
// Design note: Track frames as first-class objects so later we can
// account "frames per unit" for memory budgets and limits.
```

### 2.2 Basic Paging
```c
// arch/x86/mmu.c
struct page_table;

struct page_table* mmu_create_address_space(void);
void mmu_destroy_address_space(struct page_table* pt);
void* mmu_map_page(struct page_table* pt, phys_addr_t phys,
                   virt_addr_t virt, uint32_t flags);
void mmu_unmap_page(struct page_table* pt, virt_addr_t virt);

// Flags
#define MMU_PRESENT   (1 << 0)
#define MMU_WRITABLE  (1 << 1)
#define MMU_USER      (1 << 2)
```

**Design note:** The `struct page_table*` here will effectively be each
unit's `addr_space_t` in the unit ABI, so this API should remain
stable and independent of POSIX or any specific userspace model.

### 2.3 IDT Setup
```c
// arch/x86/idt.c
void idt_init(void);
void idt_set_gate(uint8_t vector, void (*handler)(void), uint8_t flags);

// Exception handlers (tiny! just save state and dispatch)
void exception_handler_0(void);  // Divide by zero
void exception_handler_6(void);  // Invalid opcode
void exception_handler_13(void); // General protection fault
void exception_handler_14(void); // Page fault (the important one!)

// RT constraint: Exception handlers must be <100 cycles
```

### 2.4 Timer Support
```c
// arch/x86/timer.c
void timer_init(uint32_t frequency_hz);
uint64_t timer_read_us(void);  // Microsecond precision

void timer_interrupt_handler(void) {
    struct per_cpu_data* cpu = this_cpu();
    cpu->ticks++;

    // Process per-CPU timers (sleep queues, deadlines)
    timers_tick();

    // Trigger scheduler preemption
    scheduler_tick();
}

// RT constraint: Timer must be calibrated, no drift
```

**Milestone:** Can handle exceptions, timer ticks, map memory for future user tasks

## Phase 3: Tasks, Scheduling & Syscalls (Week 5-6)

**Goal:** Get to ring3 as fast as possible

### 3.1 Task Structure
```c
// core/task.h
struct task {
    uint32_t tid;
    enum task_state state;  // READY, RUNNING, BLOCKED

    // CPU state
    struct cpu_context context;  // Saved registers

    // Memory
    struct page_table* address_space;

    // RT Scheduling
    uint8_t priority;        // 0-255, higher = more important
    uint64_t time_slice_us;  // Microseconds per slice
    uint64_t deadline;       // For EDF scheduling (optional)

    // No capabilities yet, that's Phase 4

    struct list_head run_list;  // On per-CPU run queue
};
```

### 3.2 Fixed-Priority Scheduler (RT)
```c
// core/scheduler.c
#define NUM_PRIORITIES 256

struct scheduler {
    struct task_queue ready[NUM_PRIORITIES];  // One queue per priority
    uint32_t priority_bitmap[8];  // Bitmap for O(1) priority search
    struct task* idle_task;
};

void scheduler_init(void);
struct task* scheduler_pick_next(void);  // O(1) using bitmap
void scheduler_enqueue(struct task* task);
void scheduler_yield(void);

// RT constraint: O(1) operations only, no loops over all tasks
```

### 3.3 Context Switching
```c
// arch/x86/context.S
.global context_switch
context_switch:
    # Save current context (all registers)
    # Switch page tables
    # Load new context
    ret

// RT constraint: Must be <200 cycles on modern x86
```

### 3.4 Syscall Entry/Exit
```c
// arch/x86/syscall.S
.global syscall_entry
syscall_entry:
    # Switch to kernel stack
    # Save user context
    # Call C handler
    # Restore user context
    # Return to ring3 (iret)

// Syscall dispatch
int64_t syscall_handler(uint32_t syscall_num, uint32_t arg1,
                        uint32_t arg2, uint32_t arg3, uint32_t arg4);

// Initial syscalls (no capabilities yet):
#define SYS_YIELD      1  // Yield CPU
#define SYS_EXIT       2  // Exit task
#define SYS_DEBUG_PRINT 3  // Temporary debugging
```

### 3.5 First User Task
```c
// Create a simple user task manually
void spawn_first_user_task(void) {
    struct task* task = task_create(0);  // Priority 0

    // Allocate user address space
    task->address_space = mmu_create_address_space();

    // Map user code at 0x400000
    extern char user_task_start[];
    extern char user_task_end[];
    size_t code_size = user_task_end - user_task_start;
    // ... map pages and copy code ...

    // Set up user stack at 0xC0000000
    // ... allocate and map stack ...

    // Initialize context for ring3
    task->context.eip = 0x400000;
    task->context.esp = 0xC0000000;
    task->context.cs = USER_CS;
    task->context.ss = USER_SS;

    scheduler_enqueue(task);
}
```

**Milestone:** Can run a "hello world" task in ring3, syscalls work

### 3.6 Units and Capability Table (First Cut)

Before full capabilities in Phase 4, introduce a minimal `unit_t` and
per-unit capability table so tasks/threads always belong to a unit:

```c
typedef uint64_t unit_id_t;
typedef int32_t  cap_handle_t;

typedef enum {
    CAP_TYPE_NONE = 0,
    CAP_TYPE_CHANNEL,
    CAP_TYPE_TIMER,
    // more later
} cap_type_t;

struct capability {
    cap_type_t  type;
    void       *obj;      // channel, timer, device, etc.
    uint64_t    rights;   // read/write/admin bits
};

struct unit_caps {
    struct capability *table;
    size_t             count;
    size_t             capacity;
};

struct unit {
    unit_id_t          id;
    struct page_table *address_space; // from Phase 2
    struct task       *threads;       // simple linked list for now
    struct unit_caps   caps;          // will be hardened in Phase 4
};
```

At this stage, kernel code populates caps directly for test units;
syscalls like `sys_cap_grant()` can be added later once the basic
layout is stable.

### 3.7 Kernel Channels (Internal Messaging Primitive)

Implement a simple in-kernel channel/message queue that the later IPC
layer (Phase 4 `ipc_port`) will build on:

```c
struct message {
    struct message *next;
    size_t          len;
    uint8_t        *data;
    // later: attached caps, trace id, etc.
};

struct channel {
    spinlock_t   lock;
    struct message *head;
    struct message *tail;
    size_t       queued_bytes;
    size_t       max_bytes;

    // waiters
    struct task *recv_waiters;
    struct task *send_waiters;
};

struct channel *chan_create(size_t max_bytes);
int chan_send(struct channel *ch, const void *buf, size_t len,
              uint64_t flags);
int chan_recv(struct channel *ch, void *buf, size_t buf_len,
              size_t *out_len, uint64_t flags, uint64_t timeout_ns);
```

The scheduler integrates with channels by blocking threads on
`send_waiters` / `recv_waiters` and waking them when space/data
becomes available. The syscall layer in Phase 4 will be thin wrappers
over this primitive.

## Phase 4: IPC & Capabilities (Week 7-8)

**Goal:** The microkernel core - this is what everything else builds on

### 4.1 Capability System
```c
// core/capability.h
struct capability {
    uint64_t id;              // Unique capability ID
    enum cap_type type;       // Memory, Port, IRQ, etc.
    uint32_t rights;          // READ, WRITE, GRANT, etc.
    void* object;             // Points to kernel object
};

#define CAP_RIGHT_READ    (1 << 0)
#define CAP_RIGHT_WRITE   (1 << 1)
#define CAP_RIGHT_GRANT   (1 << 2)  // Can pass to others
#define CAP_RIGHT_EXECUTE (1 << 3)

// Capability space per task (fixed-size for RT)
#define MAX_CAPS_PER_TASK 256

struct capability_space {
    struct capability caps[MAX_CAPS_PER_TASK];
    uint32_t bitmap[8];  // Track used slots, O(1) search
};

// Capability operations
int cap_check(struct task* task, cap_t cap_id, uint32_t required_rights);
int cap_grant(struct task* from, struct task* to, cap_t cap_id);
int cap_revoke(struct task* task, cap_t cap_id);
cap_t cap_derive(struct task* task, cap_t parent_cap, uint32_t reduced_rights);

// RT constraint: All ops O(1), no allocations
```

### 4.2 IPC - Message Passing
```c
// core/ipc.h
#define IPC_MSG_SIZE 256  // Fixed size for RT

struct ipc_message {
    uint32_t type;
    uint32_t length;
    cap_t caps[4];         // Pass capabilities!
    uint8_t data[IPC_MSG_SIZE];
};

struct ipc_port {
    uint32_t id;
    struct task* owner;
    struct ipc_message_queue queue;  // Bounded queue
    uint32_t max_queue_depth;        // For RT guarantees
};

// IPC operations
int ipc_send(cap_t port_cap, struct ipc_message* msg, uint64_t timeout_us);
int ipc_receive(cap_t port_cap, struct ipc_message* msg, uint64_t timeout_us);
int ipc_call(cap_t port_cap, struct ipc_message* req, struct ipc_message* resp,
             uint64_t timeout_us);  // Synchronous call

// RT constraints:
// - Pre-allocated message buffers
// - Bounded queue depth
// - Priority inheritance when blocking
// - Timeout always specified
```

### 4.3 Priority Inheritance
```c
// core/ipc.c
// When high-priority task blocks on IPC to low-priority server,
// temporarily boost server priority to avoid priority inversion

void ipc_priority_inherit(struct task* server, struct task* client) {
    if (client->priority > server->priority) {
        server->effective_priority = client->priority;
        scheduler_requeue(server);  // Move to higher priority queue
    }
}

// RT constraint: Must track priority chains for nested IPC
```

### 4.4 Syscalls for IPC
```c
// Add to syscall table:
#define SYS_IPC_SEND     10
#define SYS_IPC_RECV     11
#define SYS_IPC_CALL     12
#define SYS_CAP_CHECK    13
#define SYS_CAP_GRANT    14
```

**Milestone:** Tasks can send messages, pass capabilities, userspace servers feasible

## Phase 5: First Userspace Server (Week 9-10)

**Goal:** Prove the microkernel works by moving console to userspace

### 5.1 Console Server (Userspace!)
```c
// userspace/servers/console_server.c
int main(void) {
    cap_t console_port = /* get from kernel at boot */;
    cap_t vga_memory_cap = /* capability to VGA memory */;

    while (1) {
        struct ipc_message msg;
        ipc_receive(console_port, &msg, IPC_INFINITE);

        switch (msg.type) {
            case CONSOLE_WRITE:
                // Write to VGA using vga_memory_cap
                break;
            case CONSOLE_READ:
                // Read from keyboard (also via IPC)
                break;
        }
    }
}
```

### 5.2 Kernel Changes
```c
// core/init.c - spawn console server early
void kmain(void) {
    hal_x86_init();
    percpu_init();
    pmm_init();
    mmu_init();
    idt_init();
    timer_init();
    scheduler_init();

    // Create console server task
    struct task* console = task_create_user(console_server_binary,
                                            CONSOLE_SERVER_PRIORITY);

    // Grant it VGA memory capability
    cap_t vga_cap = cap_create_memory(0xB8000, 80*25*2, CAP_RIGHT_READ|CAP_RIGHT_WRITE);
    cap_grant_to_task(console, vga_cap);

    // Grant it port capability
    cap_t port_cap = cap_create_port(CONSOLE_PORT_ID, CAP_RIGHT_READ|CAP_RIGHT_WRITE);
    cap_grant_to_task(console, port_cap);

    // Start scheduling
    scheduler_start();  // Never returns
}
```

### 5.3 Update VGA to use IPC
```c
// lib/console_client.c (userspace library)
void console_write(const char* str) {
    struct ipc_message msg;
    msg.type = CONSOLE_WRITE;
    msg.length = strlen(str);
    memcpy(msg.data, str, msg.length);

    ipc_send(console_port_cap, &msg, 1000000);  // 1 second timeout
}

// Replace kprintf calls with IPC to console server
```

**Milestone:** VGA driver is in userspace, kernel just does IPC - true microkernel!

## Phase 6: SMP/Multicore (Week 11-12)

### 6.1 SMP Bootstrap
```c
// arch/x86/smp.c
void smp_init(void);
void smp_boot_cpu(uint32_t cpu_id);

// Application Processor entry
void ap_start(void) {
    hal->cpu_init();
    per_cpu_init(hal->cpu_id());
    scheduler_init();
    hal->irq_enable();

    // Enter scheduler
    schedule();
}
```

### 6.2 Inter-Processor Interrupts
```c
// core/ipi.c
enum ipi_type {
    IPI_SCHEDULE,      // Trigger reschedule
    IPI_TLB_FLUSH,     // Flush TLB
    IPI_STOP,          // Stop CPU
    IPI_CUSTOM,        // Custom handler
};

void ipi_send(uint32_t target_cpu, enum ipi_type type, void* data);
void ipi_broadcast(enum ipi_type type, void* data);
```

### 6.3 Per-CPU Optimization
```c
// Scheduler is already per-CPU from Phase 1
// Now test it with real load on multiple cores

// Each CPU maintains its own:
// - Run queues
// - Memory allocator cache
// - Trace buffer
// - IRQ handling

// RT constraint: Minimize cross-CPU communication
// Use lock-free per-CPU data wherever possible
```

**Milestone:** All CPUs running, true parallelism, load balancing

## Phase 7: More Userspace Servers (Week 13-14)

**Goal:** Move everything possible to userspace

### 7.1 Keyboard Driver Server
```c
// userspace/servers/keyboard_server.c
int main(void) {
    cap_t irq_cap = /* IRQ 1 capability */;
    cap_t io_cap = /* I/O port 0x60 capability */;

    while (1) {
        // Wait for keyboard interrupt
        irq_wait(irq_cap);

        // Read scancode
        uint8_t scancode = io_inb(io_cap, 0x60);

        // Send to subscribers via IPC
        ipc_send(keyboard_port, &msg, ...);
    }
}

// RT constraint: Interrupt latency still low because
// kernel dispatches to userspace server immediately
```

### 7.2 Serial Driver Server
```c
// userspace/servers/serial_server.c
// Similar pattern: IRQ capability + I/O port capability
```

### 7.3 Device Manager
```c
// userspace/servers/device_manager.c
// Coordinates device discovery and driver loading
// All via IPC and capabilities - no kernel involvement
```

**Milestone:** All drivers in userspace, kernel is <10K LOC

## Phase 8: Advanced Features (Week 15-16)

### 8.1 Shared Memory IPC
```c
// For high-bandwidth IPC (video, network)
int ipc_share_memory(cap_t dest_task, cap_t memory_cap);

// Zero-copy data transfer
```

### 8.2 IRQ Capabilities
```c
// Grant userspace servers ability to handle specific IRQs
cap_t irq_cap = cap_create_irq(IRQ_NUM, CAP_RIGHT_WAIT);
cap_grant_to_task(server_task, irq_cap);

// In userspace:
irq_wait(irq_cap);  // Blocks until IRQ fires
```

### 8.3 Memory Management Service
```c
// userspace/servers/memory_server.c
// Higher-level memory management in userspace
// Kernel only does page tables, this does allocation policies
```

**Milestone:** Feature-complete RT microkernel

## Success Criteria

After all phases:
- ✅ Boots on real hardware (not just QEMU)
- ✅ True microkernel: <10K LOC kernel, all services in userspace
- ✅ Hard RT: Bounded latency, deterministic scheduling
- ✅ Capability security: No ambient authority
- ✅ All CPUs utilized with lock-free per-CPU data
- ✅ All drivers in userspace (keyboard, serial, console)
- ✅ Fast IPC (< 500ns for small messages)
- ✅ Fast context switch (< 1µs)
- ✅ Scalable (performance grows linearly with cores)
- ✅ Testable (unit tests for each component)
- ✅ Verifiable (small TCB, clear invariants)

## Development Process

### For Each Component:

1. **Design** - Write interface first
2. **Implement** - Write implementation
3. **Test** - Unit tests + integration tests
4. **Document** - API docs + examples
5. **Review** - Security review + performance check

### Quality Gates:

- ✅ No warnings at `-Wall -Wextra -Werror`
- ✅ All functions < 50 lines
- ✅ All files < 500 lines
- ✅ Code coverage > 80%
- ✅ No known security issues

## Tools We'll Build

1. **Kernel debugger** - Interactive debugging
2. **Profiler** - Performance analysis
3. **Memory leak detector** - Find leaks
4. **Capability visualizer** - See security graph
5. **Trace viewer** - Analyze execution

## Comparison After Completion

| Feature | Our Kernel | Linux | Fuchsia | seL4 |
|---------|-----------|-------|---------|------|
| Architecture | Microkernel | Monolithic | Microkernel | Microkernel |
| Lines of code (kernel) | ~10K | ~30M | ~100K | ~10K |
| Drivers location | Userspace | Kernel | Userspace | Userspace |
| Security model | Capabilities | DAC/MAC | Capabilities | Capabilities |
| Real-time | Hard | Soft | Soft | Hard |
| IPC speed | <500ns | N/A | ~1µs | ~200ns |
| Formally verified | No | No | Partial | Yes |
| Multicore | Yes | Yes | Yes | Yes |
| Per-CPU optimization | Yes | Partial | Yes | N/A |
| Priority inheritance | Yes | Yes | Yes | Yes |

## Key Design Principles

1. **Microkernel First:** IPC and capabilities come early (Phase 4), not late
2. **RT Throughout:** Every kernel path has bounded time, no surprises
3. **Userspace by Default:** If it can be userspace, it must be userspace
4. **Per-CPU Everything:** Minimize locking, maximize parallelism
5. **Capability Security:** No ambient authority, explicit capability passing
6. **Small TCB:** <10K LOC for potential formal verification
7. **Arch in `arch/`:** Keep context switch, page tables, ISR stubs in `arch/` so unit/thread/channel logic stays generic C
8. **No POSIX in Kernel:** Fork/exec/signals live in a userspace “personality” built on units, not in the core ABI
9. **Narrow ABI Header & Vertical Slices:** Treat a single syscall header as the contract, and grow the kernel via small end‑to‑end slices (timer→scheduler, PMM→VM, channel→syscall) rather than broad layers
10. **Profiles + Config:** Use simple build-time profiles (DEV/STANDARD/HARDENED) and a single `kernel_config_t` runtime struct to control optional hardening features without multiple forks of the codebase.

## Current Phase

✅ Phase 1 complete (HAL, per-CPU, modular VGA)
🔨 Phase 2 in progress (IDT, timer, basic paging)

## Next Steps

Start Phase 2 by implementing:
1. IDT and exception handlers
2. PIT timer with microsecond precision
3. Simple bitmap physical memory manager
4. Basic x86 paging support
