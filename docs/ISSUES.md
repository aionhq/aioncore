# Known Issues and Action Items

This document tracks architectural issues, technical debt, and action items discovered during development.

## Status Legend
- ✅ **FIXED** - Issue resolved
- 🔨 **IN PROGRESS** - Currently being worked on
- 📋 **TODO** - Planned for future work
- ⚠️ **KNOWN LIMITATION** - Documented limitation, may address later

---

## Critical Issues (Blocking Current Phase)

### 1. ✅ Interrupt Frame Layout Mismatch
**Status:** FIXED
**Date Fixed:** 2025-11-21
**Issue:** `pushal` instruction stores registers in a different order than our `struct interrupt_frame` expected, causing register values in exception handlers to be incorrect.
**Fix:** Changed assembly to explicitly push registers in the order the struct expects, matching the struct layout exactly.
**Files Changed:**
- `arch/x86/idt_asm.s` - Explicit register push/pop order
- Added ESP calculation to save correct original stack pointer

### 2. ✅ HAL vs IDT Duplication
**Status:** FIXED
**Date Fixed:** 2025-11-21
**Issue:** `hal->irq_register` was a stub while `idt_register_handler` had the real implementation. Core code couldn't use HAL for interrupt registration.
**Fix:** Made HAL the single abstraction layer:
- `hal->irq_register` now calls `idt_register_handler`
- `hal->cpu_init` now calls `idt_init`
- Core code uses HAL only, never touches IDT directly
**Files Changed:**
- `arch/x86/hal.c` - Wire up HAL to IDT functions
- `core/init.c` - Remove explicit `idt_init()` call

---

## High Priority Issues

### 3. 📋 GDT Not Initialized
**Status:** TODO (Phase 3.2 - REQUIRED FOR USERSPACE)
**Priority:** **HIGH - Blocks Phase 3.2**
**Issue:** We're relying on GRUB's GDT layout (code=0x08, data=0x10), but we don't set up our own GDT. This blocks userspace tasks.
**Impact:**
- Works now with GRUB kernel segments (ring 0)
- **BLOCKS:** Ring 3 userspace tasks (need user code/data segments)
- **BLOCKS:** Proper TSS setup for syscall stack switching
- **BLOCKS:** SMP (each CPU needs its own TSS)
**Action Items:**
- [ ] Create `arch/x86/gdt.c` and `arch/x86/gdt.h`
- [ ] Define GDT with kernel AND user segments (ring 0 and ring 3)
- [ ] Install GDT in `cpu_init()`
- [ ] Set up TSS (Task State Segment) for syscall stack switching
- [ ] Document segment selector values
**Dependencies:** None, needed NOW for Phase 3.2
**Estimated Effort:** ~4 hours

### 4. ✅ Timer Calibration
**Status:** FIXED
**Date Fixed:** 2025-11-30
**Issue:** `timer_read_us()` returned raw TSC cycles, not actual microseconds.
**Fix:** Implemented PIT-based TSC calibration:
- PIT timer driver (1000 Hz)
- TSC calibration against PIT at boot
- `timer_read_us()` now returns actual microseconds
- `timer_read_tsc()` available for cycle-accurate timing
**Files Changed:**
- `arch/x86/timer.c` - Complete rewrite with PIT + TSC calibration

---

## Medium Priority Issues

### 5. 📋 Static MMU Page Table Storage
**Status:** TODO (Before Phase 3.3 - Multiple Address Spaces)
**Priority:** Medium (will block multiple userspace tasks)
**Issue:** `arch/x86/mmu.c:28` uses static 4KB buffer for page tables, limiting kernel to single address space.
**Impact:**
- Works for identity-mapped kernel
- **BLOCKS:** Multiple address spaces for userspace tasks
- **BLOCKS:** Process isolation
**Action Items:**
- [ ] Allocate page tables dynamically from PMM
- [ ] Track page table allocations for cleanup
- [ ] Update `mmu_create_address_space()` to allocate from PMM
**Dependencies:** PMM working (✅ done)
**Estimated Effort:** ~3 hours
**Files Affected:** `arch/x86/mmu.c`

