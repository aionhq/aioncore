# Units Architecture - Core Design

This document defines the "unit" model that guides our kernel design from Phase 2 onward.

## Philosophy: Units, Not Processes

We're **not** building a UNIX clone. We're building a capability-based microkernel where:

- **Units** are isolated containers of execution (not "processes")
- **Threads** execute within units
- **Channels** provide message passing between units
- **Capabilities** grant explicit access rights
- No fork, no exec, no signals, no POSIX in kernel

POSIX can be implemented as a **user-space personality** later.

---

## Core Abstractions

### Unit: The Container

```c
typedef uint64_t unit_id_t;

typedef struct unit {
    unit_id_t       id;              // Unique unit identifier
    addr_space_t   *as;              // Virtual address space
    thread_t       *threads;         // Threads executing in this unit
    unit_caps_t     caps;            // Capability table

    // Resource limits (enforced by kernel)
    size_t          max_memory;      // Max physical frames
    size_t          used_memory;     // Current frames mapped
    uint32_t        max_threads;
    uint32_t        num_threads;

    // Scheduling class (RT, interactive, batch)
    sched_class_t   sched_class;

    // Optional: namespace IDs, labels, parent unit, etc.
} unit_t;
```

**Key properties:**
- Units are isolated: no shared memory by default
- Units are secure: all access via capabilities
- Units are accountable: resource limits enforced
- Units can be kernel or userspace

### Thread: The Execution Context

```c
typedef struct thread {
    struct thread  *next;            // In runqueue or waitqueue
    unit_t         *unit;            // Owner unit
    cpu_state_t     cpu_state;       // Saved registers (arch-specific)

    uint64_t        id;              // Unique thread ID
    int             state;           // RUNNING, READY, BLOCKED, ZOMBIE

    // Scheduling
    int             base_priority;   // Fixed priority
    int             effective_priority; // With priority inheritance
    uint64_t        time_slice_ns;   // RT time quantum
    uint64_t        deadline_ns;     // For EDF scheduling (optional)

    // Blocking
    void           *wait_channel;    // What thread is waiting on
    uint64_t        wakeup_time;     // For timed waits

    // Statistics
    uint64_t        runtime_ns;
    uint64_t        context_switches;
} thread_t;
```

**Key properties:**
- Threads are cheap: just saved registers + metadata
- Threads are preemptible: scheduler can context-switch any time
- Threads can block: on channels, timers, or other resources
- Threads inherit unit's properties

### Channel: The Message Queue

```c
typedef struct message {
    struct message *next;
    size_t          len;             // Payload length
    uint8_t        *data;            // Payload (or inline for small msgs)

    // Optional: attached capabilities for passing rights
    cap_handle_t   *caps;
    size_t          num_caps;

    // Tracing
    uint64_t        trace_id;        // For distributed tracing
    uint64_t        timestamp;
} message_t;

typedef struct channel {
    spinlock_t      lock;            // Protects queue

    message_t      *head;
    message_t      *tail;
    size_t          queued_bytes;
    size_t          max_bytes;       // Backpressure limit

    // Waiting threads
    thread_t       *recv_waiters;    // Threads blocked on recv
    thread_t       *send_waiters;    // Threads blocked on send (if full)

    // Statistics
    uint64_t        msgs_sent;
    uint64_t        msgs_received;
} channel_t;
```

**Key properties:**
- Channels are bounded: max_bytes prevents memory exhaustion
- Channels support blocking: threads wait if empty/full
- Channels can pass capabilities: delegate rights
- Channels are efficient: zero-copy for small messages

### Capability: The Access Token

```c
typedef int32_t cap_handle_t;  // Index into unit's cap table

typedef enum {
    CAP_TYPE_NONE = 0,
    CAP_TYPE_CHANNEL,
    CAP_TYPE_MEMORY,
    CAP_TYPE_IRQ,
    CAP_TYPE_IO_PORT,
    CAP_TYPE_UNIT,
    // More types as needed
} cap_type_t;

#define CAP_RIGHT_READ    (1 << 0)
#define CAP_RIGHT_WRITE   (1 << 1)
#define CAP_RIGHT_EXECUTE (1 << 2)
#define CAP_RIGHT_GRANT   (1 << 3)  // Can pass to other units

typedef struct capability {
    cap_type_t      type;
    void           *obj;             // Pointer to channel_t, etc.
    uint64_t        rights;          // Bitmask of CAP_RIGHT_*

    // Bookkeeping
    atomic_t        refcount;        // How many caps reference this object
} capability_t;

typedef struct unit_caps {
    capability_t   *table;           // Dense array
    size_t          count;
    size_t          capacity;
    spinlock_t      lock;
} unit_caps_t;
```

**Key properties:**
- Capabilities are unforgeable: only kernel creates them
- Capabilities are explicit: no ambient authority
- Capabilities can be delegated: if GRANT right held
- Capabilities are typed: kernel validates operations

