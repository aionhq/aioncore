# Current Work Status

**Last Updated:** 2025-11-22 (Phase 2 COMPLETE - All memory subsystems working)
**Phase:** Phase 3 - Tasks, Scheduling & Syscalls (Next)
**Status:** Phase 2 ✅ COMPLETE | Phase 3 📋 READY TO START

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

### Phase 2.2: Physical Memory Manager (COMPLETE)

**Completed:** 2025-11-21

**What was built:**
1. ✅ PMM with bitmap allocator (`mm/pmm.c`, `include/kernel/pmm.h`)
   - Parses multiboot memory map to discover available RAM
   - Bitmap-based frame allocator (1 bit per 4KB frame)
   - Frame allocation/deallocation (currently O(n), will optimize to O(1) with free lists)
   - Reserved region tracking (kernel, low memory)
   - Statistics API for memory usage monitoring
   - Unit tests for allocation, alignment, uniqueness

**Files created:**
- `mm/pmm.c` - Physical memory manager implementation
- `include/kernel/pmm.h` - PMM API (multiboot structures, allocation functions)
- `mm/pmm_test.c` - PMM unit tests (6 tests covering allocation, freeing, bulk operations)

**Integration:**
- Updated `arch/x86/boot.s` to pass multiboot info to kmain()
- Updated `core/init.c` to call `pmm_init()` after timer initialization
- Updated `arch/x86/linker.ld` with `_kernel_start` and `_kernel_end` symbols
- Added `phys_addr_t` type to `include/kernel/types.h`
- PMM reserves kernel regions and low memory (first 1MB)

**Defensive Programming:**
- Added fallback mode: assumes 128MB RAM if multiboot info invalid
- Validates multiboot magic number and info pointer
- Prints debug messages showing multiboot flags and memory regions
- Gracefully degrades instead of crashing on missing memory map

**Note:** Current implementation uses O(n) bitmap scanning as fallback. Future optimization will add per-CPU free lists for O(1) allocation to meet RT constraints.

