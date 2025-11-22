# Kernel Development Logbook

An evolving, narrative log of how this kernel is being built – where we started, what we tried, and why the current design looks the way it does.

The goal is to make the architecture *understandable in context*: each chapter captures a stage, its motivations, and the concrete code that implements it.

---

## Chapter 0 – Vision

**Goal:** Build a modern, real‑time–capable, hybrid microkernel that:
- Keeps the kernel core *small* (scheduler, memory, IPC, interrupt dispatch, CPU mgmt only).
- Offloads everything else (drivers, filesystems, networking, higher‑level services) to user space.
- Emphasizes per‑CPU design, capability‑based security, and message‑passing IPC.
- Stays maintainable and eventually amenable to formal verification or a Rust migration.

This vision is captured in detail in:
- `ARCHITECTURE.md` – overall design and principles.
- `IMPLEMENTATION_ROADMAP.md` – phased build‑out plan.

---

## Chapter 1 – Minimal C Kernel (Monolithic Demo)

**State:** Single C file kernel with direct VGA writes and no structure.

**Key characteristics:**
- Boot via GRUB Multiboot, simple `boot.s`, linked with a very small `linker.ld`.
- `kernel.c` directly writes to VGA memory (`0xB8000`) with inline helper functions.
- No interrupts, no memory management, no clear architecture boundaries.

**Files:**
- `kernel.c` – monolithic C kernel demo (ARCHIVED - now legacy).
- `boot.s` – original bootstrap with Multiboot header (moved to arch/x86/).
- `linker.ld` – simple layout for `.multiboot`, `.text`, `.rodata`, `.data`, `.bss` (moved to arch/x86/).
- `README.md` – describes this initial architecture and demo output.

**Purpose:** Educational starting point: "Hello from C kernel" with a freestanding toolchain. This is now effectively *legacy/demo* code as we move to a more serious architecture.

---

## Chapter 2 – Modularization: HAL, Per‑CPU, and VGA Subsystem

**Motivation:** Move from a monolithic demo to a structured kernel that can grow into a microkernel and real‑time system.

**Key decisions:**
1. **Introduce a Hardware Abstraction Layer (HAL)**
   - All CPU, interrupt, MMU, I/O, timer, and system‑level operations go through `struct hal_ops`.
   - Architecture‑specific code sits in `arch/x86`, core kernel never touches assembly directly.
   - Makes porting to RISC-V or other architectures straightforward.

2. **Per‑CPU Data Structures**
   - Introduced `struct per_cpu_data` with cache‑line alignment.
   - Per‑CPU statistics, tick counters, and eventually per‑CPU run queues.
   - Lock‑free tracing infrastructure for debugging.

3. **Modular VGA Driver**
   - Moved VGA code to `drivers/vga/` subsystem.
   - Abstract VGA operations via `struct vga_ops`.
   - Implemented `kprintf` with format specifiers (%d, %u, %x, %08x, %s, %c).

**Files created:**
- `include/kernel/hal.h` – HAL interface
- `arch/x86/hal.c` – x86 HAL implementation
- `include/kernel/percpu.h` – Per-CPU data structures
- `core/percpu.c` – Per-CPU initialization
- `include/drivers/vga.h` – VGA driver interface
- `drivers/vga/vga.c` – VGA subsystem
- `drivers/vga/vga_text.c` – Text mode implementation
- `core/init.c` – Kernel initialization (replaces kernel.c)

**Critical fixes:**
- Fixed interrupt frame struct to match assembly push order (EDI first, not EAX).
- Integrated IDT into HAL so core code only uses HAL interface.
- Fixed kprintf format string parsing for %08x (width + zero padding).

**Status:** Phase 1 COMPLETE (2025-11-21)

---

## Chapter 3 – Interrupts and Real-Time Timer

**Motivation:** Enable timer-driven scheduling and establish RT timing infrastructure.

