# Kernel C Style and Rules

This is the rulebook for writing kernel‑grade C in this project.

You are not writing “normal C application code”. You are writing **microkernel C**:

- Hard real‑time aware
- Capability‑based
- Portable across architectures
- Future‑proof for Rust and formal verification

This document pulls together:

- A strict C subset
- Memory and ownership rules
- Concurrency and synchronization discipline
- Module and header structure
- Error handling rules
- Testing and verification expectations
- Style and naming conventions

---

## 1. Philosophy

The kernel must be simple. It must never be clever.

- **Correctness > performance**  
  Optimize only after profiling and only in hot paths.
- **Determinism > convenience**
- **Simplicity > generality**
- **Explicit > implicit**
- **Composition > inheritance/emulation**

Golden mindset:

- Never write code you don’t fully understand.
- No “magic”: if you can’t explain it to a sleep‑deprived engineer at 03:00, don’t commit it.

---

## 2. Language Subset (Allowed C Dialect)

We compile with `-std=gnu99` but treat the language as a strict C99/C11 subset.

### 2.1 Allowed C standard features

- C99:
  - Fixed‑width integer types (`stdint.h`).
  - `_Bool` / `bool` (`stdbool.h`).
  - `static inline` functions.
  - Designated initializers.
- Selected C11 features where useful:
  - `_Atomic` (or compiler built‑ins) for atomic operations.
  - `restrict` where meaningful for performance (optional, not required).

No C++ in the kernel. No mixing C/C++.

### 2.2 Forbidden constructs

- ❌ Variable‑length arrays (VLA).
- ❌ `alloca` and any dynamic stack allocation.
- ❌ Recursion in kernel core (scheduler, IPC, memory, HAL, drivers).
- ❌ `setjmp` / `longjmp` / exception‑like control flow.
- ❌ Function pointers from untrusted context (no “plug in arbitrary user code”).
- ❌ `goto` jumping into different scopes (only use for local cleanup).
- ❌ Floating point (`float`, `double`, `long double`) in kernel logic.
- ❌ Global mutable state without clear synchronization.
- ❌ Inline assembly outside `arch/` (and related headers) unless documented and absolutely necessary.
- ❌ Unions for type‑punning (only allowed for documented hardware register layouts).
- ❌ Macros that hide control flow (`if`, `for`, `while`, `return`).

### 2.3 Strongly discouraged (needs justification)

- ⚠ Pointer casts (beyond `void*` and byte views).
- ⚠ Raw pointer arithmetic that isn’t confined to well‑defined ranges.
- ⚠ Heavy preprocessor metaprogramming.
- ⚠ Giant functions (> 80 lines) or deeply nested control flow.
- ⚠ Giant structs with unclear ownership semantics.
- ⚠ Bitfields (except for hardware registers, clearly commented).

### 2.4 No libc in the kernel

- No `malloc`, `free`, `printf`, `sprintf`, `strcpy`, `strcat`, `strlen`, etc. from the host C library.
- All “standard” functionality must go through our `lib/` implementations (e.g., `memcpy`, `memset`, `strlcpy`, `strlcat`, bounded `snprintf` implementations).

---

## 3. Memory Safety and Ownership

Memory bugs kill kernels. Ownership must be explicit and simple.

### 3.1 Ownership is explicit and documented

Every allocation must have:

- An **owner** (module or unit).
- A clearly defined **lifetime**.
- A clearly defined **freeing responsibility**.

Document this in comments above the API:

```c
// alloc_buffer: Allocates a buffer owned by caller. Caller must free with free_buffer().
// Returns NULL on failure.
```

### 3.2 Allocation rules

- All dynamic allocations go through kernel allocators:
  - `kalloc()`, `kfree()`
  - `kmalloc()`, `kfree()`
  - `pmm_alloc_frame()`, `pmm_free_frame()`
- No raw `malloc`/`free`.
- No dynamic allocation in:
  - Interrupt handlers
  - Scheduler
  - Fast syscall paths
  - IPC hot paths
- Allocations are allowed:
  - During boot and initialization.
  - In non‑critical subsystems (e.g., debug tools) where latency is not part of RT constraints.
