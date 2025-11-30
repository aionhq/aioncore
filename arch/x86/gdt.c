/**
 * x86 Global Descriptor Table (GDT) Implementation
 *
 * Sets up flat memory model with ring 0 (kernel) and ring 3 (user) segments.
 * Includes TSS for syscall stack switching.
 */

#include <kernel/gdt.h>
#include <kernel/types.h>
#include <drivers/vga.h>

// ========== GDT Structures ==========

/**
 * GDT descriptor (8 bytes)
 */
typedef struct {
    uint16_t limit_low;      // Limit bits 0-15
    uint16_t base_low;       // Base bits 0-15
    uint8_t  base_mid;       // Base bits 16-23
    uint8_t  access;         // Access byte (P/DPL/S/Type)
    uint8_t  granularity;    // Flags and limit bits 16-19
    uint8_t  base_high;      // Base bits 24-31
} __attribute__((packed)) gdt_descriptor_t;

/**
 * GDT pointer structure (loaded with LGDT)
 */
typedef struct {
    uint16_t limit;          // Size of GDT - 1
    uint32_t base;           // Address of GDT
} __attribute__((packed)) gdt_ptr_t;

/**
 * Task State Segment (TSS)
 *
 * Used for hardware task switching (we don't use this feature) and
 * for providing kernel stack pointer (ESP0) during ring 3 → ring 0 transitions.
 */
typedef struct {
    uint32_t prev_tss;   // Previous TSS (not used)
    uint32_t esp0;       // Stack pointer for ring 0 (CRITICAL for syscalls!)
    uint32_t ss0;        // Stack segment for ring 0
    uint32_t esp1;       // Stack pointer for ring 1 (not used)
    uint32_t ss1;        // Stack segment for ring 1 (not used)
    uint32_t esp2;       // Stack pointer for ring 2 (not used)
    uint32_t ss2;        // Stack segment for ring 2 (not used)
    uint32_t cr3;        // Page directory base (not used)
    uint32_t eip;        // Instruction pointer (not used)
    uint32_t eflags;     // Flags register (not used)
    uint32_t eax;        // General purpose registers (not used)
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;         // Segment registers (not used)
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;        // LDT selector (not used)
    uint16_t trap;       // Trap on task switch (not used)
    uint16_t iomap_base; // I/O map base address (set to sizeof(tss) = no I/O map)
} __attribute__((packed)) tss_t;

// ========== GDT Data ==========

#define GDT_ENTRIES 6

static gdt_descriptor_t gdt[GDT_ENTRIES];
static gdt_ptr_t gdt_ptr;
static tss_t tss;

// ========== Access Byte Flags ==========

#define GDT_ACCESS_PRESENT      (1 << 7)  // Segment is present
#define GDT_ACCESS_DPL_0        (0 << 5)  // Privilege level 0 (kernel)
#define GDT_ACCESS_DPL_3        (3 << 5)  // Privilege level 3 (user)
#define GDT_ACCESS_DESCRIPTOR   (1 << 4)  // Descriptor type (1=code/data, 0=system)
#define GDT_ACCESS_EXECUTABLE   (1 << 3)  // Executable (code segment)
#define GDT_ACCESS_DC           (1 << 2)  // Direction/Conforming bit
#define GDT_ACCESS_RW           (1 << 1)  // Readable (code) or Writable (data)
#define GDT_ACCESS_ACCESSED     (1 << 0)  // Accessed bit (set by CPU)

// ========== Granularity Flags ==========

#define GDT_GRAN_4K             (1 << 7)  // 4KB granularity (multiply limit by 4096)
#define GDT_GRAN_32BIT          (1 << 6)  // 32-bit protected mode
#define GDT_GRAN_64BIT          (0 << 6)  // 16-bit mode (not used)
#define GDT_GRAN_AVL            (1 << 4)  // Available for system use

// ========== Helper Functions ==========

/**
 * Encode a GDT descriptor
 *
 * @param desc Descriptor to encode
 * @param base Physical base address
 * @param limit Segment limit (in pages if 4KB granularity set)
 * @param access Access byte
 * @param gran Granularity and flags
 */