**Key decisions:**
1. **IDT (Interrupt Descriptor Table)**
   - 256-entry IDT with all exception and IRQ handlers.
   - PIC remapping: IRQs 0-15 mapped to INT 32-47.
   - Exception handlers with full register dumps for debugging.
   - Integrated into HAL (`hal->irq_register`, `hal->irq_enable`).

2. **PIT Timer + TSC Calibration**
   - Initialize 8254 PIT at 1000 Hz.
   - Calibrate TSC (Time Stamp Counter) against PIT for microsecond-precision timing.
   - `timer_read_us()` provides calibrated microsecond timing.
   - Timer interrupt handler completes in <100 cycles (RT compliant).

3. **Unit Testing Framework**
   - `ktest` framework for kernel self-tests.
   - Test registration via linker sections (`.ktests`).
   - Assertion macros: ASSERT, ASSERT_EQ, ASSERT_NULL, etc.
   - Colored test output with pass/fail reporting.
   - Build integration: `make test` or `KERNEL_TESTS=1 make`.

**Files created:**
- `arch/x86/idt.c`, `arch/x86/idt_asm.s` – IDT implementation
- `include/kernel/idt.h` – IDT interface
- `arch/x86/timer.c` – PIT + TSC timer
- `include/kernel/timer.h` – Timer interface
- `include/kernel/ktest.h` – Test framework
- `core/ktest.c` – Test runner
- `lib/string_test.c` – String library tests
- `arch/x86/timer_test.c` – Timer tests
- `docs/TESTING.md` – Testing guide
- `docs/KERNEL_C_STYLE.md` – C style guide
- `.claude.md` – Development workflow rules
 - `scripts/check_kernel_c_style.sh` – Style checker enforcing `KERNEL_C_STYLE.md`

**Integration:**
- Timer integrated into HAL (`hal->timer_init`, `hal->timer_read_tsc`, `hal->timer_read_us`).
- Linker script updated with `.ktests` section for test registration.
- Makefile updated with:
  - `KERNEL_TESTS=1` support and `make test` target.
  - `check-style` target that runs `scripts/check_kernel_c_style.sh`.
  - `all: check-style $(ISO)` so style checks run before every normal build.

**Status:** Phase 2.1 COMPLETE (2025-11-21)

---

## Chapter 4 – Physical Memory Management (Phase 2.2 COMPLETE)

**Motivation:** Move from “just enough RAM to boot” to a real physical memory manager that:

- Understands which regions of RAM are actually usable (via Multiboot).
- Can allocate and free 4KB frames in a controlled way.
- Reserves kernel and low‑memory regions.
- Provides introspectable statistics for later schedulers, units, and userspace memory services.

### 4.1 PMM Design and Implementation

**Core design choices:**

- Use a **bitmap allocator** with 1 bit per 4KB frame, covering up to 4 GiB:
  - `MAX_MEMORY` = 4 GiB, `MAX_FRAMES` = `MAX_MEMORY / 4096`.
  - `frame_bitmap[BITMAP_SIZE]` lives in `.bss` as a static global.
- Start with a simple **global bitmap** implementation and plan to add per‑CPU free lists later for O(1) RT allocation.
- Treat the Multiboot memory map as the single source of truth for available vs reserved RAM.
- Reserve the first 1 MiB (BIOS, VGA, etc.) and the kernel image region before serving allocations.

**Code:**

- `include/kernel/pmm.h`:
  - Defines Multiboot structures we care about (`struct multiboot_info`, `struct multiboot_mmap_entry`) and constants.
  - Provides the PMM API:
    - `void pmm_init(uint32_t multiboot_magic, struct multiboot_info *mbi);`
    - `phys_addr_t pmm_alloc_page(void);`
    - `void pmm_free_page(phys_addr_t page);`
    - `void pmm_reserve_region(phys_addr_t start, size_t size);`
    - `void pmm_get_stats(struct pmm_stats *stats);`
  - Introduces `struct pmm_stats` for memory usage reporting.
