#ifndef KERNEL_PERCPU_H
#define KERNEL_PERCPU_H

#include <kernel/types.h>

// Maximum number of CPUs supported
#define MAX_CPUS 256

// Trace buffer size per CPU
#define TRACE_BUFFER_SIZE 1024

// Forward declarations
struct task;

// Trace event types
enum trace_event_type {
    TRACE_INTERRUPT = 0,
    TRACE_SCHEDULE,
    TRACE_TASK_SWITCH,
    TRACE_SYSCALL,
    TRACE_IPI,
    TRACE_TLB_FLUSH,
    TRACE_CUSTOM,
};

// Lightweight trace event (for performance debugging)
struct trace_event {
    uint64_t timestamp;     // TSC or microseconds
    uint32_t cpu_id;
    uint32_t event_type;
    uint64_t data[4];       // Event-specific data
};

// Per-CPU trace buffer (circular)
struct trace_buffer {
    struct trace_event events[TRACE_BUFFER_SIZE];
    uint32_t head;          // Write position
    uint32_t tail;          // Read position (for userspace reader)
    atomic_t overflow;      // Count of lost events
};

// Per-CPU data structure
// Each CPU has its own instance - NO SHARED DATA!
// This eliminates lock contention and improves cache locality
struct per_cpu_data {
    // Identity
    uint32_t cpu_id;                // This CPU's ID (0-based)
    bool online;                    // Is this CPU active?

    // Current execution context
    struct task* current_task;      // Currently running task
    void* kernel_stack;             // Kernel mode stack for this CPU

    // Scheduling
    struct task* idle_task;         // Idle task for this CPU
    struct list_head run_queue;     // Ready-to-run tasks
    uint64_t ticks;                 // Timer ticks on this CPU
    uint64_t context_switches;      // Performance counter

    // Memory allocator (per-CPU cache)
    void* slab_cache;               // CPU-local memory cache

    // Tracing and debugging
    struct trace_buffer trace;      // Lock-free trace buffer

    // Statistics
    uint64_t interrupts_handled;    // Total interrupts
    uint64_t ipis_received;         // Inter-processor interrupts
    uint64_t tlb_flushes;           // TLB flush count

    // Padding to avoid false sharing (64-byte cache line)
    uint8_t padding[0];
} __attribute__((aligned(64)));     // Align to cache line

// Global per-CPU data array
extern struct per_cpu_data per_cpu[MAX_CPUS];

// Number of online CPUs
extern uint32_t num_cpus_online;

// Get this CPU's data (fast - no function call)
#define this_cpu() (&per_cpu[hal->cpu_id()])

// Get specific CPU's data
#define cpu_data(cpu_id) (&per_cpu[cpu_id])

// Initialize per-CPU infrastructure
void percpu_init(void);

// Initialize a specific CPU's data
void percpu_init_cpu(uint32_t cpu_id);

// Trace an event (lock-free, safe to call from interrupt context)
void trace_event(enum trace_event_type type, uint64_t d0, uint64_t d1,
                 uint64_t d2, uint64_t d3);

// Read trace events (for debugging tools)
int trace_read(uint32_t cpu_id, struct trace_event* events, size_t count);

// CPU-local work queue (for deferred work)
struct work_item {
    void (*func)(void* data);
    void* data;
    struct work_item* next;
};

// Schedule work on a specific CPU
void schedule_work_on_cpu(uint32_t cpu_id, struct work_item* work);

// Process pending work on this CPU
void process_pending_work(void);

#endif // KERNEL_PERCPU_H
