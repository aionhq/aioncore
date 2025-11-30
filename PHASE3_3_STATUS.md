# Phase 3.3: First Userspace Task - Status

**Started:** 2025-11-30
**Completed:** 2025-11-30
**Status:** ✅ COMPLETE (100%)
**Goal:** Create first ring 3 userspace task that executes syscalls via INT 0x80

---

## Progress Summary

### ✅ Completed (6/6)

1. **Memory Layout Design** - 100%
   - Defined userspace virtual address space
   - Created constants for USER_CODE_BASE, USER_STACK_TOP
   - Documented GDT selectors (CS=0x1B, SS=0x23)

2. **Hybrid Context Switch** - 100%
   - Implemented dual-path context switch with CS.RPL check
   - Kernel→Kernel: Fast `jmp` path
   - Kernel→User: `iret` privilege switch
   - **Critical fix:** Check privilege BEFORE loading segment registers
   - **Verified:** Phase A/B tests still pass

3. **Userspace Test Program** - 100%
   - Created user_test.s assembly program
   - Tests: SYS_GETPID, SYS_YIELD (5x), SYS_EXIT (42)
   - Added to Makefile build
   - Embedded in kernel via symbols

4. **MMU User Page Mapping** - 100%
   - Used existing `mmu_map_page()` with MMU_USER flag
   - Mapped code at 0x00400000 with USER|PRESENT|WRITABLE
   - Mapped stack at 0xBFFFF000 with USER|PRESENT|WRITABLE
   - Copied userspace binary to mapped pages

5. **Ring 3 Task Initialization** - 100%
   - Implemented `task_create_user()` in core/user.c
   - Set CS=0x1B, SS/DS/ES/FS/GS=0x23
   - Set EIP=USER_CODE_BASE, ESP=USER_STACK_TOP
   - Set EFLAGS=0x202 (IF=1)
   - Allocated kernel stack for syscall entry
   - TSS.esp0 updated in scheduler

6. **Testing & Verification** - 100%
   - User task runs successfully in ring 3
   - All syscalls work via INT 0x80
   - Task exits cleanly with code 42
   - Privilege separation working
   - Added regression test: `make test-user`

---

## Technical Details

### Memory Layout

```
Virtual Address Space:
┌─────────────────────────────────────┐
│ 0xFFFFFFFF                          │
│    Kernel Space                     │
│    (PRESENT, no USER flag)          │
│ 0xC0000000 ◄─── USER_STACK_TOP      │
├─────────────────────────────────────┤
│    User Stack (4KB)                 │
│    (USER|PRESENT|WRITABLE)          │
│ 0xBFFFF000                          │
│         ...                         │
│ 0x00800000                          │
├─────────────────────────────────────┤
│    User Code & Data (4MB)           │
│    (USER|PRESENT)                   │
│ 0x00400000 ◄─── USER_CODE_BASE      │
├─────────────────────────────────────┤
│    NULL Guard (4MB)                 │
│ 0x00000000                          │
└─────────────────────────────────────┘
```

### GDT Selectors

| Segment | Index | Base | Limit | DPL | RPL | Selector |
|---------|-------|------|-------|-----|-----|----------|
| Kernel Code | 1 | 0x0 | 4GB | 0 | 0 | 0x08 |
| Kernel Data | 2 | 0x0 | 4GB | 0 | 0 | 0x10 |
| **User Code** | 3 | 0x0 | 4GB | **3** | **3** | **0x1B** |
| **User Data** | 4 | 0x0 | 4GB | **3** | **3** | **0x23** |
| TSS | 5 | &tss | 104 | 0 | 0 | 0x28 |

### Hybrid Context Switch Logic

```asm
# Check target privilege level
movl 24(%edx), %ecx     # Load target CS
andl $0x03, %ecx        # Extract RPL
cmpl $0, %ecx           # RPL == 0?
je .context_switch_kernel

.context_switch_user:
    # Ring 0 → Ring 3
    pushl SS                # Build iret frame
    pushl ESP
    pushl EFLAGS
    pushl CS
    pushl EIP
    iret                    # Atomic privilege switch

.context_switch_kernel:
    # Ring 0 → Ring 0
    movl 16(%edx), %esp     # Restore ESP manually
    jmp *20(%edx)           # Jump to EIP
```

### Userspace Test Program