- `mm/pmm.c`:
  - Maintains `frame_bitmap[]` and a small `pmm_state` struct with:
    - `total_frames`, `free_frames`, `reserved_frames`, and an `initialized` flag.
  - `pmm_init()`:
    - Verifies the Multiboot magic and that a memory map is present.
    - Initializes the bitmap to “all allocated” (0xFF) and then:
      - Iterates Multiboot memory map entries.
      - For each `MULTIBOOT_MEMORY_AVAILABLE` region:
        - Aligns to 4KB boundaries.
        - Clears bits for usable frames, increments `total_frames` and `free_frames`.
      - Logs each region with type (AVAILABLE, RESERVED, ACPI, NVS, BADRAM).
    - Calls `pmm_reserve_region()` twice:
      - For low memory (`0–1 MiB`).
      - For the kernel image between `_kernel_start` and `_kernel_end` (symbols from the linker script).
    - Prints final totals in frames and MiB.
  - `pmm_alloc_page()`:
    - For now uses `bitmap_find_free()` (O(n) bitmap scan) as a global allocator.
    - Marks the chosen frame as allocated and decreases `free_frames`.
    - Returns a 4KB‑aligned `phys_addr_t` or `0` on failure.
  - `pmm_free_page()`:
    - Validates that the address is 4KB‑aligned and within bounds.
    - Checks for double‑free by testing the bitmap.
    - Clears the bit and increments `free_frames`.
  - `pmm_reserve_region()`:
    - Marks a region as allocated in the bitmap.
    - Adjusts `free_frames` and `reserved_frames` for frames that were previously free.
  - `pmm_get_stats()`:
    - Returns a snapshot of the PMM’s view of total/free/reserved frames.

**Current limitation:**

- Allocation uses a linear bitmap scan for every `pmm_alloc_page()` call. This is acceptable for now (demo and early bring‑up) but is **not** yet RT‑optimal. The future step is to add per‑CPU free lists and potentially a global “first‑free” hint to make allocations O(1) in hot paths.

### 4.2 Tests for PMM Correctness

**Code:**

- `mm/pmm_test.c`:
  - Registers six tests under the `"pmm"` subsystem using `KTEST_DEFINE`:
    - `stats_valid`:
      - Ensures `total_frames > 0`.
      - Ensures `free_frames <= total_frames`.
      - Ensures `reserved_frames < total_frames`.
    - `alloc_nonzero`:
      - Verifies that `pmm_alloc_page()` returns a non‑zero physical address.
    - `alloc_aligned`:
      - Verifies that allocated pages are 4KB‑aligned.
    - `alloc_unique`:
      - Verifies two consecutive allocations return different addresses.
    - `free_restores`:
      - Checks that `free_frames` decreases after alloc and returns to its original value after free.
    - `bulk_alloc`:
      - Allocates 100 pages into an array and then frees them all, ensuring no obvious leaks or failures.

These tests run automatically when `KERNEL_TESTS` is enabled (via `make test` or `KERNEL_TESTS=1 make`), right after timer and PMM initialization.

### 4.3 Integration Into the Boot Flow

To integrate PMM cleanly without breaking existing boot logic, we changed:

- `arch/x86/boot.s`:
  - After setting up the stack and resetting EFLAGS, preserves the Multiboot parameters:
    - EAX (Multiboot magic) and EBX (pointer to `multiboot_info`) are pushed as arguments before calling `kmain`.
  - New C signature:  
    `void kmain(uint32_t multiboot_magic, uint32_t multiboot_info_addr);`
- `core/init.c`:
  - Updates `kmain` to accept Multiboot parameters and casts `multiboot_info_addr` into a `struct multiboot_info*`.
  - Calls `pmm_init(multiboot_magic, mbi)` immediately after `hal->timer_init(1000)` so PMM is ready before any future paging or allocator work.
  - Adjusts the banner text to say “Ready for next phase: paging and tasks”.