static void encode_gdt_descriptor(gdt_descriptor_t* desc, uint32_t base,
                                   uint32_t limit, uint8_t access, uint8_t gran) {
    // Encode limit (20 bits)
    desc->limit_low = limit & 0xFFFF;
    desc->granularity = (gran & 0xF0) | ((limit >> 16) & 0x0F);

    // Encode base (32 bits)
    desc->base_low = base & 0xFFFF;
    desc->base_mid = (base >> 16) & 0xFF;
    desc->base_high = (base >> 24) & 0xFF;

    // Access byte
    desc->access = access;
}

/**
 * Load GDT (assembly helper)
 *
 * Loads GDT pointer and reloads segment registers.
 */
extern void gdt_flush(uint32_t gdt_ptr_addr);

__asm__(
    ".global gdt_flush\n"
    "gdt_flush:\n"
    "    movl 4(%esp), %eax\n"        // Get GDT pointer address from stack
    "    lgdt (%eax)\n"                // Load GDT
    "    \n"
    "    # Reload segment registers\n"
    "    movw $0x10, %ax\n"            // Kernel data segment (entry 2)
    "    movw %ax, %ds\n"
    "    movw %ax, %es\n"
    "    movw %ax, %fs\n"
    "    movw %ax, %gs\n"
    "    movw %ax, %ss\n"
    "    \n"
    "    # Reload CS with far jump\n"
    "    ljmp $0x08, $gdt_flush_complete\n"  // Jump to kernel code segment (entry 1)
    "gdt_flush_complete:\n"
    "    ret\n"
);

/**
 * Load TSS (assembly helper)
 */
extern void tss_flush(void);

__asm__(
    ".global tss_flush\n"
    "tss_flush:\n"
    "    movw $0x28, %ax\n"            // TSS segment (entry 5)
    "    ltr %ax\n"                     // Load Task Register
    "    ret\n"
);

// ========== Public API ==========

/**
 * Initialize GDT
 *
 * NOTE: This is called very early (in HAL init), before VGA/serial console
 * are initialized, so kprintf output won't be visible. Use gdt_verify() later.
 */
void gdt_init(void) {
    // Entry 0: Null descriptor (required by x86)
    encode_gdt_descriptor(&gdt[0], 0, 0, 0, 0);

    // Entry 1: Kernel code segment (ring 0, executable, readable)
    uint8_t kernel_code_access = GDT_ACCESS_PRESENT | GDT_ACCESS_DPL_0 |
                                   GDT_ACCESS_DESCRIPTOR | GDT_ACCESS_EXECUTABLE |
                                   GDT_ACCESS_RW;
    uint8_t kernel_code_gran = GDT_GRAN_4K | GDT_GRAN_32BIT;
    encode_gdt_descriptor(&gdt[1], 0, 0xFFFFF, kernel_code_access, kernel_code_gran);

    // Entry 2: Kernel data segment (ring 0, writable)
    uint8_t kernel_data_access = GDT_ACCESS_PRESENT | GDT_ACCESS_DPL_0 |
                                   GDT_ACCESS_DESCRIPTOR | GDT_ACCESS_RW;
    uint8_t kernel_data_gran = GDT_GRAN_4K | GDT_GRAN_32BIT;
    encode_gdt_descriptor(&gdt[2], 0, 0xFFFFF, kernel_data_access, kernel_data_gran);

    // Entry 3: User code segment (ring 3, executable, readable)
    uint8_t user_code_access = GDT_ACCESS_PRESENT | GDT_ACCESS_DPL_3 |
                                 GDT_ACCESS_DESCRIPTOR | GDT_ACCESS_EXECUTABLE |
                                 GDT_ACCESS_RW;
    uint8_t user_code_gran = GDT_GRAN_4K | GDT_GRAN_32BIT;
    encode_gdt_descriptor(&gdt[3], 0, 0xFFFFF, user_code_access, user_code_gran);

    // Entry 4: User data segment (ring 3, writable)
    uint8_t user_data_access = GDT_ACCESS_PRESENT | GDT_ACCESS_DPL_3 |
                                 GDT_ACCESS_DESCRIPTOR | GDT_ACCESS_RW;
    uint8_t user_data_gran = GDT_GRAN_4K | GDT_GRAN_32BIT;
    encode_gdt_descriptor(&gdt[4], 0, 0xFFFFF, user_data_access, user_data_gran);

    // Entry 5: TSS (system descriptor, ring 0, available TSS type=0x09)
    // Clear TSS first
    for (size_t i = 0; i < sizeof(tss); i++) {
        ((uint8_t*)&tss)[i] = 0;
    }

    // Set TSS fields
    tss.ss0 = GDT_KERNEL_DATA_SEL;  // Kernel data segment for stack
    tss.esp0 = 0;  // Will be updated by gdt_set_kernel_stack()
    tss.iomap_base = sizeof(tss);   // No I/O permission bitmap

    // TSS descriptor: base = address of TSS, limit = size of TSS - 1
    uint32_t tss_base = (uint32_t)&tss;
    uint32_t tss_limit = sizeof(tss) - 1;
    uint8_t tss_access = GDT_ACCESS_PRESENT | GDT_ACCESS_DPL_0 | 0x09;  // Type 9 = Available TSS
    uint8_t tss_gran = 0;  // Byte granularity (not 4KB)
    encode_gdt_descriptor(&gdt[5], tss_base, tss_limit, tss_access, tss_gran);

    // Set up GDT pointer
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base = (uint32_t)&gdt;

    // Load GDT
    gdt_flush((uint32_t)&gdt_ptr);

    // Load TSS
    tss_flush();
}

