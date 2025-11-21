// Per-CPU Infrastructure Implementation

#include <kernel/percpu.h>
#include <kernel/hal.h>
#include <kernel/types.h>

// Global per-CPU data array
struct per_cpu_data per_cpu[MAX_CPUS];

// Number of online CPUs
uint32_t num_cpus_online = 0;

// Initialize per-CPU infrastructure (called once at boot)
void percpu_init(void) {
    // Initialize boot CPU's data
    uint32_t boot_cpu = hal->cpu_id();
    percpu_init_cpu(boot_cpu);

    num_cpus_online = 1;
}

// Initialize a specific CPU's data
void percpu_init_cpu(uint32_t cpu_id) {
    if (cpu_id >= MAX_CPUS) {
        return;
    }

    struct per_cpu_data* cpu = &per_cpu[cpu_id];

    // Initialize basic fields
    cpu->cpu_id = cpu_id;
    cpu->online = true;
    cpu->current_task = NULL;
    cpu->kernel_stack = NULL;  // TODO: Allocate stack
    cpu->idle_task = NULL;
    cpu->ticks = 0;
    cpu->context_switches = 0;
    cpu->interrupts_handled = 0;
    cpu->ipis_received = 0;
    cpu->tlb_flushes = 0;

    // Initialize run queue
    list_init(&cpu->run_queue);

    // Initialize trace buffer
    cpu->trace.head = 0;
    cpu->trace.tail = 0;
    atomic_init(&cpu->trace.overflow, 0);

    // Slab cache will be initialized later when memory management is ready
    cpu->slab_cache = NULL;
}

// Trace an event (lock-free, safe from interrupt context)
void trace_event(enum trace_event_type type, uint64_t d0, uint64_t d1,
                 uint64_t d2, uint64_t d3) {
    struct per_cpu_data* cpu = this_cpu();
    struct trace_buffer* trace = &cpu->trace;

    // Get next write position
    uint32_t head = trace->head;
    uint32_t next = (head + 1) % TRACE_BUFFER_SIZE;

    // Check for overflow
    if (next == trace->tail) {
        atomic_inc(&trace->overflow);
        return;  // Buffer full, drop event
    }

    // Write event
    struct trace_event* event = &trace->events[head];
    event->timestamp = hal->timer_read_tsc();
    event->cpu_id = cpu->cpu_id;
    event->event_type = type;
    event->data[0] = d0;
    event->data[1] = d1;
    event->data[2] = d2;
    event->data[3] = d3;

    // Commit write
    mb();  // Memory barrier
    trace->head = next;
}

// Read trace events (for debugging tools)
int trace_read(uint32_t cpu_id, struct trace_event* events, size_t count) {
    if (cpu_id >= MAX_CPUS || !per_cpu[cpu_id].online) {
        return -EINVAL;
    }

    struct trace_buffer* trace = &per_cpu[cpu_id].trace;
    size_t read = 0;

    while (read < count && trace->tail != trace->head) {
        events[read] = trace->events[trace->tail];
        trace->tail = (trace->tail + 1) % TRACE_BUFFER_SIZE;
        read++;
    }

    return read;
}

// Schedule work on a specific CPU (will be used for IPI-based work)
void schedule_work_on_cpu(uint32_t cpu_id, struct work_item* work) {
    // TODO: Implement work queue
    // For now, this is a stub
    (void)cpu_id;
    (void)work;
}

// Process pending work on this CPU
void process_pending_work(void) {
    // TODO: Implement work queue processing
}