- `arch/x86/linker.ld`:
  - Introduces `_kernel_start` and `_kernel_end` symbols around `.text`/`.rodata`/`.data`/`.bss` to let PMM reserve the kernel’s physical memory region.
- `include/kernel/types.h`:
  - Adds `typedef uintptr_t phys_addr_t;` as the canonical physical address type for PMM and future MMU work.
- `Makefile`:
  - Adds `mm/pmm.c` to `C_SOURCES` so PMM is built into the kernel.
  - Adds `mm/pmm_test.c` to the test build when `KERNEL_TESTS=1` is used.
- `CURRENT_WORK.md`:
  - Marks Phase 2.2 (PMM) as COMPLETE.
  - Updates “current task” to basic paging (MMU).

**Outcome:** The kernel now has a functioning physical memory manager that:

- Discovers usable memory via the Multiboot map.
- Reserves critical regions (low memory, kernel image).
- Allocates and frees 4KB frames correctly.
- Exposes statistics and is covered by unit tests for basic correctness.

---

## Chapter 5 – Memory Management Unit (Phase 2.3 COMPLETE)

**Motivation:** Enable virtual memory with paging to provide address space isolation for units and prepare for userspace tasks.

### 5.1 MMU Design and Implementation

**Core design choices:**

- Use **x86 two-level page tables** for 32-bit paging:
  - Page Directory (1024 entries, each covering 4MB)
  - Page Tables (1024 entries per table, each covering 4KB)
  - Total address space: 4GB (32-bit)
- **O(1) map/unmap** operations using direct two-level indexing (no loops)
- **Lazy page table allocation** - only allocate PT frames when needed
- **Identity mapping** for kernel (first 16MB) during initial bring-up
- Architecture-independent API via `page_table_t` opaque type

**Code:**

- `include/kernel/mmu.h`:
  - Opaque `page_table_t*` type for multi-architecture compatibility
  - Architecture-independent page flags (MMU_PRESENT, MMU_WRITABLE, MMU_USER, MMU_NOCACHE, MMU_EXEC)
  - MMU API functions:
    - `page_table_t* mmu_create_address_space(void)` - O(1), allocates page directory
    - `void* mmu_map_page(page_table_t* pt, phys_addr_t phys, virt_addr_t virt, uint32_t flags)` - O(1), <200 cycles
    - `void mmu_unmap_page(page_table_t* pt, virt_addr_t virt)` - O(1), <100 cycles
    - `void mmu_switch_address_space(page_table_t* pt)` - O(1), <50 cycles (CR3 write)
    - `page_table_t* mmu_get_current_address_space(void)`
    - `page_table_t* mmu_get_kernel_address_space(void)`
    - `void mmu_init(void)` - sets up kernel address space and enables paging

- `arch/x86/mmu.c`:
  - Implements x86 two-level paging with:
    - `PD_INDEX(virt)` = `(virt >> 22) & 0x3FF` - extracts page directory index (O(1))
    - `PT_INDEX(virt)` = `(virt >> 12) & 0x3FF` - extracts page table index (O(1))
    - `PAGE_FRAME(entry)` = `entry & ~0xFFF` - extracts physical frame address
  - Page directory and page table structures:
    - Page directory: 1024 PDEs, each points to a page table
    - Page table: 1024 PTEs, each maps a 4KB page
  - `mmu_create_address_space()`:
    - Allocates page directory frame from PMM
    - Zeros page directory (all entries not present)
    - Returns opaque `page_table_t*` handle
  - `mmu_map_page()`:
    - Calculates PD and PT indices via direct bit shifts (O(1))
    - Allocates page table if not present (lazy allocation, at most 1 PT per call)
    - Sets PTE with physical address and converted flags
    - Invalidates TLB for single page via `invlpg` instruction
  - `mmu_unmap_page()`:
    - Clears PTE and invalidates TLB (O(1))
  - `mmu_switch_address_space()`:
    - Writes page directory physical address to CR3
  - `mmu_init()`:
    - Creates kernel address space
    - Identity maps first 16MB (skips NULL page at 0x0)
    - **Loads CR3 BEFORE enabling paging** (critical bug fix)
    - Sets CR0.PG bit to enable paging
    - Logs success

