/**
 * Global Descriptor Table (GDT) Management
 *
 * Provides x86 segmentation setup for kernel and userspace.
 * Required for ring 3 (userspace) tasks and syscall stack switching.
 */

#ifndef KERNEL_GDT_H
#define KERNEL_GDT_H

#include <kernel/types.h>

/**
 * Initialize GDT
 *
 * Sets up:
 * - Null descriptor (required)
 * - Kernel code segment (ring 0)
 * - Kernel data segment (ring 0)
 * - User code segment (ring 3)
 * - User data segment (ring 3)
 * - TSS (Task State Segment) for stack switching
 *
 * Must be called early in HAL initialization.
 */
void gdt_init(void);

/**
 * Verify GDT is loaded correctly
 *
 * Reads back segment registers and prints verification.
 * Call AFTER console is initialized.
 */
void gdt_verify(void);

/**
 * Set kernel stack pointer for syscalls
 *
 * Updates TSS.esp0 to point to the kernel stack for the current task.
 * MUST be called in context_switch() before switching to a userspace task.
 *
 * @param esp0 Kernel stack pointer (top of kernel stack)
 *
 * RT Constraint: O(1), < 20 cycles (single memory write)
 */
void gdt_set_kernel_stack(uintptr_t esp0);

/**
 * GDT segment selectors
 *
 * Selector format: (index << 3) | TI | RPL
 * - index: GDT entry number
 * - TI: Table Indicator (0=GDT, 1=LDT)
 * - RPL: Requested Privilege Level (0=ring 0, 3=ring 3)
 */
#define GDT_KERNEL_CODE_SEL  0x08  // Entry 1, ring 0
#define GDT_KERNEL_DATA_SEL  0x10  // Entry 2, ring 0
#define GDT_USER_CODE_SEL    0x1B  // Entry 3, ring 3 (0x18 | 3)
#define GDT_USER_DATA_SEL    0x23  // Entry 4, ring 3 (0x20 | 3)
#define GDT_TSS_SEL          0x28  // Entry 5, ring 0

#endif // KERNEL_GDT_H
