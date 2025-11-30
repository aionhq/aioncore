// x86 Interrupt Descriptor Table (IDT) Implementation

#include <kernel/hal.h>
#include <kernel/types.h>
#include <kernel/idt.h>
#include <stdint.h>

// IDT entry structure
struct idt_entry {
    uint16_t offset_low;    // Lower 16 bits of handler address
    uint16_t selector;      // Code segment selector
    uint8_t zero;           // Always zero
    uint8_t type_attr;      // Type and attributes
    uint16_t offset_high;   // Upper 16 bits of handler address
} __attribute__((packed));

// IDT pointer structure (loaded with LIDT instruction)
struct idt_ptr {
    uint16_t limit;         // Size of IDT - 1
    uint32_t base;          // Address of IDT
} __attribute__((packed));

// IDT with 256 entries (0-255)
#define IDT_ENTRIES 256
static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtr;

// Interrupt handler function pointers
static irq_handler_fn interrupt_handlers[IDT_ENTRIES];

// Forward declarations of assembly interrupt stubs
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

// IRQ handlers (32-47)
extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

// Set an IDT entry
static void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags) {
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].offset_high = (handler >> 16) & 0xFFFF;
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
}

// Remap PIC (Programmable Interrupt Controller)
// By default, IRQs 0-7 are mapped to IDT 8-15 (conflicts with CPU exceptions!)
// We remap them to 32-47
static void pic_remap(void) {
    // ICW1: Start initialization
    hal->io_outb(0x20, 0x11);
    hal->io_outb(0xA0, 0x11);

    // ICW2: Set vector offsets
    hal->io_outb(0x21, 0x20);  // Master PIC: IRQs 0-7 → INT 32-39
    hal->io_outb(0xA1, 0x28);  // Slave PIC: IRQs 8-15 → INT 40-47

    // ICW3: Setup cascade
    hal->io_outb(0x21, 0x04);  // Master: IRQ2 has slave
    hal->io_outb(0xA1, 0x02);  // Slave: cascade identity

    // ICW4: Set mode
    hal->io_outb(0x21, 0x01);  // 8086 mode
    hal->io_outb(0xA1, 0x01);

    // Mask all interrupts initially
    hal->io_outb(0x21, 0xFF);
    hal->io_outb(0xA1, 0xFF);
}

// Initialize IDT
void idt_init(void) {
    // Set up IDT pointer
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint32_t)&idt;

    // Clear IDT
    for (int i = 0; i < IDT_ENTRIES; i++) {
        interrupt_handlers[i] = NULL;
        idt_set_gate(i, 0, 0, 0);
    }

    // Remap PIC before setting up ISRs
    pic_remap();

    // Install ISRs (CPU exceptions 0-31)
    // Flags: 0x8E = present, ring 0, 32-bit interrupt gate
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uint32_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uint32_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (uint32_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (uint32_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (uint32_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uint32_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (uint32_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uint32_t)isr8, 0x08, 0x8E);
    idt_set_gate(9, (uint32_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);

    // Install IRQ handlers (32-47)
    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2, 0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3, 0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4, 0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5, 0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6, 0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7, 0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8, 0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9, 0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);

    // Install syscall handler (INT 0x80)
    // CRITICAL: Use 0xEE (DPL=3, interrupt gate) to allow ring 3 calls
    // 0x8E would be DPL=0 and block userspace with #GP
    extern void syscall_entry_int80(void);
    idt_set_gate(0x80, (uint32_t)syscall_entry_int80, 0x08, 0xEE);

    // Load IDT
    __asm__ volatile("lidt %0" : : "m"(idtr));
}

// Register interrupt handler
void idt_register_handler(uint8_t num, irq_handler_fn handler) {
    interrupt_handlers[num] = handler;
}

// Unregister interrupt handler
void idt_unregister_handler(uint8_t num) {
    interrupt_handlers[num] = NULL;
}

// Exception names
static const char* exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Security Exception",
    "Reserved"
};

// Common ISR handler (called from assembly stubs)
void isr_handler(struct interrupt_frame* frame) {
    // Call registered handler if exists
    if (interrupt_handlers[frame->int_no]) {
        interrupt_handlers[frame->int_no](frame);
        return;
    }

    // Unhandled exception - panic
    extern int kprintf(const char* fmt, ...);
    extern void kernel_panic(const char* msg);

    kprintf("\n*** EXCEPTION: %s ***\n", exception_messages[frame->int_no]);
    kprintf("INT=%u ERR=%u\n", frame->int_no, frame->err_code);
    kprintf("EIP=%08x CS=%04x EFLAGS=%08x\n", frame->eip, frame->cs, frame->eflags);
    kprintf("EAX=%08x EBX=%08x ECX=%08x EDX=%08x\n",
            frame->eax, frame->ebx, frame->ecx, frame->edx);
    kprintf("ESP=%08x EBP=%08x ESI=%08x EDI=%08x\n",
            frame->esp, frame->ebp, frame->esi, frame->edi);

    kernel_panic("Unhandled exception");
}

// Common IRQ handler (called from assembly stubs)
void irq_handler(struct interrupt_frame* frame) {
    // Call registered handler if exists
    if (interrupt_handlers[frame->int_no]) {
        interrupt_handlers[frame->int_no](frame);
    }

    // Send EOI (End Of Interrupt) to PIC
    if (frame->int_no >= 40) {
        // Slave PIC
        hal->io_outb(0xA0, 0x20);
    }
    // Master PIC
    hal->io_outb(0x20, 0x20);

    // Check if we need to reschedule (for preemptive scheduling)
    extern bool scheduler_need_resched(void);
    extern void schedule(void);
    if (scheduler_need_resched()) {
        schedule();
    }
}