**Integration:**

- Updated `core/init.c` to call `mmu_init()` after PMM initialization
- Updated `Makefile` to compile `arch/x86/mmu.c`
- Kernel successfully boots with paging enabled

### 5.2 Critical Bug Fixes During Bring-Up

**Boot Loop Debugging Session (2025-11-22):**

During MMU bring-up, the kernel experienced a **triple fault boot loop**. Through systematic root-cause analysis, we identified and fixed:

**Bug 1: CR3/CR0 Ordering (CRITICAL)**
- **Symptom:** Kernel reboots immediately after "Enabling paging" message
- **Root cause:** CR0.PG was set to 1 BEFORE loading CR3 with page directory address
- **Impact:** CPU tried to walk page tables using garbage/zero CR3 → immediate triple fault
- **Fix:** Load CR3 first, THEN enable paging:
  ```c
  // WRONG order (causes triple fault):
  cr0 |= (1 << 31);  // Enable paging
  __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
  mmu_switch_address_space(kernel_address_space);  // Too late!

  // CORRECT order:
  mmu_switch_address_space(kernel_address_space);  // Load CR3 first
  cr0 |= (1 << 31);  // Now enable paging
  __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
  ```

**Bug 2: Insufficient Identity Mapping**
- **Symptom:** Boot loop persisted after CR3/CR0 fix
- **Root cause:** Only mapping 4MB but kernel, stack, GDT, and other structures may span beyond 4MB
- **Fix:** Identity map first 16MB to ensure all low-memory structures are accessible after paging enabled

**Lessons learned:**
1. Paging setup order matters: CR3 must be valid BEFORE CR0.PG is set
2. Identity mapping must cover ALL kernel structures (code, data, stack, GDT, page tables themselves)
3. Boot loops without output suggest very early triple faults (pre-exception handler)

### 5.3 The Great kprintf Bug Hunt (2025-11-22)

**The Symptom:**

After successfully enabling paging, debug output showed a mysterious issue:
```
[MMU] Page size: 40 bytes
[MMU] Page directory physical address (CR3): 0x00000010
```

Expected "4096 bytes" but got "40 bytes". Expected aligned CR3 (0x00001000) but got 0x10.

**The Multi-Hour Investigation:**

Over several iterations, we investigated:

1. **Stale binary hypothesis** (multiple attempts)
   - Ran `make clean` and `make` multiple times
   - Verified source code showed correct values (PAGE_SIZE = 4096)
   - Checked `strings kernel.elf` to confirm format strings were correct
   - Problem: `make clean` only removed object files matching current build config, not test .o files from previous `KERNEL_TESTS=1` builds

2. **Preprocessor/macro hypothesis**
   - Searched for multiple PAGE_SIZE definitions (found only 2: kernel + test_printf.c)
   - Attempted to check preprocessor output with `i686-elf-gcc -E` (failed due to missing stdint.h)
   - Examined disassembly of mmu.o and found `push $0x1000` (4096 in hex) - value was CORRECT in compiled code!

3. **Memory corruption hypothesis**
   - Suspected PMM was corrupting values
   - Suspected stack misalignment or va_arg issues
   - Checked for potential buffer overruns

4. **Binary analysis** (getting warmer)
   - Disassembled `mmu.o`: confirmed `push $0x1000` was in the code
   - Used `strings` to check format strings in binary
   - Everything looked correct at compile time!

**The Breakthrough:**

Created host-side unit tests (`tests/kprintf_test.c`) to isolate the number formatting:

```c
TEST(utoa_4096_decimal) {
    char buf[32];
    int len = utoa(4096, buf, 10);
    // Expected: buf="4096", len=4
    // Got: buf="4096", len=2  ← STRING WAS CORRECT BUT LENGTH WAS WRONG!
}
```