### 6. 📋 Tracing Concurrency
**Status:** TODO (Phase 6 - SMP)
**Priority:** Medium
**Issue:** Per-CPU trace buffers use plain `uint32_t` for head/tail without barriers on the read side. Concurrent reads from another CPU could see torn/stale entries.
**Current Mitigation:** Only read traces when system is quiescent or with IRQs disabled on source CPU.
**Proper Fix (for SMP):**
- [ ] Add memory barriers on read side in `trace_read()`
- [ ] Use atomic loads for head/tail
- [ ] Document concurrency model clearly
**Files Affected:** `core/percpu.c`
**Dependencies:** Wait until SMP implementation (Phase 6)
**Estimated Effort:** ~2 hours

### 7. 📋 Task Stack Size Limited to 4096
**Status:** TODO (Phase 4 or later)
**Priority:** Low
**Issue:** Tasks get only one page (4KB) of stack.
**Impact:**
- Works for simple kernel threads
- May overflow with deep call stacks or large local variables
**Action Items:**
- [ ] Support multi-page stack allocation
- [ ] Add guard pages for stack overflow detection
**Dependencies:** None (nice to have)
**Estimated Effort:** ~2 hours

### 8. 📋 No Task Cleanup / Zombie Reaping
**Status:** TODO (Phase 3.3 or 4)
**Priority:** Low
**Issue:** When tasks exit, they become zombies but are never freed.
**Impact:**
- Memory leak (task structures not freed)
- Eventually run out of task IDs
**Action Items:**
- [ ] Add reaper thread to clean up zombies
- [ ] OR: Clean up in scheduler when no references remain
**Dependencies:** None
**Estimated Effort:** ~3 hours

---

## Design Improvements

### 9. 📋 Assert Infrastructure
**Status:** TODO (Phase 4)
**Priority:** Medium
**Issue:** No runtime assertion framework yet. Need this for determinism verification and invariant checking.
**Action Items:**
- [ ] Create `lib/assert.c` and `include/kernel/assert.h`
- [ ] Implement `assert()`, `assert_irqs_disabled()`, `assert_not_null()`
- [ ] Add `#ifdef DEBUG` wrapper for performance
- [ ] Add assertions to all function preconditions
**Dependencies:** None
**Estimated Effort:** ~3 hours

### 10. 📋 Static Analysis Integration
**Status:** TODO (Phase 4)
**Priority:** Medium
**Issue:** No static analysis in build yet. Should catch bugs early.
**Action Items:**
- [ ] Integrate `cppcheck` into Makefile
- [ ] Add `clang-tidy` configuration
- [ ] Add `make analyze` target
- [ ] Fix all warnings found
**Dependencies:** None
**Estimated Effort:** ~4 hours

---

## Future Enhancements

### 11. 📋 Multi-Architecture Build
**Status:** TODO (After Phase 4)
**Priority:** Low
**Issue:** Makefile hardcodes x86 toolchain. Need conditional compilation for RISC-V port.
**Action Items:**
- [ ] Add `ARCH ?= x86` to Makefile
- [ ] Conditional toolchain selection
- [ ] Architecture-specific source file selection
**Dependencies:** Complete x86 implementation first
**Estimated Effort:** ~2 hours
**Reference:** See `MULTI_ARCH.md`

### 12. 📋 Formal Verification Setup
**Status:** TODO (Phase 5+)
**Priority:** Low
**Issue:** No formal verification tooling integrated yet.
**Action Items:**
- [ ] Set up CBMC for model checking
- [ ] Write property specifications
- [ ] Verify IPC is deadlock-free
- [ ] Verify scheduler priority guarantees
**Dependencies:** IPC and scheduler implementation
**Estimated Effort:** ~20 hours
**Reference:** See `FORMAL_VERIFICATION.md`

---

## Resolved Issues (History)

