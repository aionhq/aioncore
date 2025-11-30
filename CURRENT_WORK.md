# Current Work Status

**Last Updated:** 2025-11-30 (Phase 3.3 COMPLETE! ✅)
**Phase:** Phase 3 - Tasks, Scheduling & Syscalls
**Status:** Phase 2 ✅ | Phase 3.1 ✅ | Phase 3.2 ✅ | Phase A ✅ | Phase B ✅ | Phase C ✅ | **Phase 3.3 ✅ COMPLETE**

---

## Quick Links

- 📖 [Full Documentation Index](docs/DOCS.md)
- 🎯 [Vision & Long-term Goals](docs/VISION.md)
- 🗺️ [Implementation Roadmap](docs/IMPLEMENTATION_ROADMAP.md)
- 🐛 [Known Issues](docs/ISSUES.md)
- ✅ [TODOs in Code](docs/TODO.md)
- 📝 [Development Log](DEVELOPMENT_LOG.md)
- 🔧 [Context Switch Bug Analysis](SCHEDULER_CONTEXT_SWITCH_BUG.md)
- 🚧 [Phase 3.3 Status (Userspace)](PHASE3_3_STATUS.md)

---

## 🚀 MAJOR MILESTONE: Ring 3 Userspace Working! 🚀

**Date:** 2025-11-30

**Phase 3.3 is COMPLETE!** We now have full ring 0/ring 3 privilege separation with:
- ✅ Userspace tasks running in ring 3
- ✅ INT 0x80 syscalls from ring 3
- ✅ Hybrid iret/jmp context switch
- ✅ Memory mapped with USER flag
- ✅ TSS-based kernel stack switching
- ✅ Clean task creation/exit
- ✅ Regression test: `make test-user`

**Test results:**
```
[USER] Task 'user_test' initialized:
[USER]   CS=0x001b SS=0x0023 DS=0x0023
[USER]   EIP=0x00400000 ESP=0xc0000000 EFLAGS=0x00000202
[USER]   Kernel stack: 0x0000c000 (size=4096)
[SYSCALL] sys_exit(42) from task 'user_test'
[TASK] Task 'user_test' (ID: 1) exiting with code 42
```

All syscalls (getpid, yield x5, exit) working perfectly!

---

## 🎉 MILESTONE: Preemptive Multitasking Working! 🎉

**Date:** 2025-11-30

**Phase 3.1 is COMPLETE!** We now have fully functional preemptive multitasking with:
- ✅ Timer-driven preemption at 1000 Hz
- ✅ Priority-based scheduling (256 levels)
- ✅ O(1) task selection
- ✅ Context switching with full EFLAGS/segment register restore
- ✅ Multiple tasks running concurrently
- ✅ Clean task creation/destruction

**Test results:**
```
[TEST] Test thread started!
[TEST] iteration 0, counter=1
[TEST] iteration 1, counter=2
...
[TEST] iteration 9, counter=10
[TEST] Test thread exiting (final counter=10)

[TIMER] Tick 100
[TIMER] Tick 200
[TIMER] Tick 300
...
[TIMER] Tick 3200
```

All 10 iterations complete, timer interrupts firing continuously, no crashes!

---

## What We Fixed Today ✅

### Bug #6: Context Switch EFLAGS Not Restored (CRITICAL) - FIXED
**File:** `arch/x86/context.s`

**Problem:**
- `cpu_context_t` structure includes EFLAGS and segment registers (13 fields total)
- `context_switch()` assembly only saved/restored 6 fields: edi, esi, ebx, ebp, esp, eip
- **EFLAGS and segment registers were NEVER loaded from new task!**
- After context switch, tasks inherited previous task's EFLAGS
- Result: Timer interrupts stopped firing after first switch (IF bit not set)

**Fix:** Updated context_switch to save/restore ALL fields in cpu_context_t:
```asm
# Save EFLAGS
pushf
popl %ecx
movl %ecx, 48(%eax)

# Save segment registers (ss, ds, es, fs, gs)
...

# Restore EFLAGS first
movl 48(%edx), %ecx
pushl %ecx
popf

# Restore segment registers
...
```

**Impact:** Timer interrupts now fire after context switch, preemption works!

### Bug #7: kprintf %d Returns Length 0 for Value 0 - FIXED
**File:** `drivers/vga/vga.c:itoa()`