**The Root Cause:**

The bug was in `drivers/vga/vga.c` in the `utoa()` function:

```c
static int utoa(uint32_t value, char* buf, int base) {
    char* ptr = buf;
    char* ptr1 = buf;

    // ... generate digits in reverse ...
    do {
        *ptr++ = digit;  // ptr advances past last digit
    } while (value);

    *ptr-- = '\0';  // ptr now points to last digit

    // Reverse string
    while (ptr1 < ptr) {
        // ... swap characters ...
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
        // After loop: ptr1 is in MIDDLE of string, not at end!
    }

    return ptr1 - buf;  // ← BUG: Returns middle position, not length!
}
```

For "4096":
- After digit generation: `ptr` is at position 4 (after '6')
- After reversal: `ptr1` is at position 2 (middle of string)
- Returned length: 2 (should be 4)
- kprintf printed only 2 chars: "40"

**The Fix:**

```c
static int utoa(uint32_t value, char* buf, int base) {
    // ... digit generation ...

    int len = ptr - buf;  // ← Save length BEFORE reversal
    *ptr-- = '\0';

    // ... reversal ...

    return len;  // ← Return saved length, not ptr position
}
```

Applied same fix to `utoa64()` which had identical bug.

**What We Learned:**

1. **Test infrastructure pays off massively**
   - Host-side unit tests isolated the bug in <5 minutes after creation
   - QEMU debugging would have taken hours more
   - Fast feedback loop (gcc on host vs cross-compile + QEMU boot)

2. **Make clean wasn't clean enough**
   - Original `make clean` only removed `$(ALL_OBJECTS)` which depended on current config
   - Test .o files from `KERNEL_TESTS=1` builds were left behind
   - Fixed: `find . -name "*.o" -type f -delete` to catch all object files

3. **When debug output is wrong, debug the debug output**
   - We spent hours assuming PAGE_SIZE or the build was wrong
   - Should have tested kprintf itself first
   - The symptom (truncated numbers) pointed to formatting, not data

4. **Host tests > QEMU tests for pure logic**
   - Format conversion, string manipulation, math = test on host
   - Hardware interaction, paging, interrupts = test in QEMU
   - Build the test infrastructure early and use it liberally

**Files Created/Modified:**

- `tests/kprintf_test.c` - Host-side unit tests for utoa/itoa functions
- `tests/test_main.c` - Single main() for all test files
- `tests/host_test.h` - Improved with TEST_MAIN_IMPL guard
- `Makefile` - Fixed clean target, added kprintf tests
- `drivers/vga/vga.c` - Fixed utoa() and utoa64() length calculation

**Outcome:**

After the fix and clean rebuild:
```
[MMU] Page size: 4096 bytes
[MMU] Page directory physical address (CR3): 0x00001000
```

Perfect! The kernel now prints correct debug output and we have a solid test framework to catch similar bugs early.

**Status:** Phase 2.3 COMPLETE (2025-11-22)

---

## Documentation Evolution

### Initial Documentation (Phase 1)
- README.md – Basic build instructions
- Inline comments in code

### Comprehensive Documentation (Phase 2)
- **docs/VISION.md** – Long-term architectural vision
- **docs/IMPLEMENTATION_ROADMAP.md** – Phase-by-phase plan
- **docs/ARCHITECTURE.md** – Design principles
- **docs/UNITS_ARCHITECTURE.md** – Units model details
- **docs/RT_CONSTRAINTS.md** – Real-time requirements
- **docs/FORMAL_VERIFICATION.md** – Verification strategy
- **docs/MULTI_ARCH.md** – Multi-architecture support
- **docs/KERNEL_C_STYLE.md** – C coding standards
- **docs/TESTING.md** – Unit testing guide
- **docs/ISSUES.md** – Issue tracking
- **docs/DOCS.md** – Documentation navigation guide
- **CURRENT_WORK.md** – Daily progress tracking

