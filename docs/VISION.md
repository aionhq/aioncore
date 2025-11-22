# Kernel Vision and Surface Design

This document describes the *conceptual* surface of the kernel: the minimal set of primitives the kernel should provide so that all higher-level features (POSIX processes, containers, RT workloads, clusters, AI runtimes, etc.) can be built in user space without changing the kernel ABI every time.

It is intentionally broader than the current implementation. Some parts are core and near‑term; others are aspirational or experimental.

---

## 0. Design Goal for the Kernel Surface

**Objective:** Keep the kernel core *small, generic, and stable*, so that user space can implement:

- Process models (POSIX, actors, green threads, etc.)
- Containers / jails and other isolation schemes
- Virtual machines / microVMs
- Cluster / P2P behaviors
- Real‑time scheduling policies
- Security and governance policies
- Observability, tracing, and hook systems

without having to extend or fork the kernel ABI for each idea.

To achieve this, the kernel defines a small set of *first‑class concepts* and groups of operations. Everything else is composition in user space.

---

## 1. Execution Primitives

### 1.1 Units (Execution Domains) – *Core, near‑term*

**Concept:** A *unit* is the fundamental execution entity. It owns:

- An address space
- A capability table
- A set of threads
- Metadata: labels, scheduling class, hints, identity

Conceptual kernel operations:

- `unit_create(spec, parent_cap) -> unit_handle`
- `unit_destroy(unit_handle)`
- `unit_get_info(unit_handle) -> info`
- `unit_set_attr(unit_handle, attrs)`  (labels, hints, RT profile, etc.)

User space can then model:

- POSIX processes
- Containers (unit + namespaces + resource constraints)
- System services and daemons
- VMs (unit that owns a set of virtual devices)

### 1.2 Threads – *Core, near‑term*

Threads always live inside a unit.

Conceptual operations:

- `thread_create(unit_handle, entry_point, stack_spec, attrs) -> thread_handle`
- `thread_exit()`
- `thread_yield()`  (cooperative yield)
- `thread_set_attr(thread_handle, attrs)`  (priority, RT params, affinity)
- `thread_get_info(thread_handle)`

User space builds:

- POSIX threads
- Green threads / actors
- User‑space cooperative schedulers

---

## 2. Memory and Address Spaces

### 2.1 Virtual Memory and Regions – *Core, near‑term*

The kernel owns page tables and memory regions. Units see abstract virtual address spaces.

Conceptual operations:

- `vm_map(unit_handle, region_spec, backing, perms) -> region_handle`
  - `backing`: anonymous, file, device, shared region
- `vm_unmap(unit_handle, region_handle)`
- `vm_protect(unit_handle, region_handle, new_perms)`
- `vm_query(unit_handle, addr) -> region_info`

User space builds:

- `malloc`/arena allocators, GC, region allocators
- User‑level paging tricks (e.g. copy‑on‑write schemes)

### 2.2 Shared Regions – *Recommended, can be phased in*

Controlled sharing of memory between units.

- `vm_share(region_handle, to_unit, perms)`

Enables:

- Zero‑copy IPC
- Shared model weights / tensors
- High‑speed IO buffers

---

## 3. Capabilities and Security

### 3.1 Capability Handles – *Core, foundational*

Everything that can be touched is a capability:

- Channels
- Timers
- Files / volumes
- Devices
- Network endpoints
- Hooks / tracing registrations
- Cluster objects (remote endpoints, exported capabilities)

Conceptual operations:

- `cap_grant(from_unit, to_unit, cap_handle, rights)`
- `cap_revoke(cap_handle)`  (global or scoped)
- `cap_derive(cap_handle, restricted_rights) -> child_cap`
- `cap_query(cap_handle) -> meta`

The kernel enforces access solely via capabilities; there are no implicit global permissions.

### 3.2 Identity Primitives – *Useful, can be minimal at first*

The kernel does not implement full authentication, but it should expose:

- Unit identity (opaque ID, potentially with cryptographic backing)
- Node identity (for cluster / P2P protocols)

Conceptual operations:

- `unit_get_identity(unit_handle) -> identity_blob`
- `node_get_identity() -> identity_blob`