- Memory allocated in one module must not be freed by another unless the API explicitly transfers ownership.

### 3.3 Out‑of‑memory (OOM) is always handled

There is no magical kernel OOM killer in a microkernel.

- Every allocator call must check for `NULL`.
- Functions must return error codes instead of assuming allocation success.
- Callers must propagate or handle allocation failures explicitly.

### 3.4 Bounded stack usage

Stack is small and precious.

- Avoid large stack arrays; use heap or static buffers instead.
- No recursion (yes, again).
- Target: stack usage per function is small (< 2 KB), and obvious from code.

### 3.5 Access helpers

Where possible:

- Encapsulate page‑table manipulation, MMIO, shared memory, and ring buffer access in dedicated helpers instead of open‑coded pointer arithmetic.

---

## 4. Concurrency and Real‑Time Rules

These complement `docs/RT_CONSTRAINTS.md` and focus on how they affect C code.

### 4.1 Allowed synchronization primitives

- Spinlocks (with clear scope and duration).
- Mutexes / sleeping locks (in non‑interrupt contexts).
- Interrupt disable/enable (`irq_disable`/`irq_restore`) for **very short** critical sections.
- Atomic operations:
  - `_Atomic` where appropriate.
  - Compiler built‑ins (`__sync_*` / `__atomic_*`) wrapped in `atomic_t` helpers.

### 4.2 Forbidden synchronization patterns

- ❌ Lock‑free algorithms that are not carefully reviewed and documented.
- ❌ Ad‑hoc memory barriers or inline assembly fences in random places.
- ❌ Nested locks without a clearly documented global lock ordering.
- ❌ Disabling interrupts for longer than a few dozen cycles in RT paths.

### 4.3 Lock ordering

Define and respect a global lock hierarchy (example, to be refined as subsystems appear):

1. `pmm_lock`
2. `vm_lock`
3. `scheduler_lock`
4. `unit_table_lock`
5. `channel_locks`

Any violation of lock ordering is a bug and should be rejected in review.

### 4.4 RT‑critical vs non‑critical code

In RT‑critical paths (interrupts, scheduler, IPC hot paths, core syscalls):

- No dynamic allocation.
- No unbounded loops over global lists.
- No sleeping or blocking waits.
- No I/O operations that can stall (disk, network, slow MMIO).

Interrupt handlers must:

- Do minimal work (acknowledge IRQ, update per‑CPU state, enqueue work).
- Defer heavy work to threads or units via work queues or channels.

---

## 5. Headers, Modules, and Architecture

### 5.1 Subsystem structure

Each subsystem should have:

- One public header (`include/kernel/<subsystem>.h` or `include/drivers/<name>.h`).
- One or a small number of C files implementing the subsystem (`core/`, `mm/`, `drivers/`, etc.).

Example layout (conceptual):

```text
kernel/
  mm/
    pmm.h
    pmm.c
    vm.h
    vm.c
  sched/
    sched.h
    sched.c
  unit/
    unit.h
    unit.c
  ipc/
    chan.h
    chan.c
  hal/
    hal.h
    hal.c
```

### 5.2 Public vs internal symbols

- Headers expose **only**:
  - Types and constants needed by other subsystems.
  - Function prototypes that form the subsystem’s API.
- Everything else is `static` in C files or marked internal.

### 5.3 No cyclic dependencies

Layering must remain clean:

- `arch/` → HAL → core kernel → subsystems → userland ABI interface.
- Subsystems must not form cycles via headers or link‑time dependencies.

### 5.4 Documentation for exported functions

Every exported function must have a header comment describing:

- Purpose.
- Inputs and outputs.
- Ownership rules (who owns returned objects, who frees them).
- Error codes and semantics.

---

## 6. Error Handling Rules

### 6.1 Return codes

- Core kernel functions:
  - Return `0` on success.
  - Return negative errno on failure (`-EINVAL`, `-ENOMEM`, `-EIO`, etc.).
- Higher‑level wrappers may map these into more descriptive enums (e.g., `K_ERR_*`) if that improves clarity.

Avoid:

