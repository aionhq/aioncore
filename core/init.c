// Kernel Initialization
// This is called from boot.s after basic CPU setup

#include <kernel/hal.h>
#include <kernel/percpu.h>
#include <kernel/types.h>
#include <kernel/idt.h>
#include <kernel/pmm.h>
#include <kernel/mmu.h>
#include <kernel/task.h>
#include <kernel/scheduler.h>
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

// Test thread for Phase 3
void test_thread_entry(void* arg) {
    (void)arg;

    kprintf("[TEST] Test thread started!\n");

    // Run for a while, printing periodically
    for (int i = 0; i < 10; i++) {
        kprintf("[TEST] Test thread iteration %d\n", i);

        // Yield to other tasks
        task_yield();

        // Simulate some work
        for (volatile int j = 0; j < 100000; j++) {
            // Busy wait
        }
    }

    kprintf("[TEST] Test thread exiting\n");
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
