#ifndef KERNEL_ASSERT_H
#define KERNEL_ASSERT_H

#include <kernel/types.h>
#include <stdbool.h>

// Kernel assertion system for runtime verification
// Follows FORMAL_VERIFICATION.md Level 2 requirements

// Forward declare kprintf
int kprintf(const char* format, ...) __attribute__((format(printf, 1, 2)));

// Panic function for failed assertions
__attribute__((noreturn))
static inline void kernel_panic(const char* file, int line, const char* func,
                                const char* expr) {
    // Disable interrupts
    __asm__ volatile("cli");

    kprintf("\n\n");
    kprintf("========================================\n");
    kprintf("  KERNEL PANIC - ASSERTION FAILED\n");
    kprintf("========================================\n");
    kprintf("File: %s:%d\n", file, line);
    kprintf("Function: %s\n", func);
    kprintf("Expression: %s\n", expr);
    kprintf("========================================\n");

    // Halt forever
    while (1) {
        __asm__ volatile("hlt");
    }
}

// Assertion macros (only in DEBUG builds or when KERNEL_ASSERT enabled)
#if defined(DEBUG) || defined(KERNEL_ASSERT)

// Basic assertion: panic if condition is false
#define kassert(expr) \
    do { \
        if (!(expr)) { \
            kernel_panic(__FILE__, __LINE__, __func__, #expr); \
        } \
    } while (0)

// Assert with custom message
#define kassert_msg(expr, msg) \
    do { \
        if (!(expr)) { \
            kprintf("Assertion failed: %s\n", msg); \
            kernel_panic(__FILE__, __LINE__, __func__, #expr); \
        } \
    } while (0)

// Equality assertions
#define kassert_eq(a, b) \
    do { \
        if ((a) != (b)) { \
            kprintf("Assertion failed: %s == %s (%d != %d)\n", \
                    #a, #b, (int)(a), (int)(b)); \
            kernel_panic(__FILE__, __LINE__, __func__, #a " == " #b); \
        } \
    } while (0)

#define kassert_neq(a, b) \
    do { \
        if ((a) == (b)) { \
            kprintf("Assertion failed: %s != %s\n", #a, #b); \
            kernel_panic(__FILE__, __LINE__, __func__, #a " != " #b); \
        } \
    } while (0)

// Pointer assertions
#define kassert_not_null(ptr) \
    do { \
        if ((ptr) == NULL) { \
            kernel_panic(__FILE__, __LINE__, __func__, #ptr " != NULL"); \
        } \
    } while (0)

#define kassert_null(ptr) \
    do { \
        if ((ptr) != NULL) { \
            kernel_panic(__FILE__, __LINE__, __func__, #ptr " == NULL"); \
        } \
    } while (0)

// Interrupt state assertions
#define kassert_irqs_disabled() \
    do { \
        uint32_t flags; \
        __asm__ volatile("pushf; pop %0" : "=r"(flags)); \
        if (flags & 0x200) { \
            kernel_panic(__FILE__, __LINE__, __func__, "IRQs must be disabled"); \
        } \
    } while (0)

#define kassert_irqs_enabled() \
    do { \
        uint32_t flags; \
        __asm__ volatile("pushf; pop %0" : "=r"(flags)); \
        if (!(flags & 0x200)) { \
            kernel_panic(__FILE__, __LINE__, __func__, "IRQs must be enabled"); \
        } \
    } while (0)

// Range assertion
#define kassert_range(val, min, max) \
    do { \
        if ((val) < (min) || (val) > (max)) { \
            kprintf("Assertion failed: %s in range [%d, %d] (got %d)\n", \
                    #val, (int)(min), (int)(max), (int)(val)); \
            kernel_panic(__FILE__, __LINE__, __func__, #val " in range"); \
        } \
    } while (0)

// Alignment assertion (for RT requirements)
#define kassert_aligned(ptr, alignment) \
    do { \
        if (((uintptr_t)(ptr) % (alignment)) != 0) { \
            kprintf("Assertion failed: %s aligned to %d (ptr=%p)\n", \
                    #ptr, (int)(alignment), (void*)(ptr)); \
            kernel_panic(__FILE__, __LINE__, __func__, #ptr " aligned"); \
        } \
    } while (0)

#else
// Release build: assertions compile to nothing (zero overhead)
#define kassert(expr) ((void)0)
#define kassert_msg(expr, msg) ((void)0)
#define kassert_eq(a, b) ((void)0)
#define kassert_neq(a, b) ((void)0)
#define kassert_not_null(ptr) ((void)0)
#define kassert_null(ptr) ((void)0)
#define kassert_irqs_disabled() ((void)0)
#define kassert_irqs_enabled() ((void)0)
#define kassert_range(val, min, max) ((void)0)
#define kassert_aligned(ptr, alignment) ((void)0)
#endif

// Invariant documentation macros (for formal verification)
// These are always compiled out but serve as documentation
#define INVARIANT(desc) /* Invariant: desc */
#define PRECONDITION(desc) /* Precondition: desc */
#define POSTCONDITION(desc) /* Postcondition: desc */

#endif // KERNEL_ASSERT_H