User space can bind these to mTLS, JWT, SPIFFE‑like IDs, etc.

---

## 4. IPC and Communication

### 4.1 Channels (Message‑based IPC) – *Core, near‑term*

Message passing is the basic communication primitive; shared state is layered on top as a choice, not a requirement.

Conceptual operations:

- `chan_create(spec) -> chan_handle`
- `chan_send(chan_handle, msg_ptr, msg_len, flags)`
- `chan_recv(chan_handle, buf_ptr, buf_len, flags) -> msg_len`
- `chan_set_attr(chan_handle, attrs)`  (queue depth, policies)
- `chan_get_info(chan_handle)`

Properties:

- Bounded queues → built‑in backpressure
- Optional priorities
- Drop policies and timeouts

User space builds:

- Pipes, Unix‑style sockets, RPC systems
- Actor mailboxes and message buses
- Cluster messaging layers

### 4.2 Events and Notifications – *Core, but minimal*

A small event primitive for async behavior:

- `event_subscribe(target, mask) -> event_handle`
- `event_wait(event_handle, timeout) -> events`
- `event_ack(event_handle, event_id)`

Used for:

- Timers
- IO readiness
- Unit lifecycle signals
- Cluster membership changes

---

## 5. Time and Timers – *Core for RT*

Real‑time and scheduling experiments need good time primitives.

Conceptual operations:

- `time_get(clock_id) -> time`
  - `clock_id`: monotonic, realtime, per‑CPU, etc.
- `timer_create(clock_id, spec) -> timer_handle`
- `timer_arm(timer_handle, deadline, mode)`  (one‑shot, periodic)
- `timer_cancel(timer_handle)`

User space builds:

- Precise RT schedulers
- Reactor loops and event loops
- Timeout‑based protocols
- Cron‑style or workflow services

---

## 6. Scheduling and Real‑Time Hooks

We do not hard‑code all scheduling policies, but we expose knobs and observability.

Conceptual operations:

- `sched_set_unit_params(unit_handle, class, prio, rt_params)`
- `sched_set_thread_params(thread_handle, class, prio, rt_params)`
- `sched_query(unit_handle/thread_handle) -> stats`  (runtime, wait time, etc.)

Scheduling classes might include:

- `RT_FIXED`, `RT_EDF`, `BEST_EFFORT`, `BACKGROUND`

The kernel also defines which syscalls are RT‑safe and has clear upper bounds on syscall duration, so RT workloads can rely on them.

User space can implement:

- Custom schedulers and control planes
- Cluster‑aware scheduling
- Policy engines that react to `sched_query` data

---

## 7. Devices and IO Queues

### 7.1 Device Abstractions – *Core, but initially minimal*

Devices are exposed as capability‑backed IO endpoints, not global globals.

Conceptual operations:

- `device_list() -> [device_info]`
- `device_open(device_id, mode) -> dev_cap`
- `device_io(dev_cap, io_request_struct)`
- Optionally: `ioqueue_create(dev_cap, spec) -> ioq_handle` and `ioq_submit` / `ioq_complete`

User‑space drivers (where possible) use this to implement:

- Filesystems
- Network stacks
- GPU runtimes
- Storage layers
- Cluster filesystems

Some minimal in‑kernel drivers remain for critical, latency‑sensitive or bootstrap devices.

---

## 8. Namespaces and Resource Views – *Useful for containers*

To support containers and jails, we need namespace controls.

Conceptual operations:

- `ns_create(spec) -> ns_handle`
- `ns_attach(unit_handle, ns_handle, ns_type)`  (fs, pid, net, ipc, etc.)
- `ns_get_info(ns_handle)`

The meaning and policies of these namespaces (mount trees, network layout, etc.) are largely implemented by a user‑space unit manager.

---

## 9. Cluster and P2P Surface – *Advanced / experimental*

To support “many machines as one entity”, we define minimal primitives for clustering.

### 9.1 Node and Overlay

Conceptual operations:

- `cluster_get_node_info() -> node_info`
- `cluster_list_nodes() -> [node_info]`
- `cluster_subscribe_events() -> event_handle`  (join/leave/fault)
- `cluster_open_remote_endpoint(node_id, spec) -> remote_cap`