---

## API Design

### Physical Memory Manager (PMM)

```c
// Initialize from bootloader memory map
void pmm_init(multiboot_memory_map_t *mmap);

// Allocate/free physical frames
phys_addr_t pmm_alloc_frame(void);
void pmm_free_frame(phys_addr_t frame);

// Accounting (for unit memory limits)
size_t pmm_get_free_frames(void);
size_t pmm_get_used_frames(void);
```

**Implementation notes:**
- Start with bitmap allocator (simple, O(1) with free list)
- Later: per-CPU frame caches for scalability
- Track frame → unit ownership for accounting

### Virtual Memory Manager (VMM)

```c
// Address space management
addr_space_t *vm_as_create(void);
void vm_as_destroy(addr_space_t *as);

// Mapping operations
int vm_map(addr_space_t *as, uintptr_t vaddr, phys_addr_t paddr,
           size_t len, uint64_t flags);
int vm_unmap(addr_space_t *as, uintptr_t vaddr, size_t len);

// Switch address space
void vm_switch(addr_space_t *as);  // Install in CR3/SATP/TTBR

// Flags
#define VM_READ    (1 << 0)
#define VM_WRITE   (1 << 1)
#define VM_EXEC    (1 << 2)
#define VM_USER    (1 << 3)
```

**Implementation notes:**
- Higher-half kernel (e.g., 0xFFFF800000000000 on x86-64, 0xC0000000 on x86-32)
- Per-unit address spaces
- Copy-on-write for shared mappings (later optimization)

### Timer Subsystem

```c
// Initialize timer (PIT, HPET, APIC, etc.)
void timer_init(uint32_t frequency_hz);

// Read monotonic time
uint64_t timer_read_ns(void);    // Nanoseconds since boot
uint64_t timer_read_tsc(void);   // Raw cycle counter

// Timer wheel for efficient timeouts
void timer_add(timer_t *timer, uint64_t deadline_ns, void (*callback)(void*), void *arg);
void timer_cancel(timer_t *timer);

// Called from timer interrupt
void timer_interrupt_handler(void);
```

**Implementation notes:**
- Calibrate TSC against PIT at boot
- Timer wheel or min-heap for pending timers
- Hook into scheduler_tick() for preemption

### Scheduler

```c
// Initialize scheduler (per-CPU)
void scheduler_init(void);

// Add/remove threads
void scheduler_add(thread_t *thread);
void scheduler_remove(thread_t *thread);

// Scheduling decisions
void scheduler_tick(void);        // Called on timer interrupt
void scheduler_yield(void);       // Current thread yields
thread_t *scheduler_pick_next(void); // O(1) priority-based

// Blocking/waking
void scheduler_block(thread_t *thread, void *wait_channel);
void scheduler_wake(thread_t *thread);
void scheduler_wake_all(void *wait_channel);
```

**Implementation notes:**
- Fixed-priority scheduling (256 priority levels)
- Bitmap for O(1) runqueue search
- Per-CPU runqueues (no locking)
- Priority inheritance for channels/locks

### Channel Operations

```c
// Create/destroy channels
channel_t *chan_create(size_t max_bytes);
void chan_destroy(channel_t *ch);

// Send/receive
int chan_send(channel_t *ch, const void *buf, size_t len, uint64_t flags);
int chan_recv(channel_t *ch, void *buf, size_t buf_len,
              size_t *out_len, uint64_t flags, uint64_t timeout_ns);

// Flags
#define CHAN_NONBLOCK  (1 << 0)   // Don't block if full/empty
#define CHAN_PEEK      (1 << 1)   // Receive without removing message
```

**Implementation notes:**
- Pre-allocated message pool (bounded memory)
- Zero-copy for large messages via shared memory capabilities
- Priority inheritance: if high-priority thread sends to low-priority receiver

### Unit Management

```c
// Create/destroy units
unit_t *unit_create(const unit_spec_t *spec);
void unit_destroy(unit_t *unit);

// Thread management within unit
thread_t *unit_thread_create(unit_t *unit, void (*entry)(void*), void *arg);
void unit_thread_destroy(thread_t *thread);

// Capability management
cap_handle_t unit_cap_grant(unit_t *from, unit_t *to, cap_handle_t cap, uint64_t rights);
int unit_cap_revoke(unit_t *unit, cap_handle_t cap);
capability_t *unit_cap_lookup(unit_t *unit, cap_handle_t cap);
```

---

## Syscall ABI

### Unit syscalls

```c
// Create a new unit
long sys_unit_create(const struct unit_spec *user_spec, unit_handle_t *user_out);

// Start executing threads in unit
long sys_unit_start(unit_handle_t uh);

// Get unit information
long sys_unit_get_info(unit_handle_t uh, struct unit_info *user_out);

// Destroy unit (if you have capability)
long sys_unit_destroy(unit_handle_t uh);
```

