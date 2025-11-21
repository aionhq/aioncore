# Current Work Status

**Last Updated:** 2025-11-21 (Timer implementation complete)
**Phase:** Phase 2 - Basic Memory & Interrupts
**Status:** 🔨 IN PROGRESS (Timer ✅, PMM next)

---

## Quick Links

- 📖 [Full Documentation Index](docs/DOCS.md)
- 🎯 [Vision & Long-term Goals](docs/VISION.md)
- 🗺️ [Implementation Roadmap](docs/IMPLEMENTATION_ROADMAP.md)
- 🐛 [Known Issues](docs/ISSUES.md)

---

## What We Just Completed ✅

### Phase 2.1: Timer Implementation (COMPLETE)

**Completed:** 2025-11-21

**What was built:**
1. ✅ PIT (Programmable Interval Timer) driver (`arch/x86/timer.c`)
   - Initialize PIT at configurable frequency (1000 Hz default)
   - Calibrate TSC (Time Stamp Counter) against PIT
   - Provide microsecond-precision timing via `timer_read_us()`
   - Timer interrupt handler (<100 cycles, RT compliant)

2. ✅ Unit Testing Framework (`include/kernel/ktest.h`, `core/ktest.c`)
   - Test registration via `.ktests` linker section
   - Assertion macros (ASSERT, ASSERT_EQ, ASSERT_NULL, etc.)
   - Test runner with colored output
   - Build system integration (`make test`)
   - Example tests for string library and timer

**Files created:**
- `include/kernel/timer.h` - Timer API
- `arch/x86/timer.c` - PIT + TSC implementation
- `include/kernel/ktest.h` - Test framework API
- `core/ktest.c` - Test runner implementation
- `lib/string_test.c` - String library tests
- `arch/x86/timer_test.c` - Timer tests
- `docs/TESTING.md` - Testing guide
- `.claude.md` - Development workflow rules

**Integration:**
- Timer integrated into HAL (`hal->timer_init`, `hal->timer_read_us`, `hal->timer_read_tsc`)
- Linker script updated with `.ktests` section
- Makefile updated with `KERNEL_TESTS=1` flag and `make test` target

