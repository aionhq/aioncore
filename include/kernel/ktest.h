#ifndef KERNEL_KTEST_H
#define KERNEL_KTEST_H

/**
 * Kernel Testing Framework (ktest)
 *
 * Simple unit test framework for kernel subsystems.
 * Tests can run at boot time with -DKERNEL_TESTS build flag.
 *
 * Philosophy:
 * - Tests are self-contained functions
 * - Tests report pass/fail via return value
 * - Test output goes to kprintf (visible on VGA/serial)
 * - No dynamic allocation in tests
 * - Tests respect RT constraints
 */

#include <stdbool.h>
#include <stdint.h>
#include <drivers/vga.h>

// Test result codes
#define KTEST_PASS  0
#define KTEST_FAIL  -1

// Test function signature
typedef int (*ktest_fn)(void);

// Test descriptor
struct ktest {
    const char *name;        // Test name (e.g., "pmm_alloc_free")
    const char *subsystem;   // Subsystem (e.g., "PMM", "Timer", "Scheduler")
    ktest_fn    test_fn;     // Test function
};

// Test registration macros
#define KTEST_DEFINE(subsys, testname, fn) \
    static struct ktest __ktest_##testname \
    __attribute__((section(".ktests"), used)) = { \
        .name = #testname, \
        .subsystem = subsys, \
        .test_fn = fn \
    }

// Assertion macros for tests
#define KTEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            kprintf("  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, message); \
            return KTEST_FAIL; \
        } \
    } while(0)

#define KTEST_ASSERT_EQ(actual, expected, message) \
    do { \
        if ((actual) != (expected)) { \
            kprintf("  [FAIL] %s:%d: %s (expected %lld, got %lld)\n", \
                    __FILE__, __LINE__, message, \
                    (long long)(expected), (long long)(actual)); \
            return KTEST_FAIL; \
        } \
    } while(0)

#define KTEST_ASSERT_NEQ(actual, unexpected, message) \
    do { \
        if ((actual) == (unexpected)) { \
            kprintf("  [FAIL] %s:%d: %s (got unexpected value %lld)\n", \
                    __FILE__, __LINE__, message, (long long)(unexpected)); \
            return KTEST_FAIL; \
        } \
    } while(0)

#define KTEST_ASSERT_NULL(ptr, message) \
    KTEST_ASSERT((ptr) == NULL, message)

#define KTEST_ASSERT_NOT_NULL(ptr, message) \
    KTEST_ASSERT((ptr) != NULL, message)

// Test runner
// Returns number of failed tests (0 = all pass)
int ktest_run_all(void);
int ktest_run_subsystem(const char *subsystem);

#endif // KERNEL_KTEST_H