The secure overlay (crypto identities, encrypted transport) is a kernel networking feature; cluster policies (who may join, membership rules) are user‑space concerns.

### 9.2 Remote Capabilities and Migration Hooks

Conceptual operations:

- `cap_export(cap_handle, node_id, scope, ttl) -> token_blob`
- `cap_import(token_blob) -> cap_handle`
- `unit_checkpoint(unit_handle, flags) -> snapshot_handle`
- `unit_restore(snapshot_handle, target_node)`

User space builds:

- Cluster schedulers
- Global filesystems
- Global naming / service discovery
- Unit migration strategies

---

## 10. Hooks, Tracing, and Observability

### 10.1 Hooks – *Optional, high‑leverage*

A minimal hook subsystem for attaching safe programs to kernel events.

Conceptual operations:

- `hook_register(hook_point, prog_handle, filter_spec) -> hook_handle`
- `hook_unregister(hook_handle)`
- `hook_get_stats(hook_handle) -> stats`

Hook points might include:

- Syscall entry/exit
- Channel send/receive
- Scheduler events
- Capability grant/revoke
- Network ingress/egress
- Device IO events

Programs run in a verifiable VM/IR (e.g., eBPF/WASM) managed by a kernel module; loading such programs is a separate concern.

### 10.2 Tracing and Metrics

Conceptual operations:

- `trace_emit(event_type, data_ptr, len)`  (kernel and privileged user space)
- `trace_subscribe(filter) -> event_handle`
- `metrics_get(unit_handle/node) -> metrics_struct`

User space builds:

- Telemetry exporters
- `perf`‑like tools
- ML‑based anomaly detection and SRE tooling

---

## 11. Introspection and Self‑Description

To make the system usable and debuggable, the kernel should describe itself.

Conceptual operations:

- `sys_get_topology() -> cpus, NUMA, devices`
- `sys_list_units(filter) -> [unit_info]`
- `sys_get_logs(cap_handle, range) -> log_data`  (for tamper‑evident logs)
- `sys_get_config_version() -> id`  (to tie into declarative desired‑state systems)

User space maps these into:

- Control planes (e.g., cluster managers)
- UI / dashboards
- Auditing and compliance systems

---

## 12. What the Kernel Deliberately Does Not Own

Intentionally out of kernel scope:

- POSIX process semantics (signals, fork/exec, etc.) – provided by a “POSIX personality” in user space on top of units.
- Container specs (e.g., OCI) – implemented by a unit manager mapping specs to units, namespaces, and capabilities.
- Cluster high‑level APIs (e.g., Kubernetes‑like resources) – provided by user‑space controllers.
- Service mesh / SDN / routing policies – built using channels, hooks, and network/device caps.
- AI runtimes and GPU scheduling policies – user‑space daemons using device capabilities and hooks.

The kernel focuses on a tight set of core concepts:

1. Units (execution domains)
2. Threads (within units)
3. Address spaces and memory regions
4. Capabilities (for all resources)
5. Channels and events (IPC and async)
6. Time and timers
7. Devices and IO queues
8. Namespaces
9. Cluster primitives (node identity, overlay, remote caps, migration hooks)
10. Hooks and telemetry
11. Basic introspection

Everything else is composition and policy in user space.

---

## 13. Relation to Current Code

The current codebase implements only a small subset of this vision:

- Bootstrapping via Multiboot and `_start` (`arch/x86/boot.s`, `arch/x86/linker.ld`).
- A Hardware Abstraction Layer (`include/kernel/hal.h`, `arch/x86/hal.c`) for CPU, interrupts, I/O, and timing.
- Per‑CPU data and tracing (`include/kernel/percpu.h`, `core/percpu.c`).
- IDT and basic exception/IRQ handling (`include/kernel/idt.h`, `arch/x86/idt.c`, `arch/x86/idt_asm.s`).
- A modular VGA subsystem and `kprintf` (`include/drivers/vga.h`, `drivers/vga/vga.c`, `drivers/vga/vga_text.c`).
- A structured initialization path in `core/init.c`.

Units, threads, capabilities, channels, timers, and cluster features are *not implemented yet*; they are design targets that will shape future headers, syscalls, and subsystems as described in `ARCHITECTURE.md` and `IMPLEMENTATION_ROADMAP.md`.

