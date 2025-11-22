#ifndef KERNEL_HAL_H
#define KERNEL_HAL_H

#include <stdint.h>
#include <stddef.h>
#include <kernel/types.h>

// Virtual address type (phys_addr_t is defined in types.h)
typedef uint32_t virt_addr_t;

// Hardware Abstraction Layer Operations
// This interface isolates hardware-specific code from the kernel core
struct hal_ops {
    // ========== CPU Management ==========

    // Initialize CPU-specific features (GDT, IDT, etc.)
    void (*cpu_init)(void);

    // Get current CPU ID (for SMP systems)
    uint32_t (*cpu_id)(void);

    // Halt the CPU until next interrupt
    void (*cpu_halt)(void);

    // Detect CPU features (SSE, PAE, etc.)
    uint32_t (*cpu_features)(void);

    // ========== Interrupt Management ==========

    // Enable interrupts globally
    void (*irq_enable)(void);

    // Disable interrupts globally (returns previous state)
    uint32_t (*irq_disable)(void);

    // Restore interrupt state
    void (*irq_restore)(uint32_t state);

    // Register an interrupt handler
    int (*irq_register)(uint8_t vector, void (*handler)(void));

    // Unregister an interrupt handler
    void (*irq_unregister)(uint8_t vector);

    // ========== Memory Management ==========

    // Initialize MMU (paging, TLB, etc.)
    void (*mmu_init)(void);

    // Map a physical page to virtual address
    void* (*mmu_map)(phys_addr_t phys, virt_addr_t virt, uint32_t flags);

    // Unmap a virtual page
    void (*mmu_unmap)(virt_addr_t virt);

    // Flush TLB for a specific address
    void (*mmu_flush_tlb)(virt_addr_t virt);

    // Flush entire TLB
    void (*mmu_flush_tlb_all)(void);

    // ========== I/O Operations ==========

    // Port I/O (x86-specific, but abstracted)
    uint8_t (*io_inb)(uint16_t port);
    uint16_t (*io_inw)(uint16_t port);
    uint32_t (*io_inl)(uint16_t port);

    void (*io_outb)(uint16_t port, uint8_t value);
    void (*io_outw)(uint16_t port, uint16_t value);
    void (*io_outl)(uint16_t port, uint32_t value);

    // Memory-mapped I/O
    void* (*mmio_map)(phys_addr_t phys, size_t size);
    void (*mmio_unmap)(void* virt, size_t size);

    // ========== SMP/Multicore Operations ==========

    // Get number of CPUs
    uint32_t (*smp_num_cpus)(void);

    // Send inter-processor interrupt
    void (*smp_send_ipi)(uint32_t cpu_id, uint8_t vector);

    // Broadcast IPI to all CPUs
    void (*smp_broadcast_ipi)(uint8_t vector);

    // Boot a secondary CPU (AP)
    int (*smp_boot_cpu)(uint32_t cpu_id, void (*entry_point)(void));

    // ========== Timer Operations ==========

    // Get timestamp counter (cycles)
    uint64_t (*timer_read_tsc)(void);

    // Get time in microseconds
    uint64_t (*timer_read_us)(void);

    // Initialize timer hardware
    void (*timer_init)(uint32_t frequency_hz);

    // ========== Special Operations ==========

    // Reboot the system
    void (*system_reboot)(void);

    // Shutdown the system (if supported)
    void (*system_shutdown)(void);

    // Panic handler (architecture-specific stack trace)
    void (*panic)(const char* message);
};

// Global HAL pointer (initialized during boot)
extern struct hal_ops* hal;

// Page mapping flags (common across architectures)
#define HAL_PAGE_PRESENT   (1 << 0)  // Page is present in memory
#define HAL_PAGE_WRITABLE  (1 << 1)  // Page is writable
#define HAL_PAGE_USER      (1 << 2)  // Page is accessible from user mode
#define HAL_PAGE_NOCACHE   (1 << 3)  // Disable caching (for MMIO)
#define HAL_PAGE_NOEXEC    (1 << 4)  // Page is not executable (NX bit)

// CPU feature flags
#define HAL_CPU_FEAT_FPU   (1 << 0)
#define HAL_CPU_FEAT_SSE   (1 << 1)
#define HAL_CPU_FEAT_SSE2  (1 << 2)
#define HAL_CPU_FEAT_PAE   (1 << 3)
#define HAL_CPU_FEAT_APIC  (1 << 4)

// Initialize HAL for specific architecture
void hal_init(void);

#endif // KERNEL_HAL_H