```asm
user_test_start:
    # Test 1: SYS_GETPID
    movl $3, %eax
    int $0x80

    # Test 2: SYS_YIELD (loop 5x)
    movl $5, %ecx
yield_loop:
    movl $2, %eax
    int $0x80
    decl %ecx
    jnz yield_loop

    # Test 3: SYS_EXIT (code 42)
    movl $1, %eax
    movl $42, %ebx
    int $0x80
```

---

## Next Steps

### Step 4: MMU User Page Mapping

**Objective:** Add support for mapping pages with USER flag

**Tasks:**
1. Add `page_map_user()` function to arch/x86/mmu.c
2. Support USER|PRESENT|WRITABLE flags
3. Copy userspace code to mapped pages

**Signature:**
```c
void* page_map_user(uintptr_t virt_addr, size_t size, uint32_t flags);
```

### Step 5: Ring 3 Task Initialization

**Objective:** Create userspace task with ring 3 segments

**Tasks:**
1. Implement `task_create_user()` in core/task.c
2. Allocate and map user code/stack pages
3. Copy user_test code to USER_CODE_BASE
4. Initialize context:
   - CS = 0x1B (user code, RPL=3)
   - SS/DS/ES = 0x23 (user data, RPL=3)
   - ESP = USER_STACK_TOP
   - EIP = USER_CODE_BASE
   - EFLAGS = 0x202 (IF=1)

**Signature:**
```c
task_t* task_create_user(const char* name, void* code, size_t code_size);
```

### Step 6: Testing

**Objective:** Verify userspace task execution

**Tests:**
1. Create user task from init.c
2. Schedule user task
3. Verify INT 0x80 works from ring 3
4. Check privilege separation (can't access kernel memory)
5. Verify syscalls return correctly
6. Confirm clean exit

---

## Files Modified

**Created:**
- `include/kernel/user.h` - Userspace constants & API
- `arch/x86/user_test.s` - Userspace test program
- `PHASE3_3_STATUS.md` - This status document

**Modified:**
- `arch/x86/context.s` - Hybrid iret/jmp context switch
- `Makefile` - Added user_test.s to build
- `CURRENT_WORK.md` - Updated status
- `docs/TODO.md` - Updated with Phase 3.3 progress

**To Create:**
- `arch/x86/mmu.c` - Add `page_map_user()` function
- `core/task.c` - Add `task_create_user()` function

---

## Blockers & Risks

**Blockers:**
- None - all prerequisites complete

**Risks:**
1. **Page fault on user code** - Need correct USER|PRESENT flags
2. **Stack overflow** - Only 4KB user stack
3. **MMU bugs** - Page mapping must be correct
4. **Privilege violations** - Must verify ring 3 can't access kernel

**Mitigation:**
- Extensive logging during initial testing
- Start with simple test program
- Verify page flags before execution
- Test with QEMU debugger if needed

---

## Success Criteria

Phase 3.3 is complete when:

✅ User task created with ring 3 segments
✅ User code mapped at USER_CODE_BASE with USER flag
✅ User stack mapped at USER_STACK_TOP with USER|WRITABLE
✅ Task switches to ring 3 (CPL=3)
✅ INT 0x80 from ring 3 successfully enters kernel
✅ Syscalls (getpid, yield, exit) work from userspace
✅ Task exits cleanly with code 42
✅ Privilege separation enforced (no kernel access from ring 3)

---

## Code Quality Improvements (Post-completion)

### Implemented
1. **ENOSYS constant** - Defined in syscall.h, used throughout
2. **sys_sleep_us** - Returns -ENOSYS instead of misleading stub
3. **Regression test** - Added `make test-user` target
4. **Hot-path cleanup** - Removed temporary logging from syscalls

### Files Created
- `include/kernel/user.h` - Userspace constants and API
- `core/user.c` - Userspace task creation (task_create_user)
- `arch/x86/user_test.s` - Ring 3 test program

### Files Modified
- `arch/x86/context.s` - Hybrid iret/jmp context switch
- `core/task.c` - Added task_alloc() helper
- `include/kernel/task.h` - Added task_alloc() declaration
- `core/syscall.c` - Added ENOSYS constant, cleaned up hot paths
- `include/kernel/syscall.h` - Defined ENOSYS, documented sys_sleep_us status
- `Makefile` - Added user.c, user_test.s, test-user target
- `core/init.c` - Added Phase C userspace test

---

**Last Updated:** 2025-11-30
**Status:** ✅ Phase 3.3 COMPLETE
**Next Milestone:** Phase 3.4 - Per-task address spaces