### ✅ Context Switch EFLAGS Not Restored (CRITICAL - Phase 3.1)
**Fixed:** 2025-11-30
**Issue:** `context_switch()` only saved/restored 6 of 13 fields in `cpu_context_t`, missing EFLAGS and segment registers. After first context switch, tasks inherited previous task's EFLAGS, causing timer interrupts to stop.
**Fix:** Updated `arch/x86/context.s` to save/restore ALL 13 fields including EFLAGS and segment registers
**Impact:** Fixed preemptive multitasking - tasks now properly resume with correct interrupt state

### ✅ Scheduler Priority Preemption Logic (Phase 3.1)
**Fixed:** 2025-11-30
**Issue:** `scheduler_tick()` only checked same-priority round-robin, didn't preempt for higher-priority tasks
**Fix:** Added check for higher-priority ready tasks BEFORE round-robin check in `core/scheduler.c`
**Impact:** Idle task now properly preempted by test threads

### ✅ kprintf %d Returns Length 0 for Value 0 (Phase 3.1)
**Fixed:** 2025-11-30
**Issue:** `itoa()` in `drivers/vga/vga.c` returned length 0 when converting value 0
**Fix:** Save length before null terminator (like `utoa()` does)
**Impact:** Debug output now works correctly for zero values

### ✅ Timer Interrupts Not Preempting (Phase 3.1)
**Fixed:** 2025-11-30
**Issue:** Timer interrupts fired but didn't trigger task switches
**Fix:** Added `need_resched` check and `schedule()` call to `irq_handler()` in `arch/x86/idt.c`
**Impact:** Timer-driven preemption now working

### ✅ VGA Driver Not Modular
**Fixed:** Phase 1
**Issue:** Original VGA code was hardcoded in main kernel
**Fix:** Created modular `drivers/vga/` with ops table abstraction

### ✅ No HAL Abstraction
**Fixed:** Phase 1
**Issue:** Architecture-specific code mixed with core logic
**Fix:** Created `struct hal_ops` and separated `arch/x86/` from `core/`

### ✅ No Per-CPU Infrastructure
**Fixed:** Phase 1
**Issue:** Global state everywhere, not SMP-ready
**Fix:** Created `struct per_cpu_data` with cache-line alignment

### ✅ Unsafe String Functions
**Fixed:** Phase 1
**Issue:** No `strlcpy`, `strlcat` equivalents
**Fix:** Implemented safe string library in `lib/string.c`

---

## Issue Tracking Process

### How to Add an Issue
1. Add to appropriate section above
2. Assign priority (Critical/High/Medium/Low)
3. Estimate effort
4. List dependencies
5. Track in todo list if actively working on it

### Priority Definitions
- **Critical:** Blocks current phase, must fix immediately
- **High:** Needed for next phase, schedule soon
- **Medium:** Quality/maintainability improvement, nice to have
- **Low:** Future enhancement, defer until later phases

### Status Updates
- Update status when work begins
- Move to "Resolved Issues" when fixed
- Add "Date Fixed" and "Files Changed"
- Update dependencies if affected

---

## Current Focus (Phase 3.2 - Syscalls & Userspace)

**Completed:**
- ✅ Phase 1: Foundation & HAL
- ✅ Phase 2: Memory, timing, serial console
- ✅ Phase 3.1: Tasks & preemptive scheduling

**Next Up (Blocks Phase 3.2):**
- GDT with ring 3 segments [Issue #3] - **CRITICAL**
- TSS for kernel stack switching
- INT 0x80 syscall mechanism
- First userspace task

**Medium Priority:**
- Dynamic page table allocation [Issue #5] - Before multiple userspace tasks
- Task cleanup / zombie reaping [Issue #8]
- Stack size expansion [Issue #7]

**Future Phases:**
- Tracing concurrency [Issue #6] - Phase 6 (SMP)
- Assert infrastructure [Issue #9] - Phase 4
- Static analysis [Issue #10] - Phase 4
- Formal verification [Issue #12] - Phase 5+

---

Last Updated: 2025-11-30