**Problem:** `itoa()` returned length 0 when converting value 0, so kprintf printed nothing

**Fix:** Save length before null terminator (like utoa() does):
```c
int len = ptr - buf;  // Save length first
*ptr-- = '\0';
return len;
```

### Bug #8: Scheduler Priority Preemption Logic - FIXED
**File:** `core/scheduler.c:scheduler_tick()`

**Problem:** Only checked for tasks at SAME priority, didn't preempt for higher priority
- Idle (priority 0) running, test thread (priority 128) ready
- `scheduler_tick()` checked `ready[0].count > 0` → false
- Never set need_resched!

**Fix:** Check for higher-priority tasks first:
```c
uint8_t highest_ready = find_highest_priority();
if (highest_ready > current->priority) {
    need_resched = true;  // Higher priority ready, preempt!
}
```

### Preemption Logic Added - COMPLETE
**File:** `arch/x86/idt.c:irq_handler()`

**Added:** Check need_resched and call schedule() before returning from IRQ:
```c
extern bool scheduler_need_resched(void);
extern void schedule(void);
if (scheduler_need_resched()) {
    schedule();
}
```

**Impact:** Timer interrupts now trigger preemptive task switches!

---

## How Preemptive Scheduling Works

**The complete flow:**

1. **Timer interrupt fires** (IRQ 0, 1000 Hz)
2. **CPU saves state** (pushed by hardware: eflags, cs, eip, etc.)
3. **irq_common_stub** (asm) saves all registers, calls `irq_handler()`
4. **irq_handler()** calls `timer_interrupt_handler()`
5. **timer_interrupt_handler()** calls `scheduler_tick()`
6. **scheduler_tick()** checks if higher-priority task ready, sets `need_resched = true`
7. **irq_handler()** sends EOI, then checks `need_resched`
8. **schedule()** called:
   - Picks highest priority ready task
   - Dequeues new task (marks RUNNING)
   - Enqueues current task (marks READY)
   - Calls `context_switch(old, new)`
9. **context_switch()** saves old context, loads new context, jumps to new EIP
10. **New task resumes** where it left off
11. **Return from schedule()**, return from irq_handler, iret
12. **Task continues running** until next preemption

**Key insight:** schedule() is called FROM interrupt context, so when we return from schedule(), we return to irq_handler, which irets back to the NEW task!

---

## Files Changed Today

**Modified:**
- `arch/x86/context.s` - Added EFLAGS and segment register save/restore
- `drivers/vga/vga.c` - Fixed itoa() length calculation for value 0
- `core/scheduler.c` - Fixed scheduler_tick() priority preemption logic
- `arch/x86/idt.c` - Added need_resched check and schedule() call to irq_handler()
- `arch/x86/timer.c` - Changed debug output from every 10 ticks to every 100 ticks
- `core/init.c` - Simplified test thread (removed manual yield, added counter)

**All changes pass style checks!**

---

## What's Next: Phase 3.2

### Syscall Entry/Exit Mechanism

**Goals:**
- System call interface for userspace
- INT 0x80 or SYSENTER/SYSEXIT
- Parameter passing (registers or stack)
- Return values

**Tasks:**
1. Design syscall ABI (x86 calling convention)
2. Implement syscall entry (INT handler or SYSENTER)
3. Implement syscall dispatcher
4. Add basic syscalls (exit, yield, getpid)
5. Test from kernel mode first
6. Add privilege level transition (ring 3 → ring 0)

### First Userspace Task (Ring 3)

**Prerequisites:**
- GDT with user code/data segments
- TSS with ESP0 (kernel stack pointer)
- Syscall mechanism working

**Tasks:**
1. Set up GDT with ring 3 segments
2. Create TSS, set ESP0
3. Load TR register
4. Create userspace task (separate address space)
5. Transition to ring 3 (iret with ring 3 segments)
6. Test syscall from userspace

---

## Current Status Summary

**Phase 1: Foundation & HAL** - ✅ COMPLETE
- HAL abstraction
- Per-CPU infrastructure
- IDT and interrupts
- VGA driver
- Safe string library

**Phase 2: Memory & Timing** - ✅ COMPLETE
- Timer (PIT + TSC calibration)
- Physical memory manager (PMM)
- MMU with paging
- Serial console
- Console multiplexer
- Direct QEMU boot