---

## 14. Draft ABI for Units, Threads, Capabilities, and Messaging

This section takes the conceptual surface above and sketches a concrete, C‑style ABI that future code can target. Names and signatures are illustrative and may evolve, but they represent the style and boundaries we want.

### 14.1 Units – What Exactly Is a Unit?

A unit is the only execution container the kernel knows about. It is the basis for:

- POSIX processes
- containers/jails
- services/daemons
- VMs and microVMs

**Internal mental model (not ABI):**

```c
typedef uint64_t unit_id_t;       // globally unique within node
typedef int32_t  unit_handle_t;   // per-unit handle, like an fd

typedef enum {
    UNIT_STATE_CREATED,
    UNIT_STATE_RUNNING,
    UNIT_STATE_STOPPED,
    UNIT_STATE_ZOMBIE,
    UNIT_STATE_FAILED,
} unit_state_t;

struct unit {
    unit_id_t       id;
    unit_state_t    state;

    // Address space
    addr_space_t   *as;

    // Threads
    thread_list_t   threads;

    // Capabilities
    cap_table_t     caps;

    // Namespaces / views
    ns_handle_t     fs_ns;
    ns_handle_t     pid_ns;
    ns_handle_t     net_ns;
    ns_handle_t     ipc_ns;

    // Scheduling & RT
    sched_class_t   sched_class;    // RT_FIXED, RT_EDF, BEST_EFFORT, BACKGROUND
    int             base_priority;
    rt_params_t     rt_params;      // period, deadline, etc.

    // Metadata for policy/scheduling
    label_set_t     labels;         // e.g., "role=web", "tenant=A"
    uint64_t        flags;
};
```

**Unit creation (ABI sketch):**

```c
struct unit_spec {
    uint32_t        version;      // for ABI evolution

    // Binary / entry point
    cap_handle_t    image_cap;    // executable image capability
    const char     *entry_point;  // symbol or offset within image
    const char    *const *argv;
    const char    *const *envp;

    // Initial capabilities to install in child
    const cap_handle_t *initial_caps;
    size_t          num_initial_caps;

    // Initial namespaces (optional)
    ns_handle_t     fs_ns;
    ns_handle_t     pid_ns;
    ns_handle_t     net_ns;
    ns_handle_t     ipc_ns;

    // Scheduling hints
    sched_class_t   sched_class;
    int             base_priority;

    // Labels (policy/scheduling/cluster)
    struct kv_pair *labels;
    size_t          num_labels;

    uint64_t        flags;        // e.g., NO_INHERIT_ENV, NO_DEBUG
};

long sys_unit_create(const struct unit_spec *spec,
                     unit_handle_t          *out_handle);
```

Unit lifecycle:

```c
long sys_unit_start(unit_handle_t uh);
long sys_unit_stop(unit_handle_t uh, int exit_code);
long sys_unit_kill(unit_handle_t uh, int reason); // hard kill

struct unit_info {
    uint32_t        version;
    unit_id_t       id;
    unit_state_t    state;
    uint64_t        flags;

    // Basic stats
    uint64_t        cpu_time_ns;
    uint64_t        user_time_ns;
    uint64_t        mem_bytes;
    uint64_t        start_time_ns;

    // Scheduling
    sched_class_t   sched_class;
    int             base_priority;
};

long sys_unit_get_info(unit_handle_t uh,
                       struct unit_info *out_info);
```

### 14.2 Threads Inside a Unit

Threads are owned by units; the kernel does not impose a specific user‑space threading model.

```c
typedef int32_t thread_handle_t;

struct thread_spec {
    uint32_t        version;
    void          (*entry)(void *arg);   // user-space entry
    void           *arg;
    void           *stack_base;
    size_t          stack_size;

    // Scheduling hints (inherit from unit if omitted)
    sched_class_t   sched_class;
    int             base_priority;

    uint64_t        flags;
};

long sys_thread_create(const struct thread_spec *spec,
                       thread_handle_t          *out_th);
void sys_thread_exit(int exit_code);
long sys_thread_yield(void);

struct thread_info {
    uint32_t        version;
    thread_handle_t self;
    unit_id_t       unit_id;
    sched_class_t   sched_class;
    int             base_priority;
    uint64_t        cpu_time_ns;
    uint64_t        flags;
};

long sys_thread_get_info(thread_handle_t th,
                         struct thread_info *out);
```