### Development Workflow
- `.claude.md` – Rules for development workflow
- Always read `KERNEL_C_STYLE.md` before/after coding
- Always update `CURRENT_WORK.md` after completing work

---

## Key Milestones

- ✅ 2025-11-21: Phase 1 Complete (HAL, per-CPU, IDT, VGA)
- ✅ 2025-11-21: Phase 2.1 Complete (Timer, unit tests)
- ✅ 2025-11-21: Phase 2.2 Complete (PMM)
- ✅ 2025-11-22: Phase 2.3 Complete (MMU, paging enabled)
- ✅ 2025-11-22: **Phase 2 COMPLETE** (All memory subsystems functional)

---

## Lessons Learned

1. **HAL abstraction pays off immediately** – Separating arch code from core logic makes debugging easier and porting realistic.

2. **Per-CPU from day one** – Starting with per-CPU data structures early avoids painful refactoring later.

3. **Unit tests catch bugs early** – The ktest framework already caught several issues in string operations.

4. **Documentation is code** – Treating docs as first-class artifacts (VISION, ROADMAP, UNITS, RT constraints) keeps architecture and implementation in sync.

5. **RT constraints require discipline** – Must think about cycle counts and O(1) guarantees from the start, not as an afterthought. Timer and TSC design were shaped by those budgets.

6. **Automated style checks pay off** – Encoding the C subset and rules in `KERNEL_C_STYLE.md` and enforcing them via `scripts/check_kernel_c_style.sh` avoids whole classes of bugs (libc calls, float, VLAs) without relying solely on human review.

7. **Paging initialization order is critical** – CR3 must be loaded with a valid page directory BEFORE setting CR0.PG, otherwise the CPU tries to walk page tables using garbage CR3 and triple faults immediately.

8. **Identity mapping must be sufficient** – Initial identity mapping must cover ALL kernel structures (code, data, bss, stack, GDT, page tables) to prevent page faults during early boot. 16MB is sufficient for typical kernel layouts.

---

**Last Updated:** 2025-11-22

---

## Chapter 4 – Tasks, Scheduling & Context Switching (Phase 3)

**Goal:** Implement preemptive multitasking with O(1) scheduler and context switching.

**Date:** 2025-11-22

### What We Built

**Task Management** (`core/task.c`, `include/kernel/task.h`):
- Task Control Block (TCB) with CPU context (EDI, ESI, EBX, EBP, ESP, EIP)
- Task states: READY, RUNNING, BLOCKED, ZOMBIE
- Idle task (always schedulable, halts CPU when no work)
- Bootstrap task pattern for pre-scheduler initialization code
- Task wrapper for clean thread entry/exit

**O(1) Scheduler** (`core/scheduler.c`, `include/kernel/scheduler.h`):
- 256 priority levels (0-255, higher = more urgent)
- Priority bitmap using `__builtin_clz` for O(1) highest-priority lookup
- Per-priority circular doubly-linked queues
- `schedule()` function: pick next, context switch, < 200 cycles total
- `scheduler_tick()` called from timer interrupt (sets need_resched flag)

**Context Switching** (`arch/x86/context.s`):
- Assembly implementation following cdecl calling convention
- Saves/restores callee-saved registers (EDI, ESI, EBX, EBP, ESP, EIP)
- Uses `push ebp; mov ebp, esp` frame setup
- Uses `call 1f; pop ecx` trick to capture return address
- Uses `jmp *20(%edx)` to restore EIP (not push/ret)

### Critical Bugs Encountered & Fixed

**Bug 1: Task Stack Layout (CRITICAL)**
- Problem: Stack arguments in wrong order for cdecl convention
- Symptom: task_wrapper received arg=0, dereferenced NULL, instant crash
- Fix: [esp] = return address, [esp+4] = first argument (correct cdecl order)
- Learning: cdecl calling convention is unforgiving - must be exact