**Phase 3.1: Tasks & Preemptive Scheduling** - ✅ COMPLETE
- Task structures (TCB)
- Context switching (with EFLAGS restore!)
- O(1) scheduler (256 priority levels)
- Timer-driven preemption
- Multiple tasks running concurrently
- Cooperative AND preemptive scheduling

**Phase 3.2: Syscalls & Userspace** - 🔨 IN PROGRESS
- ✅ Syscall mechanism (INT 0x80, DPL=3)
- ✅ GDT/TSS setup with per-task ESP0
- ✅ Phase A: Direct syscall_handler tests (getpid, yield, invalid)
- ✅ Phase B: INT 0x80 from ring 0 (getpid, yield, invalid)
- ⏭️ Phase C: First ring 3 task and INT 0x80 from userspace
  - Create user task, map USER stack/code
  - Transition to ring 3 (CS=0x1B, SS=0x23)
  - Verify syscalls work from ring 3

**Phase 4: IPC & Capabilities** - ⏳ TODO
- Message passing
- Capability-based security
- Channels

---

## Build & Test

```bash
# Clean build
make clean && make

# Run (expect to see all 10 iterations + timer ticks)
make run

# Expected output:
# [TEST] Test thread started!
# [TEST] iteration 0, counter=1
# [TEST] iteration 1, counter=2
# ...
# [TEST] iteration 9, counter=10
# [TEST] Test thread exiting (final counter=10)
# [TIMER] Tick 100
# [TIMER] Tick 200
# ...
```

---

## Lessons Learned

### What Worked

1. **Serial console was invaluable**
   - Dual output (VGA + serial) made debugging possible
   - Could see timer ticks and task output interleaved
   - Proved interrupts were firing when VGA scrolled off

2. **Systematic debugging**
   - Added targeted debug output (EFLAGS, tick counters, PIC masks)
   - Discovered bugs by inspection (checked structure vs assembly)
   - Incremental verification at each step

3. **Critical architecture review**
   - User pushed back on "it's working!" claims
   - Forced us to verify actual execution, not just context switches
   - Revealed that test thread wasn't actually resuming

4. **Simplified test case**
   - Removed manual yield, let pure preemption work
   - Added simple counter to verify execution
   - Made it obvious when preemption was working

### Key Insights

1. **Context switch is a contract**
   - `cpu_context_t` defines EVERY field that must be saved
   - Assembly MUST match structure layout EXACTLY
   - Missing even one field (EFLAGS) causes subtle, hard bugs

2. **"Boot loop" was actually missing EFLAGS restore**
   - Earlier debugging thought "enabling interrupts causes crash"
   - Reality: interrupts worked, but context switch didn't restore IF bit
   - After first switch, tasks ran with random EFLAGS

3. **Preemption requires correct priority logic**
   - Can't just check same-priority round-robin
   - Must check for higher-priority tasks ready
   - Idle should always be preempted by any real task

4. **Manual yield complicated testing**
   - Original test called task_yield() explicitly
   - Made it unclear if preemption was working
   - Pure preemptive test (no yield) proved it works

---

## Known Issues

### High Priority

