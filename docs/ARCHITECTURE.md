# Modern Kernel Architecture Design

A ground-up design for a modern, secure, multicore kernel learning from historical caveats.

## Historical Caveats We're Addressing

### 1. **Monolithic Design Problems** (Linux, Windows legacy)
- ❌ Everything in kernel space = large attack surface
- ❌ One bug crashes entire system
- ❌ Hard to isolate failures
- ❌ Difficult to update components independently

**Our Approach:** Hybrid microkernel with capability-based security

### 2. **Memory Unsafety** (C kernels)
- ❌ Buffer overflows
- ❌ Use-after-free
- ❌ Null pointer dereferences
- ❌ Data races

**Our Approach:** Safe C subset + runtime checks, designed for future Rust migration

### 3. **Poor Concurrency Model** (Big Kernel Lock era)
- ❌ Coarse-grained locking
- ❌ Lock contention
- ❌ Not scalable to many cores

**Our Approach:** Lock-free where possible, per-CPU data, fine-grained locking

### 4. **Weak Security Model** (Traditional Unix)
- ❌ Ambient authority (processes can do anything)
- ❌ Confused deputy problem
- ❌ No isolation between drivers
- ❌ Direct hardware access

**Our Approach:** Capability-based security, driver isolation, principle of least privilege

### 5. **Multicore as Afterthought** (Early SMP)
- ❌ TLB shootdowns everywhere
- ❌ Cache line bouncing
- ❌ Not NUMA-aware

**Our Approach:** Per-CPU structures from day 1, NUMA-aware allocation

## Modern Design Principles

### 1. **Microkernel Core + Modular Services**

```
┌─────────────────────────────────────────┐
│          User Space                     │
│                                         │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐│
│  │ Driver  │  │ Driver  │  │  File   ││
│  │ Manager │  │ Modules │  │ System  ││
│  └─────────┘  └─────────┘  └─────────┘│
├─────────────────────────────────────────┤
│       Kernel Space (Minimal)            │
│                                         │
│  ┌──────────┐  ┌──────────┐  ┌───────┐│
│  │Scheduler │  │  Memory  │  │  IPC  ││
│  │          │  │ Manager  │  │       ││
│  └──────────┘  └──────────┘  └───────┘│
└─────────────────────────────────────────┘
```

**Kernel contains ONLY:**
- Scheduler (per-CPU)
- Memory management (paging, capabilities)
- IPC (message passing)
- Interrupt dispatch
- Basic CPU management

**Everything else runs as services:**
- Device drivers (isolated address spaces)
- File systems (userspace)
- Network stack (can be userspace or kernel module)
- Graphics (userspace with DMA)

### 2. **Capability-Based Security** (inspired by seL4, Fuchsia)

**No ambient authority** - processes can only access resources they have explicit capabilities for.

```c
// Traditional (WRONG - ambient authority):
fd = open("/dev/disk0", O_RDWR);  // Anyone can try this

// Capability-based (RIGHT):
cap_t disk_cap = receive_capability();  // Must be explicitly granted
fd = cap_open(disk_cap, O_RDWR);
```

**Benefits:**
- ✅ Principle of least privilege by default
- ✅ No confused deputy attacks
- ✅ Easy to reason about security
- ✅ Enables formal verification

### 3. **Per-CPU Architecture**

```c
struct per_cpu_data {
    uint32_t cpu_id;
    struct task* current_task;     // No shared access
    struct task_queue run_queue;   // Per-CPU scheduling
    void* kernel_stack;
    struct memory_allocator cpu_allocator;  // Lock-free
    uint64_t ticks;
    // No locks needed for this data!
};

// Access pattern:
#define this_cpu() (&per_cpu[get_cpu_id()])
```

**Benefits:**
- ✅ No lock contention for common operations
- ✅ Better cache locality
- ✅ Natural NUMA awareness
- ✅ Scales to many cores

### 4. **Hardware Abstraction Layer (HAL)**

