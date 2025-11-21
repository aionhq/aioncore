// x86 Hardware Abstraction Layer Implementation

#include <kernel/hal.h>
#include <kernel/types.h>

// CPU feature detection using CPUID
static uint32_t detect_cpu_features(void) {
    uint32_t features = 0;

    // TODO: Check if CPUID is supported and use it
    // For now, assume basic features

    features |= HAL_CPU_FEAT_FPU;   // x86 always has FPU

    return features;
}

// ========== CPU Management ==========

// Forward declaration
extern void idt_init(void);

static void cpu_init(void) {
    // TODO: Initialize GDT (Global Descriptor Table)
    // For now, boot.s and GRUB set up a basic GDT

    // Initialize IDT (Interrupt Descriptor Table)
    idt_init();
}

static uint32_t cpu_id(void) {
    // For single-CPU system, always return 0
    // TODO: Read from APIC for SMP systems
    return 0;
}

static void cpu_halt(void) {
    __asm__ volatile("hlt");
}

static uint32_t cpu_features(void) {
    return detect_cpu_features();
}

// ========== Interrupt Management ==========

static void irq_enable(void) {
    __asm__ volatile("sti");
}

static uint32_t irq_disable(void) {
    uint32_t flags;
    __asm__ volatile(
        "pushf\n"
        "pop %0\n"
        "cli\n"
        : "=r"(flags)
        :
        : "memory"
    );
    return flags;
}

static void irq_restore(uint32_t state) {
    __asm__ volatile("push %0\npopf\n" : : "r"(state) : "memory");
}

// Forward declaration
extern void idt_register_handler(uint8_t num, void (*handler)(void));
extern void idt_unregister_handler(uint8_t num);

static int irq_register(uint8_t vector, void (*handler)(void)) {
    idt_register_handler(vector, handler);
    return 0;
}

static void irq_unregister(uint8_t vector) {
    idt_unregister_handler(vector);
}

// ========== Memory Management ==========

static void mmu_init(void) {
    // TODO: Initialize paging
    // For now, we're using identity mapping from boot.s
}

static void* mmu_map(phys_addr_t phys, virt_addr_t virt, uint32_t flags) {
    (void)phys;
    (void)flags;
    // TODO: Implement page table management
    // For now, assume identity mapping
    return (void*)virt;
}

static void mmu_unmap(virt_addr_t virt) {
    (void)virt;
    // TODO: Implement page unmapping
}

static void mmu_flush_tlb(virt_addr_t virt) {
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

static void mmu_flush_tlb_all(void) {
    uint32_t cr3;
    __asm__ volatile(
        "mov %%cr3, %0\n"
        "mov %0, %%cr3\n"
        : "=r"(cr3)
        :
        : "memory"
    );
}

// ========== I/O Operations ==========

static uint8_t io_inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static uint16_t io_inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static uint32_t io_inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void io_outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void io_outw(uint16_t port, uint16_t value) {
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static void io_outl(uint16_t port, uint32_t value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static void* mmio_map(phys_addr_t phys, size_t size) {
    (void)size;
    // TODO: Map physical address to virtual address with caching disabled
    // For now, return identity mapping
    return (void*)phys;
}

static void mmio_unmap(void* virt, size_t size) {
    (void)virt;
    (void)size;
    // TODO: Unmap MMIO region
}

// ========== SMP Operations ==========

static uint32_t smp_num_cpus(void) {
    // TODO: Detect number of CPUs via ACPI/MP tables
    return 1;  // Single CPU for now
}

static void smp_send_ipi(uint32_t cpu_id, uint8_t vector) {
    (void)cpu_id;
    (void)vector;
    // TODO: Implement via APIC
}

static void smp_broadcast_ipi(uint8_t vector) {
    (void)vector;
    // TODO: Implement via APIC
}

static int smp_boot_cpu(uint32_t cpu_id, void (*entry_point)(void)) {
    (void)cpu_id;
    (void)entry_point;
    // TODO: Implement AP (Application Processor) boot
    return -ENODEV;
}

// ========== Timer Operations ==========

// Forward declarations from timer.c
extern void timer_init(uint32_t frequency_hz);
extern uint64_t timer_read_tsc(void);
extern uint64_t timer_read_us(void);

// HAL wrappers for timer operations
static uint64_t hal_timer_read_tsc(void) {
    return timer_read_tsc();
}

static uint64_t hal_timer_read_us(void) {
    return timer_read_us();
}

static void hal_timer_init(uint32_t frequency_hz) {
    timer_init(frequency_hz);
}

// ========== Special Operations ==========

static void system_reboot(void) {
    // Use keyboard controller to reboot
    io_outb(0x64, 0xFE);

    // If that didn't work, triple fault
    __asm__ volatile("int $0xFF");
}

static void system_shutdown(void) {
    // ACPI shutdown would go here
    // For now, just halt
    while (1) {
        cpu_halt();
    }
}

static void panic_handler(const char* message) {
    // Architecture-specific panic
    // TODO: Print stack trace
    irq_disable();

    // Call kernel panic
    extern void kernel_panic(const char*);
    kernel_panic(message);
}

// ========== x86 HAL Operations Table ==========

static struct hal_ops x86_hal = {
    // CPU
    .cpu_init = cpu_init,
    .cpu_id = cpu_id,
    .cpu_halt = cpu_halt,
    .cpu_features = cpu_features,

    // Interrupts
    .irq_enable = irq_enable,
    .irq_disable = irq_disable,
    .irq_restore = irq_restore,
    .irq_register = irq_register,
    .irq_unregister = irq_unregister,

    // Memory
    .mmu_init = mmu_init,
    .mmu_map = mmu_map,
    .mmu_unmap = mmu_unmap,
    .mmu_flush_tlb = mmu_flush_tlb,
    .mmu_flush_tlb_all = mmu_flush_tlb_all,

    // I/O
    .io_inb = io_inb,
    .io_inw = io_inw,
    .io_inl = io_inl,
    .io_outb = io_outb,
    .io_outw = io_outw,
    .io_outl = io_outl,
    .mmio_map = mmio_map,
    .mmio_unmap = mmio_unmap,

    // SMP
    .smp_num_cpus = smp_num_cpus,
    .smp_send_ipi = smp_send_ipi,
    .smp_broadcast_ipi = smp_broadcast_ipi,
    .smp_boot_cpu = smp_boot_cpu,

    // Timer
    .timer_read_tsc = hal_timer_read_tsc,
    .timer_read_us = hal_timer_read_us,
    .timer_init = hal_timer_init,

    // System
    .system_reboot = system_reboot,
    .system_shutdown = system_shutdown,
    .panic = panic_handler,
};

// Global HAL pointer
struct hal_ops* hal = NULL;

// Initialize x86 HAL
void hal_x86_init(void) {
    hal = &x86_hal;
    hal->cpu_init();
}
