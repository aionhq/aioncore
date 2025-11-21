# Kernel Testing Guide

This document explains how to write and run unit tests for the AionCore kernel.

## Philosophy

Following `KERNEL_C_STYLE.md` section 7.1, each critical subsystem should provide self-tests:

- Physical memory manager (PMM)
- Virtual memory / VM mapping logic
- Scheduler and context switching
- IPC channels and capability passing
- Unit lifecycle

## Test Framework (ktest)

The kernel includes a simple unit testing framework (`include/kernel/ktest.h`) that allows:

- Self-contained test functions
- Pass/fail reporting via return codes
- Test output to VGA/serial via kprintf
- No dynamic allocation in tests
- Tests respect RT constraints

## Writing Tests

### Test Structure

```c
#include <kernel/ktest.h>

// Test function - returns KTEST_PASS (0) or KTEST_FAIL (-1)
static int test_my_feature(void) {
    // Test code here
    KTEST_ASSERT(condition, "error message");
    KTEST_ASSERT_EQ(actual, expected, "values should match");

    return KTEST_PASS;
}

// Register test
KTEST_DEFINE("subsystem_name", test_name, test_my_feature);
```

### Assertion Macros

- `KTEST_ASSERT(condition, message)` - Assert condition is true
- `KTEST_ASSERT_EQ(actual, expected, message)` - Assert equality
- `KTEST_ASSERT_NEQ(actual, unexpected, message)` - Assert inequality
- `KTEST_ASSERT_NULL(ptr, message)` - Assert pointer is NULL
- `KTEST_ASSERT_NOT_NULL(ptr, message)` - Assert pointer is not NULL

### Example Tests

See existing test files:
- `lib/string_test.c` - String library tests
- `arch/x86/timer_test.c` - Timer subsystem tests

## Building with Tests

### Enable tests during build:

```bash
# Build with tests enabled
KERNEL_TESTS=1 make

# Or use the test target (clean + build + run)
make test
```

### Run tests in QEMU:

```bash
# Build and run with GUI to see test output
KERNEL_TESTS=1 make && make run

# Tests run automatically after initialization
```

## Test Output

When tests run, you'll see output like:

```
========================================
  KERNEL TEST SUITE
========================================

[TEST] string::strlen_basic ... PASS
[TEST] string::strlcpy_basic ... PASS
[TEST] string::strlcpy_truncate ... PASS
[TEST] string::strlcat_basic ... PASS
[TEST] timer::tsc_monotonic ... PASS
[TEST] timer::timer_calibrated ... PASS

========================================
Tests run: 10
Passed:    10
Failed:    0
========================================

[SUCCESS] All tests passed!
```

## Test Registration

Tests are registered using the `KTEST_DEFINE` macro which places them in the `.ktests` section of the kernel binary. The linker script (`arch/x86/linker.ld`) defines `__start_ktests` and `__stop_ktests` symbols that bracket this section, allowing the test runner to iterate over all registered tests.

## Guidelines

1. **Keep tests simple** - One assertion per test is ideal
2. **Test one thing** - Each test should verify a single behavior
3. **No side effects** - Tests should not modify global state
4. **Fast execution** - Tests run at boot, keep them quick
5. **Document failures** - Use descriptive error messages
6. **Follow C style** - Tests must follow `KERNEL_C_STYLE.md`

## Testing Critical Paths

For RT-critical code (interrupt handlers, scheduler, IPC):

- **Measure timing** - Use TSC to verify bounded execution
- **No dynamic allocation** - Tests must respect no-alloc constraints
- **Determinism** - Same inputs should produce same results
- **Stress test** - Test edge cases and boundary conditions

Example timing test:

```c
static int test_operation_bounded(void) {
    uint64_t start = timer_read_tsc();

    // Perform operation
    my_critical_operation();

    uint64_t end = timer_read_tsc();
    uint64_t cycles = end - start;

    // Verify < 100 cycle bound
    KTEST_ASSERT(cycles < 100, "operation exceeds cycle budget");

    return KTEST_PASS;
}
```

## Future Enhancements

- Unit tests in userspace (Phase 5+)
- Integration tests via test units
- Fuzzing for capability table
- Property-based testing for scheduler
- Code coverage measurement
- CI/CD integration

## See Also

- `KERNEL_C_STYLE.md` - Section 7: Testing, Debug, and Verification
- `include/kernel/ktest.h` - Test framework API
- `lib/string_test.c` - Example test implementation
