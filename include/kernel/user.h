/**
 * Userspace Task Definitions
 *
 * Memory layout and constants for ring 3 tasks.
 */

#ifndef KERNEL_USER_H
#define KERNEL_USER_H

#include <kernel/types.h>

/**
 * Userspace Virtual Memory Layout
 *
 * 0x00000000 - 0x00400000: Reserved (NULL pointer guard)
 * 0x00400000 - 0x00800000: User code & data (4MB)
 * 0xBFFFF000 - 0xC0000000: User stack (4KB, grows down)
 * 0xC0000000 - 0xFFFFFFFF: Kernel space (not accessible from ring 3)
 */

#define USER_CODE_BASE    0x00400000  // 4MB - standard ELF load address
#define USER_CODE_SIZE    0x00400000  // 4MB max
#define USER_STACK_TOP    0xC0000000  // Just below kernel (3GB)
#define USER_STACK_SIZE   0x00001000  // 4KB

// GDT selectors for userspace (with RPL=3)
#define USER_CS_SELECTOR  0x1B  // GDT entry 3 with RPL=3
#define USER_DS_SELECTOR  0x23  // GDT entry 4 with RPL=3

// EFLAGS for userspace tasks
#define USER_EFLAGS       0x202 // IF=1 (interrupts enabled), reserved bit 1 = 1

/**
 * Create a userspace task
 *
 * @param name        Task name
 * @param entry_point Physical address of user code
 * @param code_size   Size of user code in bytes
 * @return            Task pointer, or NULL on failure
 */
struct task;
struct task* task_create_user(const char* name, void* entry_point, size_t code_size);

#endif // KERNEL_USER_H
