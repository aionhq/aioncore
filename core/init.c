// Kernel Initialization
// This is called from boot.s after basic CPU setup

#include <kernel/hal.h>
#include <kernel/percpu.h>
#include <kernel/types.h>
#include <kernel/idt.h>
#include <kernel/pmm.h>
#include <kernel/mmu.h>
#include <drivers/vga.h>

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

    // Display welcome banner
    vga_clear();
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("AionCore v%d.%d.%d\n", KERNEL_VERSION_MAJOR,
            KERNEL_VERSION_MINOR, KERNEL_VERSION_PATCH);

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("RT Microkernel - Phase 2\n");

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

    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    kprintf("\nKernel initialization complete!\n");

    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    kprintf("Ready for next phase: paging and tasks\n");

    // TODO: Phase 2 remaining: Initialize basic paging
    // TODO: Phase 3: Task structures & scheduler
    // TODO: Phase 3: Syscall entry/exit
    // TODO: Phase 4: IPC & Capabilities

    // Enable interrupts (timer will now fire)
    kprintf("\nEnabling interrupts...\n");
    hal->irq_enable();

    // Idle loop - print timer ticks periodically
    kprintf("Entering idle loop (timer running at 1000 Hz)...\n");
    kprintf("Press Ctrl+A then X to exit QEMU\n\n");

    uint64_t last_tick = 0;
    while (1) {
        // Print tick count every 1000 ticks (1 second at 1000 Hz)
        uint64_t current_tick = this_cpu()->ticks;
        if (current_tick - last_tick >= 1000) {
            uint64_t time_us = hal->timer_read_us();
            kprintf("Tick: %llu (Time: %llu.%06llu seconds)\n",
                    current_tick,
                    time_us / 1000000ULL,
                    time_us % 1000000ULL);
            last_tick = current_tick;
        }

        hal->cpu_halt();
    }
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