**Bug 2: Context Switch Pattern (CRITICAL)**
- Problem: Used incorrect ESP save/restore pattern
- Symptom: Triple fault on first task switch (infinite boot loop)
- Fix: Implemented proper pattern with frame setup, call/pop for EIP, jmp for restore
- Learning: Context switch assembly requires established patterns, can't improvise

**Bug 3: Makefile Clean Target**
- Problem: Two `clean:` definitions - second replaced first
- Symptom: `make clean` didn't clean anything
- Fix: Consolidated into single clean target with dependencies
- Learning: Make silently replaces duplicate targets

**Bug 4: Stack Size Ignored**
- Problem: Accepted any stack_size parameter but always allocated 4096 bytes
- Symptom: Risk of memory corruption with larger stacks
- Fix: Enforce stack_size == 4096 (only one page currently supported)

**Bug 5: IRQ0 Masked**
- Problem: PIC remapped with all IRQs masked (0xFF), IRQ0 never unmasked
- Symptom: Timer interrupts couldn't fire (preemption impossible)
- Fix: Added `irq_clear_mask(0)` after registering timer handler

### Current State

**Working:**
- ✅ Task creation with proper stack setup
- ✅ Context switching between kernel threads
- ✅ Cooperative scheduling (manual yield)
- ✅ O(1) scheduler queues and priority selection
- ✅ No crashes, no boot loops!

**Not Yet Tested:**
- ⚠️ Timer-driven preemption (interrupts disabled for debugging)
- ⚠️ Preemptive multitasking
- ⚠️ Multiple concurrent threads

### Key Design Decisions

**Bootstrap Task Pattern:**
- Created sentinel task representing pre-scheduler initialization code
- Marked as ZOMBIE so it's never rescheduled
- Current task pointer starts at bootstrap_task before first schedule()
- Allows clean transition from initialization to first real task

**Idle Task Always Ready:**
- Idle task (priority 0) is always READY
- Scheduler never has "nothing to run"
- Idle thread just halts CPU until next interrupt
- Ensures scheduler_pick_next() never returns NULL

**Simplified Context:**
- Only save/restore callee-saved registers (EDI, ESI, EBX, EBP, ESP, EIP)
- Caller-saved registers (EAX, ECX, EDX) preserved by C calling convention
- Segment registers not saved/restored (all tasks kernel mode for now)
- Will need segment handling when adding usermode (Phase 4)

### Remaining Work

**High Priority:**
1. Re-enable interrupts and test timer-driven preemption
2. Stack fragility: 4KB with no guard pages (risk of silent corruption)

**Medium Priority:**
3. Static pt_storage bug: only one address space possible
4. Bootstrap task lifecycle cleanup (stays ZOMBIE forever)

**Low Priority (Phase 4):**
5. Segment register handling for usermode (need GDT/TSS)

### Lessons Learned

**1. Incremental testing is critical**
- Adding tasks + scheduler + context switch + timer all at once made debugging very hard
- Should have tested context_switch in isolation first
- Testing without interrupts proved context_switch bug was separate from IRQ bugs

**2. cdecl calling convention is unforgiving**
- Stack layout must be exact: [esp] = return, [esp+4] = arg1
- Any deviation causes immediate crash
- No compiler warnings for assembly mistakes

**3. Context switch requires exact patterns**
- Can't improvise with ESP save/restore
- Must follow established patterns from literature
- Getting it wrong causes triple fault (not a helpful error message!)

**4. User feedback was key**
- User identified all 5 bugs from code review
- Provided exact fix for context_switch pattern
- External code review caught what unit tests couldn't

**5. Documentation during debugging helps**
- Writing PHASE3_BROKEN.md while debugging helped organize thoughts
- Root cause analysis document captured 5 hypotheses
- Clear documentation made it easy to resume after interruptions

---

**Status:** Phase 3 is 70% complete. Cooperative scheduling works. Next: enable preemptive scheduling.

**Last Updated:** 2025-11-22