### Thread syscalls

```c
// Create thread in current unit
long sys_thread_create(void (*entry)(void*), void *arg, thread_handle_t *out);

// Yield CPU
long sys_thread_yield(void);

// Exit thread
__attribute__((noreturn)) void sys_thread_exit(int status);

// Sleep
long sys_thread_sleep(uint64_t duration_ns);
```

### Channel syscalls

```c
// Create channel
long sys_chan_create(const struct chan_spec *user_spec, int *user_out);

// Send message
long sys_chan_send(int chan_fd, const void *buf, size_t len, uint64_t flags);

// Receive message
long sys_chan_recv(int chan_fd, void *buf, size_t len, uint64_t *out_len,
                   uint64_t flags, uint64_t timeout_ns);

// Close channel end
long sys_chan_close(int chan_fd);
```

### Capability syscalls

```c
// Grant capability to another unit
long sys_cap_grant(cap_handle_t cap, unit_handle_t target_unit, uint64_t rights);

// Get capability info
long sys_cap_get_info(cap_handle_t cap, struct cap_info *user_out);

// Revoke capability
long sys_cap_revoke(cap_handle_t cap);
```

---

## Example: First Userspace Unit

### Kernel creates and starts a unit:

```c
// In kernel init
void spawn_first_userspace_unit(void) {
    // 1. Create unit
    unit_spec_t spec = {
        .max_memory = 16 * 1024 * 1024,  // 16 MB
        .max_threads = 4,
        .sched_class = SCHED_CLASS_INTERACTIVE,
    };
    unit_t *unit = unit_create(&spec);

    // 2. Map user program
    extern uint8_t user_program_start[];
    extern uint8_t user_program_end[];
    size_t program_size = user_program_end - user_program_start;

    phys_addr_t program_frame = pmm_alloc_frame();
    memcpy((void*)program_frame, user_program_start, program_size);
    vm_map(unit->as, 0x400000, program_frame, PAGE_SIZE, VM_READ | VM_EXEC | VM_USER);

    // 3. Map user stack
    phys_addr_t stack_frame = pmm_alloc_frame();
    vm_map(unit->as, 0xC0000000 - PAGE_SIZE, stack_frame, PAGE_SIZE, VM_READ | VM_WRITE | VM_USER);

    // 4. Create console channel capability
    channel_t *console_chan = chan_create(4096);
    cap_handle_t console_cap = unit_cap_grant(kernel_unit, unit,
                                               kernel_console_cap,
                                               CAP_RIGHT_WRITE);

    // 5. Create first thread
    thread_t *thread = unit_thread_create(unit, NULL, NULL);
    thread->cpu_state.rip = 0x400000;  // Entry point
    thread->cpu_state.rsp = 0xC0000000;
    thread->cpu_state.cs = USER_CS;
    thread->cpu_state.ss = USER_SS;

    // 6. Add to scheduler
    scheduler_add(thread);

    kprintf("Created first userspace unit (id=%lu)\n", unit->id);
}
```

### Userspace program:

```c
// user/hello.c
#include <aion/syscall.h>

void _start(void) {
    const char *msg = "Hello from userspace unit!\n";

    // Capability handle 0 is console (granted by kernel)
    sys_chan_send(0, msg, 28, 0);

    // Exit
    sys_thread_exit(0);
}
```

---

## Implementation Phases

### Phase 2: Foundation for Units

1. **Timer** - Design as scheduler heartbeat from the start
2. **PMM** - Track frames, prepare for unit accounting
3. **VMM** - Per-unit address spaces, higher-half kernel

### Phase 3: Units, Threads, Channels

1. **In-kernel threads** - Get preemptive scheduling working
2. **Unit abstraction** - Wrap threads in units, even for kernel
3. **Channel messaging** - In-kernel message queues
4. **First userspace unit** - Map simple program, switch to ring3

### Phase 4: Capabilities & Security

1. **Capability table** - Per-unit cap management
2. **Syscall enforcement** - Check caps before operations
3. **Capability passing** - Via channels

### Phase 5+: Polish & Performance

1. **Zero-copy messaging** - Share pages via memory caps
2. **SMP** - Per-CPU runqueues, work stealing
3. **RT guarantees** - Priority inheritance, deadline scheduling
4. **Tracing** - Distributed tracing across units

---

## Design Principles

1. **No POSIX in kernel** - Build units first, POSIX personality later
2. **Capabilities everywhere** - No ambient authority
3. **Messages, not syscalls** - Most operations via IPC
4. **Small vertical slices** - End-to-end at every step
5. **One narrow ABI** - `include/aion/syscall.h` is the contract

---

Last Updated: 2025-11-21
Next: Phase 2 implementation with unit-awareness built in