**Reference:** [docs/TESTING.md](docs/TESTING.md), [Phase 2.4 Details](docs/IMPLEMENTATION_ROADMAP.md#24-timer-support)

---

### Phase 1: Foundation & HAL (COMPLETE)

**Completed:** 2025-11-21

**What was built:**
1. ✅ HAL abstraction layer (`include/kernel/hal.h`, `arch/x86/hal.c`)
   - CPU operations (init, halt, features)
   - Interrupt control (enable/disable/restore)
   - I/O operations (inb/outb)
   - Memory operations (TLB flush)
   - Timer operations (TSC read)

2. ✅ Per-CPU infrastructure (`core/percpu.c`, `include/kernel/percpu.h`)
   - Per-CPU data structures (cache-line aligned)
   - Lock-free tracing system
   - Per-CPU statistics

3. ✅ IDT and interrupt handling (`arch/x86/idt.c`, `arch/x86/idt_asm.s`)
   - 256-entry IDT (exceptions + IRQs)
   - PIC remapping (IRQs 0-15 → INT 32-47)
   - Exception handlers with register dumps
   - IRQ handler framework

4. ✅ Modular VGA driver (`drivers/vga/`)
   - VGA text mode with ops table abstraction
   - Color support, scrolling, cursor management
   - kprintf with format specifiers (%d, %u, %x, %08x, %s, %c, %%)

5. ✅ Safe string library (`lib/string.c`)
   - strlcpy, strlcat (always null-terminate)
   - memset, memcpy, strlen

**Critical fixes applied:**
- Fixed interrupt frame struct to match assembly push order
- Integrated IDT into HAL (hal->irq_register now functional)
- HAL cpu_init calls idt_init automatically

**Reference:** [Phase 1 Details](docs/IMPLEMENTATION_ROADMAP.md#phase-1-foundation--hal-week-1-2--done)

---

## What We're Working On Now 🔨

### Phase 2: Basic Memory & Interrupts (Week 3-4)

**Started:** 2025-11-21
**Target Completion:** ~1 week

**Current Task:** Physical Memory Manager (PMM)

### 2.2 Physical Memory Manager (NEXT UP)

**Plan:**
- Bitmap-based frame allocator
- O(1) allocation with free lists
- Track frame ownership for unit accounting
- Reserve kernel regions

**Files to create:**
- `mm/pmm.c` - Physical memory manager
- `include/kernel/pmm.h` - PMM interface

**Reference:** [Phase 2 PMM Details](docs/IMPLEMENTATION_ROADMAP.md#21-physical-memory-manager-simple-bitmap)

### 2.3 Basic Paging (TODO - After PMM)

**Plan:**
- x86 page table management
- Create/destroy address spaces
- Map/unmap pages
- Switch address spaces (CR3)

**Files to create:**
- `arch/x86/mmu.c` - x86 paging implementation
- `include/kernel/mmu.h` - MMU interface

**Design note:** `struct page_table*` will become each unit's `addr_space_t`.

**Reference:** [Phase 2 Paging Details](docs/IMPLEMENTATION_ROADMAP.md#22-basic-paging)

---

## What's Coming Next 📋

### Phase 3: Tasks, Scheduling & Syscalls (Week 5-6)

**Not started yet - Depends on Phase 2 completion**

**Will implement:**
1. Task/thread structures
2. Fixed-priority scheduler (O(1))
3. Context switching (assembly)
4. Syscall entry/exit mechanism
5. First userspace task (ring3)
6. Minimal unit abstraction
7. Kernel channels (messaging primitive)

**Reference:** [Phase 3 Details](docs/IMPLEMENTATION_ROADMAP.md#phase-3-tasks-scheduling--syscalls-week-5-6)

### Phase 4: IPC & Capabilities (Week 7-8)

**Not started yet - Depends on Phase 3 completion**

**Will implement:**
1. Capability system
2. IPC message passing
3. Priority inheritance
4. Syscalls for IPC and capabilities

**Reference:** [Phase 4 Details](docs/IMPLEMENTATION_ROADMAP.md#phase-4-ipc--capabilities-week-7-8)

---

## Known Issues 🐛

### High Priority (Blocking Phase 2)

None currently - Phase 1 issues resolved.

### Medium Priority (Will address in Phase 2/3)

1. **GDT Not Initialized** [Issue #3](docs/ISSUES.md#3--gdt-not-initialized)
   - Currently relying on GRUB's GDT
   - Need to set up our own GDT for TSS/syscalls
   - **Action:** Implement in Phase 2 alongside timer

2. **Timer Semantics Unclear** [Issue #4](docs/ISSUES.md#4--timer-semantics-unclear)
   - `timer_read_us()` returns TSC, not microseconds
   - **Action:** Fix by implementing PIT calibration (current task)

3. **Assert Infrastructure Missing** [Issue #7](docs/ISSUES.md#7--assert-infrastructure)
   - Need runtime assertions for invariant checking
   - **Action:** Add in Phase 3 when implementing scheduler

**Full issue list:** [docs/ISSUES.md](docs/ISSUES.md)

---

## Development Notes

### Build & Test

```bash
# Clean build
make clean && make

# Run in QEMU
make run

# Expected output:
# - Green: "AionCore v0.1.0"
# - Cyan: "RT Microkernel - Phase 2"
# - White: Initialization messages
# - Memory layout info
# - "Entering idle loop..."
```

### File Structure

```
kernel/
├── arch/x86/          # x86-specific code
│   ├── boot.s         # Multiboot entry
│   ├── hal.c          # HAL implementation
│   ├── idt.c          # Interrupt handling
│   ├── idt_asm.s      # Interrupt stubs
│   └── linker.ld      # Memory layout
├── core/              # Architecture-neutral core
│   ├── init.c         # Kernel entry
│   └── percpu.c       # Per-CPU data
├── drivers/vga/       # VGA driver
├── lib/               # Kernel library
├── mm/                # Memory management (coming in Phase 2)
├── include/           # Public headers
│   ├── kernel/        # Core headers
│   └── drivers/       # Driver interfaces
└── docs/              # Documentation
    ├── DOCS.md        # Documentation index
    ├── VISION.md      # Long-term vision
    ├── IMPLEMENTATION_ROADMAP.md  # Phase-by-phase plan
    ├── UNITS_ARCHITECTURE.md      # Units model details
    ├── RT_CONSTRAINTS.md          # Real-time requirements
    ├── FORMAL_VERIFICATION.md     # Verification strategy
    ├── MULTI_ARCH.md              # Multi-arch support
    └── ISSUES.md                  # Issue tracking
```

### Coding Guidelines

**Follow these principles (from [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)):**

- ✅ Small functions (<50 LOC)
- ✅ No undefined behavior (see [docs/KERNEL_C_STYLE.md](docs/KERNEL_C_STYLE.md))
- ✅ Bounded execution time (O(1) in RT paths)
- ✅ No dynamic allocation in critical paths
- ✅ Document invariants
- ✅ Use safe string functions only
- ✅ Keep arch-specific code in `arch/`
- ✅ All hardware access via HAL

For detailed rules about the allowed C subset, forbidden features (VLAs, recursion, `setjmp`/`longjmp`, unsafe string functions, etc.), and concurrency/RT constraints at the C level, see:

- 📘 [Kernel C Style and Rules](docs/KERNEL_C_STYLE.md)

**RT Constraints (from [docs/RT_CONSTRAINTS.md](docs/RT_CONSTRAINTS.md)):**

| Operation | Max Time | Variance |
|-----------|----------|----------|
| Context switch | <200 cycles | <10 cycles |
| Scheduler pick | <100 cycles | <5 cycles |
| IRQ dispatch | <100 cycles | <10 cycles |

---

## Daily Workflow

### Starting Work

1. Read this file (`CURRENT_WORK.md`)
2. Check [docs/IMPLEMENTATION_ROADMAP.md](docs/IMPLEMENTATION_ROADMAP.md) for current phase details
3. Check [docs/ISSUES.md](docs/ISSUES.md) for blockers

### During Work

1. Follow API signatures from roadmap
2. Follow RT constraints from [docs/RT_CONSTRAINTS.md](docs/RT_CONSTRAINTS.md)
3. Document any issues in [docs/ISSUES.md](docs/ISSUES.md)

### After Completing Work

1. Update this file (`CURRENT_WORK.md`) - move completed items to "What We Just Completed"
2. Update [docs/IMPLEMENTATION_ROADMAP.md](docs/IMPLEMENTATION_ROADMAP.md) - mark milestones complete
3. Update [docs/ISSUES.md](docs/ISSUES.md) - close fixed issues
4. Commit with clear message

---

## Questions?

- **What's the big picture?** → Read [docs/VISION.md](docs/VISION.md)
- **What's the plan?** → Read [docs/IMPLEMENTATION_ROADMAP.md](docs/IMPLEMENTATION_ROADMAP.md)
- **Why this design?** → Read [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- **How do units work?** → Read [docs/UNITS_ARCHITECTURE.md](docs/UNITS_ARCHITECTURE.md)
- **What are the RT requirements?** → Read [docs/RT_CONSTRAINTS.md](docs/RT_CONSTRAINTS.md)
- **What needs fixing?** → Read [docs/ISSUES.md](docs/ISSUES.md)
- **Where are all the docs?** → Read [docs/DOCS.md](docs/DOCS.md)

---

## Immediate Next Actions

**Today (2025-11-21):**

1. ✅ Reorganize documentation into `docs/`
2. ✅ Create `CURRENT_WORK.md`
3. 🔨 Implement PIT timer driver
   - Create `arch/x86/timer.c`
   - Initialize PIT at 1000 Hz
   - Calibrate TSC
   - Wire to timer IRQ (32)
4. 📋 Test timer in idle loop
5. 📋 Update docs when complete

**This Week:**
- Complete timer implementation
- Implement PMM (physical memory manager)
- Start basic paging

**This Sprint (2 weeks):**
- Complete Phase 2 (memory + interrupts + timer)
- Start Phase 3 (tasks + scheduler)

---

**Keep this file updated daily!**
