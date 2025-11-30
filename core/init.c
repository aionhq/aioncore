// Kernel Initialization
// This is called from boot.s after basic CPU setup

#include <kernel/hal.h>
#include <kernel/percpu.h>
#include <kernel/types.h>
#include <kernel/idt.h>
#include <kernel/gdt.h>
#include <kernel/pmm.h>
#include <kernel/mmu.h>
#include <kernel/task.h>
#include <kernel/scheduler.h>
#include <kernel/syscall.h>
#include <kernel/user.h>
#include <kernel/console.h>
#include <drivers/vga.h>
#include <drivers/serial.h>

// Forward declarations
extern void hal_x86_init(void);

// Kernel version
#define KERNEL_VERSION_MAJOR 0
#define KERNEL_VERSION_MINOR 1
#define KERNEL_VERSION_PATCH 0

// Kernel entry point (called from boot.s)
// Parameters: multiboot_magic, multiboot_info_addr (passed from boot.s)
__attribute__((noreturn)) void kmain(uint32_t multiboot_magic, uint32_t multiboot_info_addr) {
    // Phase 1: Initialize HAL (Hardware Abstraction Layer)
    // This must happen first - provides CPU, interrupt, I/O operations
    hal_x86_init();

    // Phase 2: Initialize per-CPU infrastructure
    // Sets up per-CPU data structures for this boot CPU
    percpu_init();

    // Phase 3: Initialize VGA display
    // Now we can print to screen!
    vga_subsystem_init();

    // Phase 3.5: Initialize console multiplexer
    // Register VGA and serial backends so kprintf goes to both
    console_init();
    console_register(vga_get_console_backend());
    console_register(serial_get_console_backend());

    // Verify GDT was loaded correctly (after console init so output is visible)
    gdt_verify();
    kprintf("\n");

    // Display welcome banner
    vga_clear();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("AionCore v%d.%d.%d\n", KERNEL_VERSION_MAJOR,
            KERNEL_VERSION_MINOR, KERNEL_VERSION_PATCH);

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("RT Microkernel - Phase 3\n");

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("\n");

    // Display initialization status
    kprintf("[OK] HAL initialized (x86 architecture)\n");
    kprintf("[OK] Per-CPU data initialized (CPU #%u)\n", (unsigned int)this_cpu()->cpu_id);
    kprintf("[OK] VGA text driver loaded\n");
    kprintf("[OK] IDT initialized (exceptions + IRQs)\n");

    // Phase 4: Initialize timer
    // Initialize PIT at 1000 Hz and calibrate TSC
    kprintf("\n");
    hal->timer_init(1000);

    // Phase 5: Initialize physical memory manager
    kprintf("\n");
    struct multiboot_info *mbi = (struct multiboot_info *)(uintptr_t)multiboot_info_addr;
    pmm_init(multiboot_magic, mbi);

    // Phase 6: Initialize MMU and enable paging
    kprintf("\n");
    mmu_init();

    // Phase 7: Initialize task subsystem
    kprintf("\n");
    task_init();

    // Phase 8: Initialize scheduler
    scheduler_init();

    // Phase 9: Initialize syscalls
    kprintf("\n");
    syscall_init();

#ifdef KERNEL_TESTS
    // Run kernel self-tests
    extern int ktest_run_all(void);
    int test_failures = ktest_run_all();

    if (test_failures > 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("\n[ERROR] %d test(s) failed!\n", test_failures);
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    } else {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        kprintf("\n[SUCCESS] All tests passed!\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }
#endif

    // Display CPU information
    uint32_t features = hal->cpu_features();
    kprintf("\nCPU Features: ");
    if (features & HAL_CPU_FEAT_FPU)  kprintf("FPU ");
    if (features & HAL_CPU_FEAT_SSE)  kprintf("SSE ");
    if (features & HAL_CPU_FEAT_SSE2) kprintf("SSE2 ");
    if (features & HAL_CPU_FEAT_PAE)  kprintf("PAE ");
    if (features & HAL_CPU_FEAT_APIC) kprintf("APIC ");
    kprintf("\n");

    // Display memory info
    kprintf("\nMemory Layout:\n");
    kprintf("  Kernel: 0x%08x\n", 0x100000);
    kprintf("  Per-CPU data: 0x%08x\n", (unsigned int)(uintptr_t)&per_cpu[0]);

    // Test thread entry point
    extern void test_thread_entry(void* arg);

    // Phase C: Test userspace task (ring 3)
    kprintf("\n[TEST] === Phase C: Userspace Task (Ring 3) ===\n");
    kprintf("[TEST] Creating userspace task...\n");

    task_t* user_task = task_create_user("user_test", NULL, 0);
    if (user_task) {
        scheduler_enqueue(user_task);
        kprintf("[TEST] Userspace task created and enqueued\n");
    } else {
        kprintf("[TEST] ERROR: Failed to create userspace task\n");
    }

    // Create a test kernel thread
    task_t* test_task = task_create_kernel_thread("test_thread",
                                                    test_thread_entry,
                                                    NULL,
                                                    SCHED_DEFAULT_PRIORITY,
                                                    4096);
    if (test_task) {
        scheduler_enqueue(test_task);
        kprintf("[INIT] Test thread created and enqueued\n");
    }

    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    kprintf("\nKernel initialization complete!\n");

    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Ready: Tasks and scheduler operational\n");

    // TODO: Phase 3: Syscall entry/exit
    // TODO: Phase 4: IPC & Capabilities

    // Enable interrupts for timer-driven preemption/testing
    kprintf("\nEnabling interrupts and yielding to scheduler...\n");
    kprintf("Press Ctrl+A then X to exit QEMU\n\n");

    hal->irq_enable();

    // Drop into scheduler
    task_yield();

    // Should never reach here
    kprintf("[INIT] ERROR: Returned from idle task!\n");
    while (1) {
        hal->cpu_halt();
    }
}

// Global counter to verify test thread is actually running
volatile int test_counter = 0;

// Test thread for Phase 3
void test_thread_entry(void* arg) {
    (void)arg;

    kprintf("[TEST] Test thread started!\n");

    // Wait for timer ticks to confirm interrupts are working
    kprintf("[TEST] Waiting for timer interrupts...\n");
    extern struct per_cpu_data per_cpu[];
    uint64_t start_ticks = per_cpu[0].ticks;
    uint64_t timeout = 10000000;  // Safety timeout
    for (volatile uint64_t i = 0; i < timeout; i++) {
        if (per_cpu[0].ticks > start_ticks + 10) {
            kprintf("[TEST] Timer confirmed working (saw %llu ticks)\n\n",
                    (unsigned long long)(per_cpu[0].ticks - start_ticks));
            break;
        }
    }
    if (per_cpu[0].ticks <= start_ticks) {
        kprintf("[TEST] WARNING: No timer ticks detected! Interrupts may be disabled!\n\n");
    }

    // Phase A: Test direct syscall_handler() calls
    // (Now safe to test yields because timer will preempt idle and bring us back)
    kprintf("[TEST] === Phase A: Direct syscall_handler() calls ===\n");

    // Test 1: sys_getpid
    kprintf("[TEST] Testing sys_getpid()...\n");
    long pid = syscall_handler(SYS_GETPID, 0, 0, 0, 0, 0);
    kprintf("[TEST] sys_getpid() returned: %ld\n", pid);

    // Test 2: sys_yield
    kprintf("[TEST] Testing sys_yield()...\n");
    long ret = syscall_handler(SYS_YIELD, 0, 0, 0, 0, 0);
    kprintf("[TEST] sys_yield() returned: %ld\n", ret);

    // Test 3: Invalid syscall
    kprintf("[TEST] Testing invalid syscall (999)...\n");
    ret = syscall_handler(999, 0, 0, 0, 0, 0);
    kprintf("[TEST] Invalid syscall returned: %ld (expected -38)\n", ret);

    // Test 4: sys_sleep_us (disabled for now – needs stable sleep queues)
    // kprintf("[TEST] Testing sys_sleep_us(100000) - 100ms...\n");
    // ret = syscall_handler(SYS_SLEEP_US, 100000, 0, 0, 0, 0);
    // kprintf("[TEST] sys_sleep_us() returned: %ld\n", ret);

    kprintf("[TEST] Phase A tests complete!\n\n");

    // Phase B: Test INT 0x80 from kernel mode (ring 0)
    // Declared in arch/x86/syscall.s (inline asm not allowed in core/ code)
    extern long syscall_int80(long syscall_num, long arg0, long arg1, long arg2, long arg3, long arg4);

    kprintf("[TEST] === Phase B: INT 0x80 from ring 0 ===\n");

    // Test 1: sys_getpid via INT 0x80
    kprintf("[TEST] Testing INT 0x80 with SYS_GETPID...\n");
    long result = syscall_int80(SYS_GETPID, 0, 0, 0, 0, 0);
    kprintf("[TEST] INT 0x80 SYS_GETPID returned: %ld\n", result);

    // Test 2: sys_yield via INT 0x80
    kprintf("[TEST] Testing INT 0x80 with SYS_YIELD...\n");
    result = syscall_int80(SYS_YIELD, 0, 0, 0, 0, 0);
    kprintf("[TEST] INT 0x80 SYS_YIELD returned: %ld\n", result);

    // Test 3: Invalid syscall via INT 0x80
    kprintf("[TEST] Testing INT 0x80 with invalid syscall (999)...\n");
    result = syscall_int80(999, 0, 0, 0, 0, 0);
    kprintf("[TEST] INT 0x80 invalid syscall returned: %ld (expected -38)\n", result);

    kprintf("[TEST] Phase B tests complete!\n\n");

    // Run for a while, incrementing counter
    for (int i = 0; i < 5; i++) {
        test_counter++;
        kprintf("[TEST] iteration %d, counter=%d\n", i, test_counter);

        // Don't yield manually - let preemption happen
        // Simulate some work
        for (volatile int j = 0; j < 1000000; j++) {
            // Busy wait
        }
    }

    kprintf("[TEST] Test thread exiting (final counter=%d)\n", test_counter);
    task_exit(0);
}

// Kernel panic handler
__attribute__((noreturn)) void kernel_panic(const char* message) {
    // Disable interrupts
    if (hal && hal->irq_disable) {
        hal->irq_disable();
    }

    // Red screen of death
    if (vga) {
        vga->set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
        vga->clear();
        kprintf("*** KERNEL PANIC ***\n\n");
        kprintf("%s\n", message);
        kprintf("\nSystem halted.");
    }

    // Halt forever
    while (1) {
        if (hal && hal->cpu_halt) {
            hal->cpu_halt();
        }
    }
}