/**
 * Verify GDT is loaded correctly
 *
 * Call this AFTER console is initialized to print verification.
 * Reads back segment registers and verifies they match expected values.
 */
void gdt_verify(void) {
    uint16_t cs, ds, ss, tr;

    // Read current segment registers
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    __asm__ volatile("mov %%ds, %0" : "=r"(ds));
    __asm__ volatile("mov %%ss, %0" : "=r"(ss));
    __asm__ volatile("str %0" : "=r"(tr));  // Store Task Register

    kprintf("[GDT] GDT verification:\n");
    kprintf("[GDT]   CS = 0x%04x (expected 0x%04x) %s\n",
            (unsigned int)cs, (unsigned int)GDT_KERNEL_CODE_SEL,
            cs == GDT_KERNEL_CODE_SEL ? "OK" : "FAIL");
    kprintf("[GDT]   DS = 0x%04x (expected 0x%04x) %s\n",
            (unsigned int)ds, (unsigned int)GDT_KERNEL_DATA_SEL,
            ds == GDT_KERNEL_DATA_SEL ? "OK" : "FAIL");
    kprintf("[GDT]   SS = 0x%04x (expected 0x%04x) %s\n",
            (unsigned int)ss, (unsigned int)GDT_KERNEL_DATA_SEL,
            ss == GDT_KERNEL_DATA_SEL ? "OK" : "FAIL");
    kprintf("[GDT]   TR = 0x%04x (expected 0x%04x) %s\n",
            (unsigned int)tr, (unsigned int)GDT_TSS_SEL,
            tr == GDT_TSS_SEL ? "OK" : "FAIL");

    kprintf("[GDT] TSS base: 0x%08lx, limit: %u bytes, ESP0: 0x%08lx\n",
            (unsigned long)&tss, (unsigned int)sizeof(tss),
            (unsigned long)tss.esp0);

    // Verify all passed
    if (cs == GDT_KERNEL_CODE_SEL && ds == GDT_KERNEL_DATA_SEL &&
        ss == GDT_KERNEL_DATA_SEL && tr == GDT_TSS_SEL) {
        kprintf("[GDT] All segment registers correct!\n");
    } else {
        kprintf("[GDT] ERROR: Segment register mismatch!\n");
    }
}

/**
 * Set kernel stack pointer for syscalls
 *
 * MUST be called in context_switch() before switching to a new task.
 */
void gdt_set_kernel_stack(uintptr_t esp0) {
    tss.esp0 = esp0;
}
