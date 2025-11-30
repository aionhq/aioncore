/**
 * Simple Userspace Test Program
 *
 * This is a minimal ring 3 program that tests syscalls from userspace.
 * It will be mapped at USER_CODE_BASE (0x00400000) with USER flag.
 *
 * Test sequence:
 * 1. Call SYS_GETPID (should return task ID)
 * 2. Call SYS_YIELD (should context switch)
 * 3. Loop a few times
 * 4. Call SYS_EXIT (terminate)
 */

.section .text
.global user_test_start
.global user_test_end

user_test_start:
    # Test 1: SYS_GETPID
    movl $3, %eax          # SYS_GETPID
    movl $0, %ebx
    movl $0, %ecx
    movl $0, %edx
    movl $0, %esi
    movl $0, %edi
    int $0x80
    # EAX now contains our task ID

    # Test 2: SYS_YIELD (do this a few times)
    movl $5, %ecx          # Loop counter
yield_loop:
    movl $2, %eax          # SYS_YIELD
    int $0x80
    decl %ecx
    jnz yield_loop

    # Test 3: SYS_EXIT
    movl $1, %eax          # SYS_EXIT
    movl $42, %ebx         # Exit code = 42
    int $0x80

    # Should never reach here
    hlt

user_test_end:
    # Marker for end of user code (used to calculate size)
