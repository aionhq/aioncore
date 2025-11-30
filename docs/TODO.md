# TODOs and Known Issues

**Last Updated:** 2025-11-30

This document tracks all TODOs in the codebase, categorized by priority and phase.

---

## 🔴 Critical - Phase 3.3 (Userspace) ✅ ALL RESOLVED

### 1. Context Switch CS Restoration for Userspace ✅ RESOLVED
**File:** `arch/x86/context.s:82-152`
**Status:** ✅ **FIXED** (2025-11-30)
**Solution:** Implemented hybrid iret/jmp context switch

**Implementation:**
- Check target CS.RPL BEFORE loading segment registers
- Kernel→Kernel (RPL=0): Fast path using manual ESP restore + `jmp`
- Kernel→User (RPL=3): Build iret frame + `iret` privilege switch
- Critical fix: Cannot load ring 3 SS from ring 0 - let iret do it

**Test Results:**
- Phase A/B/C tests all pass
- Ring 3 tasks working perfectly

### 2. MMU User Page Mapping ✅ RESOLVED
**File:** `core/user.c:75-105`
**Status:** ✅ **FIXED** (2025-11-30)
**Solution:** Used existing `mmu_map_page()` with MMU_USER flag

**Implementation:**
- Map code at USER_CODE_BASE (0x00400000) with MMU_USER|MMU_PRESENT|MMU_WRITABLE
- Map stack at USER_STACK_TOP-4KB (0xBFFFF000) with MMU_USER|MMU_PRESENT|MMU_WRITABLE
- Copy userspace code from embedded symbols (user_test_start/end)

**Test Results:**
- User pages mapped successfully
- Page faults prevented by USER flag

### 3. Ring 3 Task Initialization ✅ RESOLVED
**File:** `core/user.c:30-154`
**Status:** ✅ **FIXED** (2025-11-30)
**Solution:** Implemented `task_create_user()` function

**Implementation:**
- Set CS=0x1B (user code selector, RPL=3)
- Set SS/DS/ES/FS/GS=0x23 (user data selector, RPL=3)
- Set EIP=USER_CODE_BASE, ESP=USER_STACK_TOP
- Set EFLAGS=0x202 (IF=1)
- Allocate kernel stack for syscall entry
- TSS.esp0 updated in scheduler before switch

**Test Results:**
- User task runs successfully in ring 3
- All syscalls work via INT 0x80
- Task exits cleanly with code 42
- Regression test: `make test-user` passes

---

## 🟡 Medium Priority - Phase 4

### 2. sys_sleep_us Implementation
**File:** `core/syscall.c:151-158`
**Issue:** Not implemented - returns -ENOSYS
**Fix:** Implement sleep queue with timer-based wakeup

**Requirements:**
- Add sleep queue data structure
- Timer callback to check wake times
- Block/unblock task state transitions
- Proper accounting of elapsed time

**Status:** Returns -ENOSYS (clearly documented), deferred to Phase 4
**Priority:** MEDIUM - not critical for Phase 3

### 3. Multi-Page Stack Allocation
**File:** `core/task.c:160`
**Issue:** Tasks limited to 4KB stack (single page)
**Fix:** Allocate multiple contiguous pages from PMM

**Requirements:**
- Update task_create_kernel_thread to support stack_size parameter
- Allocate N pages from PMM
- Handle stack overflow detection (guard pages?)

**Status:** Works fine for now, 4KB sufficient for testing
**Priority:** MEDIUM - Phase 4 or later

### 4. Zombie Task Cleanup
**File:** `core/scheduler.c:258`
**Issue:** Exited tasks not freed (memory leak)
**Fix:** Add reaper thread or cleanup in scheduler

**Options:**
1. Reaper thread that periodically scans for zombies
2. Parent-based cleanup (wait() syscall)
3. Immediate cleanup in scheduler (may cause re-entrancy issues)

**Status:** Not urgent, only 1-2 test tasks
**Priority:** MEDIUM - Phase 4

---

## 🟢 Low Priority - Future Work

### 5. Per-CPU Kernel Stack
**File:** `core/percpu.c:44`
**Issue:** `cpu->kernel_stack = NULL`
**Fix:** Allocate stack per CPU

**Status:** Not needed until SMP (Phase 5+)
**Priority:** LOW

### 6. Work Queue Implementation
**File:** `core/percpu.c:72, 88`
**Issue:** Work queue not implemented
**Fix:** Add deferred work mechanism

**Status:** Not needed yet
**Priority:** LOW

### 7. HAL Unimplemented Functions
**File:** `arch/x86/hal.c` (multiple)

Functions returning NULL/0 or stubbed:
- `cpu_detect_features()` - Just checks FPU, should use CPUID
- `cpu_read_id()` - Always returns 0 (need APIC for SMP)
- `page_map_mmio()` / `page_unmap_mmio()` - Stubs
- `cpu_count()` - Returns 1 (need ACPI/MP tables)
- `cpu_send_ipi()` / `cpu_broadcast_ipi()` - Not implemented (SMP)
- `cpu_start_ap()` - Not implemented (SMP)
- `cpu_stack_trace()` - Not implemented (debugging)

**Status:** Most are SMP-related, not critical
**Priority:** LOW - Phase 5+ (SMP)

### 8. MMU Per-CPU Address Space Tracking
**File:** `arch/x86/mmu.c:44`
**Issue:** No tracking of current address space per CPU
**Fix:** Add to per_cpu_data structure

**Status:** Not needed until SMP
**Priority:** LOW

---

## ✅ Completed TODOs

### Phase 3 Syscalls
- ~~Phase 3: Syscall entry/exit~~ ✅ DONE (core/init.c:47)
- ~~INT 0x80 mechanism~~ ✅ DONE
- ~~GDT/TSS setup~~ ✅ DONE
- ~~Basic syscalls (exit, yield, getpid)~~ ✅ DONE

---

## Summary by Phase

**Phase 3.3 (Userspace - ✅ COMPLETE):**
- ✅ Hybrid iret/jmp context switch (arch/x86/context.s:82-152) - DONE
- ✅ Userspace memory layout design (include/kernel/user.h) - DONE
- ✅ Userspace test program (arch/x86/user_test.s) - DONE
- ✅ MMU user page mapping (core/user.c:75-105) - DONE
- ✅ Ring 3 task initialization (core/user.c:30-154) - DONE
- ✅ Syscalls from ring 3 working (INT 0x80) - DONE
- ✅ Regression test added (make test-user) - DONE

**Phase 4 (IPC & Capabilities):**
- 🟡 sys_sleep_us implementation (core/syscall.c:156)
- 🟡 Multi-page stacks (core/task.c:160)
- 🟡 Zombie task cleanup (core/scheduler.c:258)

**Phase 5+ (SMP & Advanced):**
- 🟢 Per-CPU kernel stacks
- 🟢 Work queues
- 🟢 HAL SMP functions
- 🟢 CPUID feature detection
- 🟢 Stack traces

**Phase X (Future):**
- 🟢 Phase 4 mentioned in core/init.c:48 (IPC & Capabilities)

---

## Notes

- All critical TODOs for Phase 3.3 are clearly documented with implementation paths
- Most remaining TODOs are deferred to later phases (4, 5+)
- No blocking issues for current phase progression