```c
// Hardware-specific code isolated in HAL
struct hal_interface {
    void (*init)(void);
    void (*enable_interrupts)(void);
    void (*disable_interrupts)(void);
    void* (*map_io)(phys_addr_t addr, size_t size);
    void (*send_ipi)(uint32_t cpu_id, uint32_t vector);
    // ... more hardware operations
};

// Kernel code is portable:
void kernel_init(void) {
    hal->init();
    scheduler_init();
    hal->enable_interrupts();
}
```

**Benefits:**
- ✅ Easy to port to new architectures
- ✅ Can support multiple platforms in one binary
- ✅ Clear separation of concerns
- ✅ Testable on non-hardware platforms

### 5. **Message-Passing IPC** (zero-copy where possible)

```c
// Fast IPC with capability passing
struct message {
    uint32_t type;
    uint32_t length;
    capability_t caps[4];  // Pass capabilities!
    uint8_t data[];
};

// Zero-copy for large transfers:
int send_message_zerocopy(port_t dest, struct message* msg);
```

**Benefits:**
- ✅ Clean component boundaries
- ✅ Can be made very fast (< 1000 cycles)
- ✅ Enables driver isolation
- ✅ Natural async programming model

### 6. **Memory Safety Without Garbage Collection**

```c
// Reference counting for kernel objects
struct kobject {
    atomic_t refcount;
    void (*destroy)(struct kobject*);
};

static inline void kobject_get(struct kobject* obj) {
    atomic_inc(&obj->refcount);
}

static inline void kobject_put(struct kobject* obj) {
    if (atomic_dec_and_test(&obj->refcount))
        obj->destroy(obj);
}

// Bounds-checked arrays:
struct safe_array {
    void* data;
    size_t length;
    size_t capacity;
};

static inline void* safe_array_get(struct safe_array* arr, size_t idx) {
    if (idx >= arr->length)
        return NULL;  // or panic
    return arr->data + idx * arr->elem_size;
}
```

**Benefits:**
- ✅ No use-after-free bugs
- ✅ No buffer overflows
- ✅ Deterministic cleanup
- ✅ No GC pauses

### 7. **Lock-Free Data Structures**

```c
// Per-CPU work queues (no locks!)
struct work_queue {
    struct work_item* head;
    struct work_item* tail;
    // Only accessed by owning CPU
};

// When cross-CPU communication needed: message passing or atomics
void schedule_work_on_cpu(uint32_t cpu, struct work_item* work) {
    // Atomic enqueue
    struct work_item* old_head;
    do {
        old_head = per_cpu[cpu].work_queue.head;
        work->next = old_head;
    } while (!atomic_cas(&per_cpu[cpu].work_queue.head, old_head, work));

    // Send IPI to wake CPU
    hal->send_ipi(cpu, WORK_VECTOR);
}
```

**Benefits:**
- ✅ No lock contention
- ✅ Better cache utilization
- ✅ Scales linearly with cores
- ✅ Lower latency

### 8. **Modular Driver Architecture**

```c
// Driver interface (all drivers implement this)
struct driver_ops {
    int (*probe)(struct device*);
    int (*remove)(struct device*);
    int (*suspend)(struct device*);
    int (*resume)(struct device*);
};

// Drivers register with kernel:
int register_driver(struct driver_ops* ops, const char* name);

// Can be:
// 1. Kernel modules (loaded dynamically)
// 2. Userspace drivers (most secure)
// 3. Linked into kernel (for essential drivers)
```

**Benefits:**
- ✅ Hot-plug drivers
- ✅ Isolated failures
- ✅ Easy to update
- ✅ Flexible deployment

### 9. **Built-in Tracing and Debugging**

```c
// Lightweight tracing (inspired by Linux eBPF)
struct trace_event {
    uint64_t timestamp;
    uint32_t cpu_id;
    uint32_t event_type;
    uint64_t data[4];
};

// Circular per-CPU trace buffers
void trace_event(uint32_t type, uint64_t d0, uint64_t d1) {
    struct per_cpu_data* cpu = this_cpu();
    uint32_t idx = cpu->trace_idx++ % TRACE_BUFFER_SIZE;
    cpu->trace_buffer[idx] = (struct trace_event) {
        .timestamp = rdtsc(),
        .cpu_id = cpu->cpu_id,
        .event_type = type,
        .data = {d0, d1, 0, 0}
    };
}
```