- Silent failures.
- Swallowing errors without logging in early bring‑up builds.

### 6.2 When it is acceptable to panic

Kernel panic is reserved for:

- Corrupted kernel memory or invariants that must never be violated.
- Irrecoverable hardware errors.
- Broken page tables or unsafe execution state.
- Double fault / triple fault conditions.

For all other cases, return an error and let callers or supervisors decide.

### 6.3 `goto` for cleanup

`goto` is acceptable for structured cleanup:

```c
int foo(void) {
    int rc = 0;

    rc = step1();
    if (rc < 0) goto out;

    rc = step2();
    if (rc < 0) goto out;

out:
    cleanup();
    return rc;
}
```

Do not use `goto` to create spaghetti control flow.

---

## 7. Testing, Debug, and Verification

### 7.1 Subsystem self‑tests

Each critical subsystem should provide self‑tests, especially:

- Physical memory manager (PMM).
- Virtual memory / VM mapping logic.
- Scheduler and context switching.
- IPC channels and capability passing.
- Unit lifecycle.

Tests can run:

- At boot (under a “boot selftest” flag or build option).
- From a dedicated test unit in user space.

### 7.2 Assertions

- Use assertions sparingly, for:
  - Invariants that “must never happen”.
  - Internal consistency checks.
- Do **not** use assertions for:
  - Input validation from untrusted sources.
  - Expected error conditions.

Assertions must not have side effects.

### 7.3 Static analysis and tooling

Over time, we aim to run:

- `clang-tidy` / `cppcheck` / `sparse` on kernel code.
- Coccinelle scripts for common bug patterns (optional but encouraged).

### 7.4 Formal verification targets

Where feasible, formally model and/or verify:

- Capability table invariants.
- Scheduler state transitions.
- VM mapping invariants.
- IPC channel queue logic.

Tools could include: TLA+, Coq, CBMC, or other model checkers for small core algorithms.

---

## 8. Style and Naming

### 8.1 Functions

- Naming:
  - `subsystem_action()` or `prefix_action()` style.
  - Use lowercase with underscores.

Examples:

- `sched_pick_next()`
- `unit_create()`
- `vm_map()`
- `chan_send()`

### 8.2 Types

- Use `_t` suffix for typedef‑ed structs and enums:

```c
typedef struct thread thread_t;
typedef struct unit   unit_t;
typedef enum sched_class sched_class_t;
```

Examples:

- `thread_t`
- `unit_t`
- `addr_space_t`

### 8.3 Constants and macros

- ALL_CAPS_WITH_UNDERSCORES for constants:
  - `K_PAGE_SIZE`
  - `K_ERR_INVALID`
  - `HAL_PAGE_PRESENT`

### 8.4 File size and responsibility

- Aim for:
  - Files < 1000 lines.
  - Functions < 50 lines in core paths.
- One main responsibility per file; avoid “kitchen‑sink” modules.

---

## 9. Practical Checklist for New Kernel C Code

Before adding or modifying kernel C code, verify:

**Language & libs**

- [ ] No use of libc beyond our `lib/` implementations.
- [ ] No VLAs, `alloca`, `setjmp`, `longjmp`, or exception‑like control flow.

**Memory & stack**

- [ ] No dynamic allocation in interrupt/scheduler/IPC hot paths.
- [ ] Stack usage is predictable and bounded (no recursion, no large stack arrays).

**Concurrency & RT**

- [ ] No sleeping or blocking in interrupts.
- [ ] Atomic operations and barriers use helpers from `include/kernel/types.h`.
- [ ] Any loop in a critical path has a clear, small bound.
- [ ] Execution time respects RT constraints in `docs/RT_CONSTRAINTS.md`.

**Strings & buffers**

- [ ] Only bounded string functions (`strlcpy`, `strlcat`, bounded `snprintf`).
- [ ] Buffer lengths are always passed and checked.

**Pointers & aliasing**

- [ ] No unsafe cross‑type pointer casting.
- [ ] Any `volatile` is used only for MMIO or special cases (not for locking).

If in doubt, choose the simpler, more explicit solution, even if it adds a few more lines.