On top of this, libraries can implement POSIX `pthread`, green threads, or actor runtimes.

### 14.3 Capabilities – Generalized, Type‑Tagged Handles

Capabilities generalize file descriptors: they are typed, rights‑controlled handles for all resources.

```c
typedef int32_t cap_handle_t;

typedef enum {
    CAP_TYPE_INVALID = 0,
    CAP_TYPE_CHANNEL,
    CAP_TYPE_TIMER,
    CAP_TYPE_DEVICE,
    CAP_TYPE_FILE,
    CAP_TYPE_VOLUME,
    CAP_TYPE_NAMESPACE,
    CAP_TYPE_HOOK,
    CAP_TYPE_REMOTE_ENDPOINT,
    // …
} cap_type_t;

struct cap_info {
    uint32_t   version;
    cap_type_t type;
    uint64_t   rights;       // bitmask: read, write, exec, admin, etc.
    uint64_t   flags;
};

long sys_cap_get_info(cap_handle_t ch, struct cap_info *out);

long sys_cap_grant(unit_handle_t   target_unit,
                   cap_handle_t    cap,
                   uint64_t        rights);

long sys_cap_derive(cap_handle_t   parent_cap,
                    uint64_t       restricted_rights,
                    cap_handle_t  *out_child_cap);

long sys_cap_revoke(cap_handle_t cap);
```

This is enough to implement descriptor passing, reduced‑privilege views (e.g., read‑only), and just‑in‑time privilege delegation in user space.

### 14.4 Messaging Model – Channels Between Units

Message‑based channels are the core IPC primitive. No global shared memory is required for communication; shared memory is an explicit `vm_share`/capability choice.

```c
typedef int32_t chan_handle_t;

typedef enum {
    CHAN_MODE_UNI,   // one-way; distinct send/recv handles
    CHAN_MODE_BIDI,  // two-way; a single handle can send & receive
} chan_mode_t;

struct chan_spec {
    uint32_t    version;
    chan_mode_t mode;

    size_t      max_msg_size;
    size_t      max_queue_len; // messages or bytes

    uint64_t    flags;         // e.g., RELIABLE, RT_SAFE, PRIORITY_AWARE
};

long sys_chan_create(const struct chan_spec *spec,
                     chan_handle_t          *out_local_handle);
```

Message IO descriptor:

```c
struct msg_io {
    void        *buf;        // header + payload
    size_t       buf_len;
    uint64_t     flags;      // e.g., NONBLOCK, PEEK

    // For send: caps to attach
    cap_handle_t *caps;
    size_t        num_caps;

    // For recv: filled in by kernel
    size_t       out_msg_len;
    size_t       out_num_caps;
};

long sys_chan_send(chan_handle_t ch, struct msg_io *msg);
long sys_chan_recv(chan_handle_t ch, struct msg_io *msg,
                   uint64_t timeout_ns);
```

On top of channels, user space can build pipes, sockets, RPC frameworks, streaming protocols, and actor systems.

### 14.5 Events and Async Integration

Events generalize epoll/kqueue to multiple source types (channels, timers, units, cluster).

```c
typedef int32_t event_handle_t;

typedef enum {
    EVENT_SRC_CHAN,
    EVENT_SRC_TIMER,
    EVENT_SRC_UNIT,
    EVENT_SRC_CLUSTER,
    // …
} event_src_type_t;

struct event_spec {
    uint32_t          version;
    event_src_type_t  src_type;
    uint64_t          src_id;      // e.g., chan_handle, unit_id, node_id
    uint64_t          mask;        // readable, writable, closed, etc.
    uint64_t          flags;
};

long sys_event_subscribe(const struct event_spec *spec,
                         event_handle_t          *out_ev);

struct event_record {
    uint32_t          version;
    event_src_type_t  src_type;
    uint64_t          src_id;
    uint64_t          mask;
    uint64_t          timestamp_ns;
    uint64_t          data0;
    uint64_t          data1;
};

long sys_event_wait(event_handle_t ev,
                    struct event_record *records,
                    size_t              max_records,
                    uint64_t            timeout_ns);
```

