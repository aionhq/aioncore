#ifndef KERNEL_IDT_H
#define KERNEL_IDT_H

#include <stdint.h>

// Interrupt frame structure (matches assembly layout)
struct interrupt_frame {
    // Pushed by our handler
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;

    // Pushed by CPU (or our wrapper)
    uint32_t int_no, err_code;

    // Pushed by CPU automatically
    uint32_t eip, cs, eflags, useresp, ss;
};

// Interrupt handler function type
typedef void (*irq_handler_fn)(struct interrupt_frame* frame);

// Initialize IDT
void idt_init(void);

// Register/unregister interrupt handlers
void idt_register_handler(uint8_t num, irq_handler_fn handler);
void idt_unregister_handler(uint8_t num);

// Enable/disable specific IRQ lines
static inline void irq_clear_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = 0x21;
    } else {
        port = 0xA1;
        irq -= 8;
    }

    extern struct hal_ops* hal;
    value = hal->io_inb(port) & ~(1 << irq);
    hal->io_outb(port, value);
}

static inline void irq_set_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = 0x21;
    } else {
        port = 0xA1;
        irq -= 8;
    }

    extern struct hal_ops* hal;
    value = hal->io_inb(port) | (1 << irq);
    hal->io_outb(port, value);
}

#endif // KERNEL_IDT_H