**Reference:** [Phase 2.2 Details](docs/IMPLEMENTATION_ROADMAP.md#22-physical-memory-manager-pmm)

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

### Phase 2.3: Basic Paging (MMU) - COMPLETE

**Completed:** 2025-11-22

**What was built:**
1. ✅ MMU with x86 two-level page tables (`arch/x86/mmu.c`, `include/kernel/mmu.h`)
   - Page directory and page table management
   - O(1) map/unmap operations (direct 2-level indexing, no loops)
   - Lazy page table allocation (allocate on first use)
   - Address space creation/destruction/switching
   - Identity mapping for kernel (first 4MB)
   - CR3 management for address space switching
   - TLB invalidation (single-page and full flush)

**Files created:**
- `include/kernel/mmu.h` - Architecture-independent MMU API (154 lines)
- `arch/x86/mmu.c` - x86 two-level paging implementation (326 lines)

**API implemented:**
```c
page_table_t* mmu_create_address_space(void);           // O(1)
void mmu_destroy_address_space(page_table_t* pt);       // O(n) - cleanup only
void* mmu_map_page(page_table_t* pt, phys_addr_t phys,
                   virt_addr_t virt, uint32_t flags);    // O(1), <200 cycles
void mmu_unmap_page(page_table_t* pt, virt_addr_t virt); // O(1), <100 cycles
void mmu_switch_address_space(page_table_t* pt);        // O(1), <50 cycles
page_table_t* mmu_get_current_address_space(void);
page_table_t* mmu_get_kernel_address_space(void);
void mmu_init(void);
```

**Page flags (architecture-independent):**
- `MMU_PRESENT` - Page is present in memory
- `MMU_WRITABLE` - Page is writable
- `MMU_USER` - Page accessible from user mode
- `MMU_NOCACHE` - Disable caching (for MMIO)
- `MMU_EXEC` - Page is executable (reserved for future PAE/NX support)

**Integration:**
- Updated `core/init.c` to call `mmu_init()` after PMM initialization
- Updated `Makefile` to compile `arch/x86/mmu.c`
- Paging enabled via CR0.PG bit in `mmu_init()`
- Kernel address space created with identity-mapped first 4MB

**RT Constraints Enforced:**
- `mmu_map_page()`: O(1) - direct indexing via PD_INDEX/PT_INDEX macros, no loops
- `mmu_unmap_page()`: O(1) - direct indexing, single TLB invalidation
- `mmu_switch_address_space()`: O(1) - single CR3 write
- No global page directory walks or scans
- Allocates at most 1 page table per map operation (lazy allocation)

**Design highlights:**
- Opaque `page_table_t` type for multi-arch compatibility
- Two-level paging: page directory (1024 entries) + page tables (1024 entries each)
- Direct indexing: `PD_INDEX(virt) = (virt >> 22) & 0x3FF`, `PT_INDEX(virt) = (virt >> 12) & 0x3FF`
- Identity mapping for kernel simplifies initial paging setup
- Each unit will get its own `page_table_t*` for address space isolation

**Testing:**
- Kernel boots successfully with paging enabled
- Identity-mapped first 16MB allows kernel/VGA/low memory access
- No page faults during initialization
- Unit tests pending

**Critical Bug Fixed (Post-Phase 2.3):**
- **Bug:** Debug output showed "Page size: 40 bytes" instead of "4096 bytes"
- **Symptom:** All numbers printed with `kprintf("%u", ...)` were truncated to 2 digits
- **Initial investigation (several hours):** Checked for stale binaries, preprocessor issues, PAGE_SIZE definitions, searched for memory corruption
- **Root cause:** `utoa()` function in `drivers/vga/vga.c` had incorrect length calculation
  - After reversing digits, returned `ptr1 - buf` (middle of string) instead of actual length
  - For "4096" (4 chars), returned length=2, causing kprintf to print only "40"
- **Fix:** Save length before reversal: `int len = ptr - buf; ... return len;`
- **How we found it:** Created host-side unit tests (`tests/kprintf_test.c`) that isolated the bug immediately
- **Lesson:** Unit tests catch bugs faster than debugging in QEMU. Test infrastructure pays off!
- **Also fixed:** Same bug in `utoa64()` function
- **Also created:** Host test framework improvements (`tests/test_main.c`, better `host_test.h`)

**Reference:** [Phase 2.3 Details](docs/IMPLEMENTATION_ROADMAP.md#23-basic-paging)

---

## What We're Working On Now 🔨

### Phase 3 Planning: Tasks, Scheduling & Syscalls

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
8. Wire in first hardening hooks (profile + `kernel_config_t` scaffolding, but minimal runtime use)

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

**Completed Today (2025-11-22):**

1. ✅ Implement MMU header (`include/kernel/mmu.h`)
   - Defined opaque `page_table_t` type
   - Defined architecture-independent page flags
   - Defined MMU API functions

2. ✅ Implement x86 MMU (`arch/x86/mmu.c`)
   - Two-level page table structures (page directory + page tables)
   - `mmu_create_address_space()` - allocate page directory
   - `mmu_map_page()` - create page table entries with lazy allocation
   - `mmu_unmap_page()` - remove page table entries
   - `mmu_switch_address_space()` - CR3 manipulation

3. ✅ Debug and fix boot loop (Critical bugs fixed)
   - **Bug 1:** Fixed CR3/CR0 ordering - load CR3 BEFORE enabling paging
   - **Bug 2:** Extended identity mapping from 4MB to 16MB to cover all kernel structures
   - Root cause analysis identified 5 potential issues, fixed the critical ones

4. ✅ Enable paging in kernel
   - Created kernel address space with identity mapping (first 16MB)
   - Loaded CR3 with page directory physical address
   - Enabled CR0.PG bit
   - Successfully booted with paging enabled!

5. ✅ **Phase 2 Complete!** All memory management subsystems working:
   - Timer (PIT + TSC calibration)
   - Physical Memory Manager (PMM)
   - Memory Management Unit (MMU with paging enabled)
   - Kernel boots cleanly and enters idle loop

**Next Session (Phase 3 - Tasks & Scheduler):**

1. 📋 Write MMU unit tests (optional cleanup task)
   - Test page directory creation
   - Test page mapping/unmapping
   - Test address space switching

2. 📋 Plan Phase 3 implementation
   - Review Phase 3 requirements in roadmap
   - Design task structure
   - Design context switching mechanism

3. 📋 Implement task/thread structures
   - Define `struct task` (TCB - Task Control Block)
   - Define task states (READY, RUNNING, BLOCKED)
   - Implement task creation/destruction

4. 📋 Implement context switching
   - Write assembly for register save/restore
   - Implement `switch_to()` function
   - Test basic task switching

5. 📋 Implement basic scheduler
   - Fixed-priority O(1) scheduler
   - Per-priority run queues
   - `schedule()` function

**This Week:**
- ✅ Complete Phase 2 (DONE!)
- Start Phase 3 (Tasks + Scheduler)
- Get first task switch working

**This Sprint (2 weeks):**
- ✅ Complete Phase 2 (memory + interrupts + timer)
- Complete Phase 3 (tasks + scheduler + syscalls)
- First userspace task running in ring3

---

**Keep this file updated daily!**