### 14.6 Versioning and Error Model

To keep the ABI stable over time:

- Every public struct begins with a `version` field. New versions can add fields at the end while remaining backward compatible.
- Syscalls return errno‑style negative error codes, with clear categories:
  - Permission / capability errors (e.g., `-EPERM`, `-EACCES`, `CAP_EXPIRED`)
  - Resource exhaustion (`-ENOMEM`, queue full, etc.)
  - Invalid input / incompatible version (`-EINVAL`)
  - Timeout (`-ETIMEDOUT`)
  - Not found (`-ENOENT`)

Higher‑level protocols can embed richer error semantics in their message payloads.

---

## 15. Rust and Erlang Influences (Concrete Design Steps)

The kernel surface above is intentionally language‑agnostic, but we can explicitly borrow proven ideas from Rust (ownership, Send/Sync) and Erlang (isolated processes, supervision, distribution) to shape how units, capabilities, and channels behave.

### 15.1 Ownership Semantics for Capabilities

**Goal:** Model capabilities as *owned* resources, with explicit “move” vs “borrow” semantics when passing them between units.

Design decisions:

- A capability handle is conceptually a *linear* resource:
  - **Move:** sender loses usable access when the cap is transferred.
  - **Borrow:** sender retains access; receiver gets a restricted view.
  - **Clone:** only allowed if the capability type and rights allow duplication.
- Capability metadata should record:
  - Owning unit (for cleanup on unit exit).
  - Rights (read/write/execute/admin).
  - Transfer policy (move allowed, borrow only, clonable).

Concrete steps:

1. Extend `struct cap_info` (section 14.3) with ownership/transfer metadata:
   - `unit_id_t owner_unit;`
   - `uint32_t transfer_flags;` (e.g., `CAP_TRANSFER_MOVE_ALLOWED`, `CAP_TRANSFER_BORROW_ONLY`, `CAP_TRANSFER_CLONE_ALLOWED`).
2. Extend channel message descriptors (`struct msg_io` in section 14.4) with per‑cap transfer semantics:
   - For example: an array of `(cap_handle_t cap, uint32_t transfer_mode)` entries, where `transfer_mode` ∈ {MOVE, BORROW, CLONE}.
3. Define precise semantics for `sys_cap_grant` and `sys_cap_derive`:
   - `sys_cap_grant(..., rights)` performs a *move* or *borrow* depending on transfer mode and rights.
   - `sys_cap_derive(parent_cap, restricted_rights, &child_cap)` creates a capability with *reduced* rights, suitable for borrowing or delegating.
4. On unit teardown, the kernel walks the unit’s capability table, decrefs/destroys the underlying kernel objects, and closes any associated channels/timers/devices (RAII‑style cleanup).

### 15.2 Send/Sync‑Style Classification of Kernel Objects

**Goal:** Avoid unsafe sharing by classifying which objects may be shared across units/CPUs.

Design decisions:

- Inspired by Rust’s `Send`/`Sync` traits, each capability type is classified as:
  - **SENDABLE:** safe to transfer between units/CPUs (e.g., channels, timers, files, network endpoints).
  - **LOCAL_ONLY:** must remain local to a unit or CPU (e.g., some debug views, raw per‑CPU state).
- The kernel enforces that `LOCAL_ONLY` caps:
  - Cannot be granted to other units.
  - Cannot be used across CPUs if they rely on CPU‑local invariants.

Concrete steps:

1. Add a `concurrency_class` field to `struct cap_info`:
   - e.g., `CAP_CONC_LOCAL_ONLY`, `CAP_CONC_SENDABLE`.
2. When implementing `sys_cap_grant` and channel‑based cap transfer:
   - Reject attempts to transfer `LOCAL_ONLY` capabilities across units or CPUs with `-EPERM`.
3. Document, per capability type, whether it is `LOCAL_ONLY` or `SENDABLE` and under which conditions that can change (e.g., a debug cap only in a special build).

