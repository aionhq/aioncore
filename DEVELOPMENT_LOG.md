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

## Chapter 4 – Physical Memory Management (IN PROGRESS)

**Next steps:**
- Bitmap-based physical memory manager (PMM).
- O(1) frame allocation with free lists.
- Track frame ownership for unit accounting.
- Reserve kernel regions.

**Reference:** See `CURRENT_WORK.md` for current status.

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
- 🔨 2025-11-21: Phase 2.2 In Progress (PMM)

---

## Lessons Learned

1. **HAL abstraction pays off immediately** – Separating arch code from core logic makes debugging easier and porting realistic.

2. **Per-CPU from day one** – Starting with per-CPU data structures early avoids painful refactoring later.

3. **Unit tests catch bugs early** – The ktest framework already caught several issues in string operations.

4. **Documentation is code** – Treating docs as first-class artifacts (VISION, ROADMAP, UNITS, RT constraints) keeps architecture and implementation in sync.

5. **RT constraints require discipline** – Must think about cycle counts and O(1) guarantees from the start, not as an afterthought. Timer and TSC design were shaped by those budgets.

6. **Automated style checks pay off** – Encoding the C subset and rules in `KERNEL_C_STYLE.md` and enforcing them via `scripts/check_kernel_c_style.sh` avoids whole classes of bugs (libc calls, float, VLAs) without relying solely on human review.

---

**Last Updated:** 2025-11-21