1. **Static MMU Page Table Storage** [Issue #5]
   - `arch/x86/mmu.c:28` uses static 4KB buffer
   - Limits kernel to single address space
   - **Fix:** Allocate page tables dynamically from PMM
   - **Blocks:** Multiple address spaces for userspace

### Medium Priority

2. **GDT Not Initialized** [Issue #3]
   - Currently relying on GRUB's GDT
   - Need own GDT for TSS/syscalls
   - **Required for:** Phase 3.2 userspace tasks

3. **Stack Size Limited to 4096**
   - Only one page per task
   - **Fix:** Multi-page allocation in Phase 4

4. **No Task Cleanup**
   - Zombie tasks not freed
   - **Fix:** Add reaper thread or cleanup in scheduler

---

## What We Just Completed Today ✅

**Phase 3.2 Step 1: GDT + TSS Setup (COMPLETE):**
- ✅ Designed GDT with ring 0 and ring 3 segments
- ✅ Created 8 unit tests for GDT descriptor encoding
- ✅ Implemented arch/x86/gdt.c and include/kernel/gdt.h
- ✅ Added GDT verification with segment register readback
- ✅ Verified: CS=0x08, DS=0x10, SS=0x10, TR=0x28 all correct
- ✅ TSS loaded and ready for syscall stack switching

**Files created:**
- `tests/gdt_test.c` - 8 unit tests, all passing
- `arch/x86/gdt.c` - GDT/TSS implementation (279 lines)
- `include/kernel/gdt.h` - GDT API (62 lines)

**Files modified:**
- `arch/x86/hal.c` - Call gdt_init() in cpu_init()
- `core/init.c` - Call gdt_verify() after console init
- `Makefile` - Added gdt.c and gdt_test.c

## What We Just Completed ✅ (Phase 3.2 Step 2: Syscall Mechanism)

**Completed (2025-11-30):**
- ✅ Designed syscall ABI (Linux i386-compatible register convention)
- ✅ Implemented arch/x86/syscall.s (INT 0x80 entry/exit assembly, ~120 lines)
- ✅ Created syscall dispatcher (core/syscall.c, ~160 lines)
- ✅ Registered INT 0x80 handler in IDT with DPL=3 (type 0xEE)
- ✅ Implemented 4 basic syscalls (exit, yield, getpid, sleep_us)
- ✅ Fixed current_task NULL issue (use g_scheduler.current_task)
- ✅ Updated TSS.esp0 in schedule() for ring 3 stack switching

**Files created (3 new files, ~370 lines):**
- `include/kernel/syscall.h` - Syscall API and numbers (67 lines)
- `arch/x86/syscall.s` - INT 0x80 entry/exit (120 lines)
- `core/syscall.c` - Dispatcher and implementations (160 lines)

**Files modified:**
- `arch/x86/idt.c` - Registered INT 0x80 with DPL=3
- `core/init.c` - Added syscall_init() + Phase A tests
- `core/scheduler.c` - Added TSS.esp0 update before context switch
- `Makefile` - Added syscall.s and syscall.c to build

**Critical fixes:**
- Fixed sys_exit/sys_getpid to use `g_scheduler.current_task` instead of `this_cpu()->current_task`
- Added TSS.esp0 update in schedule() before context_switch()
- Used uniform syscall signature (all take 5 long args, cast unused ones to void)

## 🔨 Phase 3.3: First Userspace Task (IN PROGRESS - 50%)

**Started:** 2025-11-30
**Goal:** Create first ring 3 userspace task that makes syscalls via INT 0x80

### Step 1: Memory Layout Design ✅ COMPLETE

**Completed:**
- Defined userspace virtual memory layout
- Created `include/kernel/user.h` with constants

**Memory Layout:**
```
0x00000000 - 0x00400000: Reserved (NULL pointer guard)
0x00400000 - 0x00800000: User code & data (4MB, USER|PRESENT)
0xBFFFF000 - 0xC0000000: User stack (4KB, USER|PRESENT|WRITABLE)
0xC0000000 - 0xFFFFFFFF: Kernel space (PRESENT only, no USER)
```

**Constants Defined:**
- USER_CODE_BASE = 0x00400000
- USER_STACK_TOP = 0xC0000000
- USER_CS_SELECTOR = 0x1B (GDT entry 3, RPL=3)
- USER_DS_SELECTOR = 0x23 (GDT entry 4, RPL=3)

**Files created:**
- `include/kernel/user.h` - Userspace constants and API

### Step 2: Hybrid Context Switch ✅ COMPLETE

**Completed:**
- Implemented iret-based context switch with kernel/user path selection
- Kernel→Kernel: Fast path using `jmp *eip`
- Kernel→User: Privilege switch using `iret` instruction

**Implementation:**
Changed `arch/x86/context.s` to check target CS.RPL:
- If RPL == 0 (kernel): restore ESP manually, `jmp` to EIP
- If RPL == 3 (user): build iret frame, use `iret` for atomic privilege switch

**Why This Works:**
- `iret` from ring 0 to ring 0: only pops EIP/CS/EFLAGS (doesn't switch stacks)
- `iret` from ring 0 to ring 3: pops EIP/CS/EFLAGS/ESP/SS (switches to user stack)
- Hybrid approach: kernel fast path avoids unnecessary iret overhead

**Test Results:**
- ✅ Phase A tests still pass
- ✅ Phase B tests still pass
- ✅ Kernel→kernel context switches work
- ✅ Ready for kernel→user switches

**Files modified:**
- `arch/x86/context.s` - Lines 106-130 (hybrid iret/jmp path)

### Step 3: Userspace Test Program ✅ COMPLETE

**Completed:**
- Created simple ring 3 test program in assembly
- Tests SYS_GETPID, SYS_YIELD (5x), SYS_EXIT

**Test Program Flow:**
```asm
1. Call SYS_GETPID (verify syscall works from ring 3)
2. Loop 5x calling SYS_YIELD (verify context switching)
3. Call SYS_EXIT with code 42 (clean exit)
```

**Files created:**
- `arch/x86/user_test.s` - Userspace test program (~40 lines)
- `Makefile` - Added user_test.s to ASM_SOURCES

### Step 4: MMU User Page Mapping 🔨 NEXT

**TODO:**
- Add `page_map_user()` function to MMU
- Map pages with USER|PRESENT|WRITABLE flags
- Copy userspace code to mapped pages

### Step 5: Ring 3 Task Initialization 🔨 TODO

**TODO:**
- Implement `task_create_user()` function
- Set CS=0x1B, SS/DS/ES=0x23
- Set initial ESP to USER_STACK_TOP
- Set EFLAGS with IF=1

### Step 6: Testing 🔨 TODO

**TODO:**
- Create user task from init
- Verify INT 0x80 works from ring 3
- Test privilege separation

---

## ✅ Phase 3.2 Step 5: Phase B Testing (COMPLETE)

**Completed (2025-11-30):**
- ✅ Created syscall_int80() helper in arch/x86/syscall.s
- ✅ Added Phase B tests (INT 0x80 from ring 0)
- ✅ All tests pass with fixed argument marshalling

**Phase B Implementation:**
Created `syscall_int80()` wrapper function to invoke INT 0x80 from C code:
- Loads 6 arguments into registers (EAX=syscall_num, EBX-EDI=args)
- Executes `int $0x80` instruction
- Returns result in EAX
- Follows cdecl calling convention (saves/restores callee-saved registers)

**Test Results:**
```
[TEST] === Phase B: INT 0x80 from ring 0 ===
[TEST] INT 0x80 SYS_GETPID returned: 1           ✅
[TEST] INT 0x80 SYS_YIELD returned: 0             ✅
[TEST] INT 0x80 invalid syscall returned: -38    ✅
[TEST] Phase B tests complete!
```

**Proof of Argument Marshalling Fix:**
Phase B validates the fix from Step 4 - arguments are now correctly loaded into temporaries before pushing, so syscalls receive proper values instead of garbage.

**Files modified:**
- `arch/x86/syscall.s` - Added syscall_int80() helper (lines 132-160)
- `core/init.c` - Added Phase B tests using syscall_int80()

## ✅ Phase 3.2 Step 4: Critical Bug Fixes (COMPLETE)

**Completed (2025-11-30):**
- ✅ Fixed INT 0x80 argument marshalling bug (syscall.s)
- ✅ Fixed CS register not saved/restored in context_switch (context.s)
- ✅ Added proper SS restoration (was missing!)
- ✅ Documented sys_sleep_us as STUB implementation
- ✅ Removed heavy kprintf calls from hot paths

**INT 0x80 Argument Marshalling Fix:**
Problem: Stack offsets calculated BEFORE pushes, so subsequent pushes invalidated offsets
- Each `pushl offset(%esp)` changed ESP, making next offset wrong
- Result: Syscalls received garbage arguments
- Fix: Load all 6 args into registers BEFORE pushing them

**CS/SS Register Save/Restore Fix:**
Problem: cpu_context_t includes cs/ss but context_switch never saved/restored them
- CS at offset +24 was never touched
- SS was saved but never restored
- Result: Would crash when switching to userspace tasks (different CS)
- Fix: Save CS/SS with `movw %cs, %cx`, restore SS with `movl %ecx, %ss`
- Note: CS restoration deferred to iret-based switch for userspace

**sys_sleep_us Documentation:**
- Clearly documented as STUB that does NOT sleep for requested duration
- Just yields once to prove syscall mechanism works
- Full implementation deferred to Phase 4 (sleep queues + timer wakeup)

**Hot Path Optimization:**
- Removed kprintf from syscall_handler (invalid syscall cases)
- Prevents console reentrancy from IRQ context
- Reduces timing skew during syscall testing

**Files modified:**
- `arch/x86/syscall.s` - Fixed arg marshalling (lines 68-85)
- `arch/x86/context.s` - Added CS/SS save/restore (lines 59-62, 87-96)
- `core/syscall.c` - Documented sys_sleep_us as STUB, removed hot path kprintf

## ✅ Phase 3.2 Step 3: Context Switch Bug Fix & Phase A Testing (COMPLETE)

**Completed (2025-11-30):**
- ✅ Fixed critical context switch bug (call/pop trick saved wrong EIP)
- ✅ Rewrote context_switch to use ESP-based offsets (removed frame pointer dependency)
- ✅ All Phase A syscall tests passing
- ✅ Cleaned up excessive debug output from scheduler and syscalls

**Context Switch Fix:**
Changed `arch/x86/context.s` from EBP-based frame unwinding to ESP-based:
- Save EIP from `0(%esp)` (actual return address on stack)
- Save ESP as `esp + 12` (caller's ESP after function returns)
- Save EBP value directly without dereferencing (avoids page faults)
- Removed prologue entirely, no frame pointer dependency

**sys_sleep_us Fix:**
Simplified from timer-based busy-wait loop to single yield:
- Original crashed due to rapid context switches with timer reads
- Deferred full sleep queue implementation to Phase 4
- Single yield proves syscall mechanism works correctly

**Phase A Test Results:**
- ✅ sys_getpid() returns correct task ID (1)
- ✅ sys_yield() successfully context switches
- ✅ Invalid syscall returns -ENOSYS (-38)
- ✅ sys_sleep_us() works with simplified implementation

**Files modified:**
- `arch/x86/context.s` - Fixed EIP/ESP save logic (lines 27-56)
- `core/syscall.c` - Simplified sys_sleep_us, removed debug output
- `core/scheduler.c` - Removed excessive debug logging
- `SCHEDULER_CONTEXT_SWITCH_BUG.md` - Documented resolution
- `CURRENT_WORK.md` - Updated status (Phase 3.2 complete!)

## ✅ Phase 3.2 COMPLETE! Syscalls Working!

**Status:** ✅ COMPLETE - Context switching and syscalls fully operational!
**Date Completed:** 2025-11-30

### Context Switch Bug Resolution

**Status:** ✅ RESOLVED - Context switching now works correctly!
**Date Fixed:** 2025-11-30

**Root Causes:**
1. **Wrong EIP save**: Original code used `call/pop` trick that saved address INSIDE context_switch, not caller's return address
2. **Frame pointer dependency**: Code relied on EBP being stable, but compiler omits frame pointers by default

**The Fix:** Changed `arch/x86/context.s` to use ESP-based offsets (lines 27-56):
- Removed prologue - no frame pointer needed
- Save EIP from `0(%esp)` (actual return address on stack)
- Save ESP as `esp + 12` (caller's ESP after return)
- Save EBP value directly without dereferencing

**Results:**
- ✅ sys_getpid() works
- ✅ sys_yield() works and returns properly!
- ✅ Context switches work in both directions
- ✅ Multiple yields work correctly

**See:** `SCHEDULER_CONTEXT_SWITCH_BUG.md` for detailed analysis

## Immediate Next Actions

**Phase A Tests:** ✅ COMPLETE!
1. ✅ sys_getpid() - working (returns task ID 1)
2. ✅ sys_yield() - working (context switches successfully)
3. ✅ Invalid syscall handling - working (returns -ENOSYS)
4. ✅ sys_sleep_us() - working (simplified to single yield, full timer-based sleep deferred to Phase 4)

**Phase B Tests:** ✅ COMPLETE!
1. ✅ INT 0x80 SYS_GETPID - working (returns task ID 1)
2. ✅ INT 0x80 SYS_YIELD - working (context switches successfully)
3. ✅ INT 0x80 invalid syscall - working (returns -ENOSYS)

**Next Steps:**
1. 🔨 Move to Phase 3.3 (first userspace task in ring 3)

**Phase 3.3: First Userspace Task:**
- Create simple userspace binary
- Map code + stack with USER flag
- Transition to ring 3 with iret
- Test INT 0x80 from ring 3 (full end-to-end syscall from userspace)

---

**Phase 3.1 COMPLETE! Preemptive multitasking is working!** 🎉

Next stop: Syscalls and userspace!