### 15.3 Erlang‑Style Units, Mailboxes, and Supervision

**Goal:** Treat units as Erlang‑style processes: isolated, message‑driven, and supervised by user‑space managers.

Design decisions:

- A unit is an isolated execution domain; the only way to interact with it is via capabilities and channels.
- Each unit can have one or more “mailboxes” (channels) that receive messages.
- When a unit crashes or exits, other units should observe this via events, not by inspecting shared state.

Concrete steps:

1. Define standard unit lifecycle events in the event system (section 14.5):
   - Add an `EVENT_SRC_UNIT` with sub‑types like `UNIT_EVENT_EXIT`, `UNIT_EVENT_CRASH`.
   - Add a small `unit_exit_reason` struct (exit code, signal/reason, timestamp).
2. Use `sys_event_subscribe` to build supervision trees in user space:
   - Supervisors subscribe to events from child units.
   - On `UNIT_EVENT_EXIT`, user space decides whether to restart, escalate, or ignore.
3. Define that the kernel *never* auto‑restarts units. It only:
   - Contains the failure (no state leakage).
   - Reclaims capabilities and resources.
   - Emits the appropriate unit events.
4. Encourage “mailbox” patterns in user space by:
   - Using channels as primary IPC, with simple tagged message headers (e.g., a `type` field in payload).
   - Providing a lightweight pattern‑matching or dispatch layer in libraries (not in the kernel).

### 15.4 Distribution Transparency and Remote Channels

**Goal:** Make local vs remote messaging look the same from the ABI’s perspective, as in Erlang’s node‑transparent messaging.

Design decisions:

- A remote channel / endpoint should still be a `chan_handle_t` or `cap_handle_t` at the ABI level.
- Cluster primitives (section 9) are responsible for:
  - Creating remote endpoints.
  - Handling encryption, routing, and retries.

Concrete steps:

1. Extend `cap_type_t` with `CAP_TYPE_REMOTE_ENDPOINT` (already sketched in 14.3) and define its semantics:
   - Must be SENDABLE.
   - Backed by a kernel‑managed overlay (cluster subsystem).
2. Ensure `sys_chan_send/recv` can operate on both local and remote channels:
   - Same signatures; remote vs local behavior differs only in latency and failure modes.
3. Use the cluster and capability export/import primitives (section 9.2) to:
   - Export a local channel capability to another node.
   - Import it as a `CAP_TYPE_REMOTE_ENDPOINT` in a different unit.

---

## 16. Testing Strategy for Core Primitives

To keep the kernel small and trustworthy, primitives like units, threads, capabilities, and channels must be thoroughly tested. Most testing happens in user space (on QEMU and real hardware), with some optional kernel self‑tests in debug builds.

### 16.1 Testing Philosophy

- Prefer **small, vertical end‑to‑end slices**:
  - Example: PIT timer interrupt → `scheduler_tick()` → context switch between two test threads.
  - Example: in‑kernel channel → syscall wrapper → userspace ping‑pong.
- Keep kernel self‑tests minimal and optional:
  - Run only in a special debug/test build or under a boot flag.
  - Avoid impacting RT guarantees in production builds.

### 16.2 Primitive‑Level Tests

**Physical memory and VM:**

- Boot‑time self‑tests (in test builds):
  - Allocate/free many frames via PMM, check for leaks and double frees.
  - Map/unmap a small set of pages in a test address space, verify reads/writes behave as expected.

**Units and threads:**

- Create a unit with a single thread that:
  - Runs a simple loop, yields (`sys_thread_yield`), and exits with a known code.
  - Kernel or a supervisor unit verifies `sys_unit_get_info` reflects the correct state and exit code.
- Preemptive scheduling tests:
  - Multiple threads with different priorities; verify higher‑priority threads get more CPU and that `scheduler_tick` preempts correctly.

**Capabilities and ownership:**

- Move semantics:
  - Create a capability (e.g., a channel) owned by unit A.
  - Transfer it to unit B with MOVE semantics via a channel message.
  - Verify unit A can no longer use the moved cap; unit B can.
- Borrow semantics:
  - Derive a restricted cap for unit B (e.g., read‑only).
  - Confirm both units can use their respective caps within their rights; operations beyond rights fail with the correct errno.
