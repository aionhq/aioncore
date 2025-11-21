# Known Issues and Action Items

This document tracks architectural issues, technical debt, and action items discovered during development.

## Status Legend
- ✅ **FIXED** - Issue resolved
- 🔨 **IN PROGRESS** - Currently being worked on
- 📋 **TODO** - Planned for future work
- ⚠️ **KNOWN LIMITATION** - Documented limitation, may address later

---

## Critical Issues (Must Fix Before Phase 3)

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
**Status:** TODO (Phase 2)
**Priority:** High
**Issue:** We're relying on GRUB's GDT layout (code=0x08, data=0x10), but we don't set up our own GDT. This is fragile and makes porting/SMP harder.
**Impact:**
- Works now with GRUB, but not guaranteed
- Difficult to understand actual segment configuration
- Blocks proper TSS setup for syscalls
- Blocks SMP (each CPU needs its own TSS)
**Action Items:**
- [ ] Create `arch/x86/gdt.c` and `arch/x86/gdt.h`
- [ ] Define GDT with clear segment descriptors
- [ ] Install GDT in `cpu_init()`
- [ ] Set up TSS (Task State Segment) for syscall stack switching
- [ ] Document segment selector values (0x08, 0x10, 0x18, 0x20, 0x23, 0x2B)
**Dependencies:** None, can be done now
**Estimated Effort:** ~4 hours

### 4. ⚠️ Timer Semantics Unclear
**Status:** KNOWN LIMITATION
**Priority:** High (Phase 2)
**Issue:** `hal->timer_read_us()` returns raw TSC cycles, not actual microseconds. Name is misleading.
**Impact:**
- Cannot do accurate timing without calibration
- RT deadlines will be wrong
- Scheduler time slices will be unpredictable
**Options:**
1. Rename to `timer_read_cycles()` (honest but less useful)
2. Calibrate TSC against PIT and convert to microseconds (correct)
**Recommended:** Option 2 - implement PIT-based calibration
**Action Items:**
- [ ] Implement PIT (Programmable Interval Timer) driver
- [ ] Calibrate TSC frequency at boot
- [ ] Convert TSC to microseconds in `timer_read_us()`
- [ ] Provide `timer_read_tsc()` for cycle-accurate timing
**Dependencies:** None
**Estimated Effort:** ~6 hours (includes PIT timer implementation)

---

## Medium Priority Issues

### 5. 📋 Tracing Concurrency
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

### 6. 📋 Legacy Documentation Drift
**Status:** TODO (Cleanup)
**Priority:** Low
**Issue:** Top-level `README.md`, old `kernel.c`, old `boot.s`, and old `linker.ld` describe the original monolithic VGA demo, but the new build uses the modular architecture.
**Impact:** Confusing for new contributors
**Action Items:**
- [ ] Move old files to `archive/` directory
- [ ] Update `README.md` to describe new architecture
- [ ] Add `GETTING_STARTED.md` with build instructions
- [ ] Add `ARCHITECTURE.md` reference to README
**Dependencies:** None
**Estimated Effort:** ~2 hours

---

## Design Improvements

### 7. 📋 Assert Infrastructure
**Status:** TODO (Phase 3)
**Priority:** Medium
**Issue:** No runtime assertion framework yet. Need this for determinism verification and invariant checking.
**Action Items:**
- [ ] Create `lib/assert.c` and `include/kernel/assert.h`
- [ ] Implement `assert()`, `assert_irqs_disabled()`, `assert_not_null()`
- [ ] Add `#ifdef DEBUG` wrapper for performance
- [ ] Add assertions to all function preconditions
**Dependencies:** None
**Estimated Effort:** ~3 hours

### 8. 📋 Static Analysis Integration
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

### 9. 📋 Multi-Architecture Build
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

### 10. 📋 Formal Verification Setup
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

## Current Focus (Phase 2)

**Actively Working On:**
- Timer implementation (PIT + TSC calibration) [Issue #4]
- Physical memory manager
- Basic paging/MMU

**Up Next:**
- GDT setup [Issue #3]
- Assert infrastructure [Issue #7]

**Future Phases:**
- Tracing concurrency [Issue #5] - Phase 6
- Static analysis [Issue #8] - Phase 4
- Formal verification [Issue #10] - Phase 5+

---

Last Updated: 2025-11-21