**Benefits:**
- ✅ Always-on tracing (zero overhead when not reading)
- ✅ Post-mortem debugging
- ✅ Performance profiling
- ✅ Security auditing

### 10. **Modern Hardware Utilization**

```c
// IOMMU for device isolation
struct iommu_domain {
    page_table_t* device_page_table;
    capability_t allowed_memory_regions[16];
};

void iommu_map_device(struct device* dev, struct iommu_domain* domain) {
    // Device can only access mapped regions
    hal->iommu_attach(dev->bus_addr, domain->device_page_table);
}

// Hardware virtualization support
struct vcpu {
    uint32_t cpu_id;
    struct vmcs* vmcs;  // Intel VT-x
    struct vmcb* vmcb;  // AMD-V
};

// TPM/secure boot integration
bool verify_module_signature(struct module* mod) {
    return crypto_verify(mod->data, mod->sig, tpm_public_key);
}
```

**Benefits:**
- ✅ Hardware-enforced isolation
- ✅ VM support built-in
- ✅ Secure boot
- ✅ DMA protection

## Component Boundaries

### Core Kernel (Minimal Trusted Computing Base)

```c
// kernel/core/
├── scheduler.c          // Per-CPU scheduler
├── memory.c             // Page allocator, capabilities
├── ipc.c                // Message passing
├── interrupt.c          // Interrupt dispatch
├── smp.c                // Multicore management
└── capability.c         // Security primitives
```

**Size target:** < 10,000 lines of code (verifiable!)

### Hardware Abstraction Layer

```c
// kernel/hal/
├── x86/                 // x86-64 support
│   ├── cpu.c
│   ├── mmu.c
│   └── interrupts.c
├── arm64/               // ARM64 support
│   └── ...
└── riscv/               // RISC-V support (future)
```

### Kernel Services (Can be modules or userspace)

```c
// kernel/services/
├── drivers/             // Device drivers
│   ├── vga/
│   ├── serial/
│   ├── keyboard/
│   └── disk/
├── fs/                  // File systems (can be userspace)
├── net/                 // Network stack (can be userspace)
└── security/            // Security policies
```

## Memory Layout

```
0x0000000000000000 - User space start
0x0000000100000000 - User space end (4GB)
0xFFFF800000000000 - Kernel space start (higher half)
0xFFFF800000100000 - Kernel code (1MB mark)
0xFFFF800001000000 - Kernel heap
0xFFFF800100000000 - MMIO mappings
0xFFFFFFFFC0000000 - Per-CPU data (last 1GB)
```

## Security Properties

1. **Isolation:** Each driver runs in its own address space
2. **Least Privilege:** Capabilities required for all operations
3. **Fail-Safe:** Single driver crash doesn't crash kernel
4. **Auditable:** All security-relevant operations traced
5. **Verifiable:** Small TCB enables formal verification
6. **Defense in Depth:** IOMMU, NX bit, ASLR, stack canaries

## Hardening Profiles and Runtime Configuration

In addition to the core design, the kernel supports the idea of **optional hardening** that can be enabled without forking the codebase.

### Compile-Time Profiles

We define three build-time profiles:

- `CONFIG_PROFILE_DEV`
- `CONFIG_PROFILE_STANDARD` (default)
- `CONFIG_PROFILE_HARDENED`

From these, we derive a small set of feature flags (see `include/kernel/config.h`):

- `CONFIG_ENABLE_DEBUG_ASSERTS`
- `CONFIG_ENABLE_PARANOID_CHECKS`
- `CONFIG_ENABLE_RAM_SCRUBBER`
- `CONFIG_ENABLE_HASH_KERNEL_TEXT`
- `CONFIG_ENABLE_LOG_VERBOSE`

Profiles decide **which mechanisms exist in the binary**; mechanisms that are not enabled are compiled out and incur no runtime cost.

### Runtime Configuration (`kernel_config_t`)

At runtime, all tunable hardening knobs are grouped into a single struct:

```c
typedef struct {
    uint32_t version;

    // Scrubber
    uint8_t  ram_scrubber_enabled;
    uint32_t ram_scrubber_rate_pages_per_sec;

    // Invariants
    uint8_t  invariants_enabled;
    uint8_t  invariants_sample_rate; // 1 = always, 10 = 1/10 ops

    // Messaging
    uint8_t  msg_header_checks_enabled;
    uint8_t  msg_payload_crc_enabled;
    uint8_t  msg_crc_sample_rate;

    // Watchdog
    uint8_t  watchdog_enabled;
    uint32_t watchdog_timeout_ms;

    // Kernel text verification
    uint8_t  hash_kernel_text_enabled;
    uint32_t hash_kernel_text_interval_sec;
} kernel_config_t;
```

`kernel_config_init_defaults()` will use compile-time feature flags to set sane defaults per profile (e.g., DEV vs STANDARD vs HARDENED).

Future mechanisms (RAM scrubber, invariants, message CRC, watchdog, text hashing) will:

- Be compiled in or out via `CONFIG_ENABLE_*`.
- Read intensity/enable flags from `kernel_config_t`.
- Use **sampling** (e.g. 1/N checks) to remain RT-compliant.

## Performance Goals

| Operation | Target | Rationale |
|-----------|--------|-----------|
| Context switch | < 1µs | Modern CPUs are fast |
| IPC (same core) | < 500ns | Must be faster than syscall |
| IPC (cross-core) | < 2µs | Cache coherency cost |
| Interrupt latency | < 10µs | Hard real-time capable |
| Syscall | < 100ns | Minimal overhead |

## Development Phases

### Phase 1: Foundation (We are here)
- ✅ Boot and VGA output
- ✅ Basic HAL
- → IDT and interrupts
- → Per-CPU infrastructure
- → Simple scheduler

### Phase 2: Memory & Security
- Memory management (paging)
- Capability system
- User/kernel separation
- Simple IPC

### Phase 3: Drivers & Modularity
- Driver framework
- VGA driver (isolated)
- Serial driver
- Keyboard driver
- Module loading

### Phase 4: Multicore
- SMP bootstrap
- Per-CPU schedulers
- IPI infrastructure
- Spinlocks and atomics

### Phase 5: Advanced Features
- File system (userspace)
- Network stack
- Graphics (DRM-like)
- VM support

## Why This Approach?

### vs. Linux (Monolithic)
- ✅ Smaller attack surface
- ✅ Better isolation
- ✅ Easier to verify
- ⚠️ Slightly slower (but modern IPC is fast!)

### vs. Traditional Microkernel (Minix, Hurd)
- ✅ Better performance (hybrid approach)
- ✅ More pragmatic (Linux drivers can be ported)
- ✅ Proven pattern (macOS, Windows NT do this)

### vs. Research Kernels (seL4)
- ⚠️ Less formally verified
- ✅ More practical/usable
- ✅ Faster development
- ✅ Can add formal verification later

## Modern Inspiration

- **Fuchsia** (Google): Capability-based, modern IPC
- **seL4**: Formally verified, capability-based
- **Redox** (Rust): Memory-safe, microkernel
- **macOS XNU**: Hybrid microkernel that works
- **Windows NT**: Successful hybrid design

## Code Organization Philosophy

```c
// GOOD: Clear interfaces
struct scheduler_ops {
    void (*schedule)(void);
    void (*add_task)(struct task*);
    void (*remove_task)(struct task*);
};

// BAD: Tight coupling
void schedule(void) {
    // Direct access to VGA
    vga_print("Scheduling...");  // WRONG!
}

// GOOD: Dependency injection
void schedule(void) {
    // Use tracing/logging subsystem
    trace_event(TRACE_SCHEDULE, current_task->id, 0);
}
```

## Next Steps

We'll implement this architecture incrementally, starting with the foundation and building up modular components that can be tested, verified, and swapped out independently.

Each component will have:
- ✅ Clear interface definition
- ✅ Unit tests
- ✅ Integration tests
- ✅ Performance benchmarks
- ✅ Security review

Ready to build the future? 🚀