- Cleanup:
  - Terminate a unit and verify:
    - Its capabilities are reclaimed.
    - Underlying objects (channels, timers) are destroyed or detached correctly.

**Channels and messaging:**

- Ping‑pong:
  - Two units exchange messages via a channel; verify ordering, no loss, and correct timeouts.
- Backpressure:
  - Fill a bounded channel to capacity; ensure senders block or see `EAGAIN` depending on flags.
- Concurrency:
  - Multiple sender threads vs one receiver, and vice versa; verify no deadlocks or corruption under load.

**Events and supervision:**

- Subscribe a supervisor unit to child unit events (using `EVENT_SRC_UNIT`):
  - Kill a child unit, verify that the supervisor receives a `UNIT_EVENT_EXIT` and can restart it.
  - Ensure no spurious events and that repeated failures do not leak resources.

### 16.3 Harness and CI

- Use QEMU‑based test runners to:
  - Boot the kernel into a test mode.
  - Run a suite of userspace test programs that exercise the syscalls for units, threads, caps, and channels.
  - Collect basic timing metrics (e.g., context switch and IPC latency) in debug builds.
- For each new primitive introduced:
  - Add at least one minimal kernel‑level assertion or self‑test.
  - Add at least one user‑space integration test that runs through the public ABI.

This strategy keeps the core primitives honest as the kernel evolves, while respecting the RT and microkernel constraints outlined earlier.

---

## 17. Hardening Profiles and Future Robustness Features

The kernel is designed so that additional robustness can be enabled without changing the ABI or rewriting core subsystems.

### 17.1 Compile-Time Profiles

We use three conceptual profiles:

- **DEV** (`CONFIG_PROFILE_DEV`):
  - Maximum debug and checks.
  - Extra invariants and verbose logging enabled.
- **STANDARD** (`CONFIG_PROFILE_STANDARD`, default):
  - Minimal overhead.
  - Basic safety mechanisms available, heavy ones off by default.
- **HARDENED** (`CONFIG_PROFILE_HARDENED`):
  - Extra robustness features (scrubber, text hashing, watchdog) enabled.
  - Suitable for more demanding environments.

These profiles expand into feature flags via `include/kernel/config.h`, e.g.:

- `CONFIG_ENABLE_DEBUG_ASSERTS`
- `CONFIG_ENABLE_PARANOID_CHECKS`
- `CONFIG_ENABLE_RAM_SCRUBBER`
- `CONFIG_ENABLE_HASH_KERNEL_TEXT`
- `CONFIG_ENABLE_LOG_VERBOSE`

If a feature is not enabled at compile time, its code does not exist in the binary.

### 17.2 Runtime Configuration (`kernel_config_t`)

At runtime, all hardening-related knobs are grouped into a single `kernel_config_t` (see `ARCHITECTURE.md` hardening section). Examples:

- Scrubber: enable/disable and rate (pages per second).
- Invariants: enable/disable and sampling rate (e.g. 1/N operations).
- Messaging: header checks always on; optional payload CRC sampling.
- Watchdog: enabled flag and timeout in ms.
- Kernel text verification: enabled flag and interval in seconds.

Future components (scheduler, IPC, memory scrubber) will read from `kernel_config_t` when deciding how aggressively to apply checks, so the same binary can be tuned for different environments.

### 17.3 Future Hooks (Not Implemented Yet)

Planned but not yet implemented:

- **Scheduler invariants:**
  - Optional, sampled checks in `scheduler_tick()` to validate run queues and priorities.
- **IPC robustness:**
  - Always-on header sanity checks.
  - Optional CRC or hash of payloads on a sampling basis.
- **RAM scrubber:**
  - Background thread/unit that periodically reads or scrubs pages based on configuration.
- **Watchdog:**
  - Kernel-level or management-unit-level watchdog that can reset or log on stalls.
- **Kernel text hashing:**
  - Periodic hashing of kernel text to detect corruption in hardened profiles.

These should all be:

- Compiled in or out by `CONFIG_ENABLE_*`.
- Controlled at runtime by `kernel_config_t`.
- Designed with RT constraints in mind (sampling, bounded work).
