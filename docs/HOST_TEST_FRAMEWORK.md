# Host-Side Unit Test Framework Design

## Problem Statement

**Current State:** Tests are baked into kernel binary and only run during boot
- ❌ Kernel must boot for tests to run
- ❌ If kernel crashes during init, tests never execute
- ❌ Slow feedback loop (10+ seconds per iteration)
- ❌ Can't catch bugs BEFORE building ISO

**Required:** True unit tests that run on host machine (macOS/Linux)
- ✅ Instant feedback (<1 second)
- ✅ No QEMU, no bootloader, no kernel required
- ✅ Catch bugs BEFORE wasting time on kernel build
- ✅ Standard development workflow

## Architecture

```
kernel/
├── tests/                    # Host-side unit tests
│   ├── host_test.h          # Test harness (simple, no dependencies)
│   ├── pmm_test.c           # PMM unit tests
│   ├── mmu_test.c           # MMU unit tests
│   └── bitmap_test.c        # Bitmap operations
├── mm/
│   ├── pmm.c                # Kernel PMM (with #ifdef HOST_TEST for mocking)
│   └── pmm.h
├── test_build/              # Compiled test binaries
│   └── test_runner          # Single executable
└── Makefile                 # With 'make test' target
```

## Workflow

```bash
# Developer workflow:
make test           # Compiles and runs host tests (< 1 second)
make build          # Only builds if tests pass
make run            # Boots kernel in QEMU

# CI/CD workflow:
make test && make build && make integration-test
```

## Test Harness (Simple, No External Dependencies)

**File: `tests/host_test.h`**

```c
#ifndef HOST_TEST_H
#define HOST_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// Test result tracking
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Color output
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_RESET   "\x1b[0m"

// Assert macros
#define TEST_ASSERT(condition, message) do { \
    if (!(condition)) { \
        printf(COLOR_RED "  FAIL: %s\n" COLOR_RESET, message); \
        return 0; \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b, message) do { \
    if ((a) != (b)) { \
        printf(COLOR_RED "  FAIL: %s (expected %lu, got %lu)\n" COLOR_RESET, \
               message, (unsigned long)(b), (unsigned long)(a)); \
        return 0; \
    } \
} while(0)

#define TEST_ASSERT_NEQ(a, b, message) do { \
    if ((a) == (b)) { \
        printf(COLOR_RED "  FAIL: %s (values should differ)\n" COLOR_RESET, message); \
        return 0; \
    } \
} while(0)

// Test registration
#define TEST(name) \
    static int test_##name(void); \
    static void __attribute__((constructor)) register_##name(void) { \
        run_test(#name, test_##name); \
    } \
    static int test_##name(void)

// Test runner
static void run_test(const char* name, int (*test_fn)(void)) {
    tests_run++;
    printf("[ TEST ] %s ... ", name);
    fflush(stdout);

    if (test_fn()) {
        printf(COLOR_GREEN "PASS\n" COLOR_RESET);
        tests_passed++;
    } else {
        tests_failed++;
    }
}

// Summary
static void __attribute__((destructor)) print_summary(void) {
    printf("\n");
    printf("========================================\n");
    printf("Tests run: %d\n", tests_run);
    printf(COLOR_GREEN "Passed:    %d\n" COLOR_RESET, tests_passed);
    if (tests_failed > 0) {
        printf(COLOR_RED "Failed:    %d\n" COLOR_RESET, tests_failed);
        exit(1);
    } else {
        printf(COLOR_GREEN "ALL TESTS PASSED!\n" COLOR_RESET);
        exit(0);
    }
}

#endif // HOST_TEST_H
```

## Mocking Kernel Dependencies

**Problem:** PMM uses `kprintf()`, `kassert()`, etc. which don't exist on host

**Solution:** Conditional compilation

**File: `mm/pmm.c`** (modified)

```c
// At top of file:
#ifdef HOST_TEST
    // Host-side mocks
    #define kprintf printf
    #define kassert(cond) assert(cond)
    #define kassert_aligned(addr, align) assert(((addr) % (align)) == 0)
    #define kassert_not_null(ptr) assert((ptr) != NULL)

    #define PRECONDITION(msg) (void)msg
    #define POSTCONDITION(msg) (void)msg
    #define INVARIANT(msg) (void)msg
#else
    // Kernel includes
    #include <kernel/assert.h>
    #include <drivers/vga.h>
#endif
```

## Example Test File

**File: `tests/pmm_test.c`**

```c
#include "host_test.h"
#include "../include/kernel/pmm.h"
#include "../include/kernel/types.h"

// Mock multiboot info for testing
static struct multiboot_info test_mbi = {
    .flags = MULTIBOOT_FLAG_MMAP,
    .mmap_addr = 0,  // Will be set dynamically
    .mmap_length = 0
};

// Test: Frame addresses are 4K-aligned
TEST(pmm_frame_aligned) {
    // Initialize PMM with test data
    pmm_init(MULTIBOOT_MAGIC, &test_mbi);

    // Allocate 10 frames
    for (int i = 0; i < 10; i++) {
        phys_addr_t addr = pmm_alloc_page();
        TEST_ASSERT(addr != 0, "allocation succeeded");
        TEST_ASSERT((addr & 0xFFF) == 0, "address is 4K-aligned");
        TEST_ASSERT(addr >= 0x1000, "address >= 4096");
        pmm_free_page(addr);
    }

    return 1;  // PASS
}

// Test: Frame calculation correctness
TEST(pmm_frame_calculation) {
    pmm_init(MULTIBOOT_MAGIC, &test_mbi);

    phys_addr_t addr = pmm_alloc_page();
    TEST_ASSERT(addr != 0, "allocation succeeded");

    // Verify frame*4096 calculation
    uint32_t frame_num = addr / 4096;
    phys_addr_t reconstructed = frame_num * 4096;
    TEST_ASSERT_EQ(addr, reconstructed, "frame*4096 is reversible");

    // Specifically test frame 33 → 0x21000 (not 0xd34!)
    if (frame_num == 33) {
        TEST_ASSERT_EQ(addr, 0x21000, "frame 33 = 0x21000");
    }

    pmm_free_page(addr);
    return 1;
}

// Test: Total memory is reasonable
TEST(pmm_memory_sane) {
    pmm_init(MULTIBOOT_MAGIC, &test_mbi);

    struct pmm_stats stats;
    pmm_get_stats(&stats);

    // Should have at least 1MB (256 frames)
    TEST_ASSERT(stats.total_frames >= 256, "total >= 1MB");

    // Free should be <= total
    TEST_ASSERT(stats.free_frames <= stats.total_frames, "free <= total");

    return 1;
}
```

## Makefile Integration

```makefile
# Host test configuration
HOST_CC := gcc
HOST_CFLAGS := -std=gnu99 -O0 -g -Wall -Wextra -DHOST_TEST \
               -I$(INCLUDE_DIR) -fno-builtin

# Test sources (not compiled into kernel)
TEST_SOURCES := tests/pmm_test.c \
                tests/mmu_test.c \
                tests/bitmap_test.c

# Extract testable code (with mocks)
TESTABLE_SOURCES := mm/pmm.c

# Build host test runner
test: test_build/test_runner
	@echo "Running host-side unit tests..."
	@./test_build/test_runner

test_build/test_runner: $(TEST_SOURCES) $(TESTABLE_SOURCES)
	@mkdir -p test_build
	@$(HOST_CC) $(HOST_CFLAGS) -o $@ $(TEST_SOURCES) $(TESTABLE_SOURCES)

# Clean test artifacts
clean: clean-tests

clean-tests:
	@rm -rf test_build

.PHONY: test clean-tests
```

## Benefits

### 1. Instant Feedback
```bash
$ make test
Running host-side unit tests...
[ TEST ] pmm_frame_aligned ... PASS
[ TEST ] pmm_frame_calculation ... PASS
[ TEST ] pmm_memory_sane ... PASS

========================================
Tests run: 3
Passed:    3
ALL TESTS PASSED!

real    0m0.143s  # <1 second!
```

### 2. Catch Bugs Early
```bash
$ make test
Running host-side unit tests...
[ TEST ] pmm_frame_aligned ... PASS
[ TEST ] pmm_frame_calculation ...
  FAIL: frame 33 = 0x21000 (expected 135168, got 3383)

========================================
Tests run: 2
Passed:    1
Failed:    1
make: *** [test] Error 1

# BUG CAUGHT BEFORE BUILDING KERNEL!
```

### 3. TDD Workflow
```bash
# Write test first
$ vim tests/new_feature_test.c

# Test fails (feature not implemented)
$ make test
FAIL

# Implement feature
$ vim mm/new_feature.c

# Test passes
$ make test
PASS

# Now build kernel
$ make build
```

## Testing Strategy

### Unit Tests (Host)
- PMM allocation/deallocation
- Frame number calculations
- Bitmap operations
- MMU address calculations (not actual paging)
- String operations

### Integration Tests (Kernel)
- Keep existing kernel self-tests for:
  - Actual paging (requires CPU MMU)
  - Hardware timer calibration
  - Interrupt handling
  - Hardware-specific code

### What to Test Where

| Component | Host Test | Kernel Test |
|-----------|-----------|-------------|
| Frame calculations | ✅ YES | Optional |
| Bitmap operations | ✅ YES | Optional |
| PMM allocation logic | ✅ YES | Optional |
| Actual paging | ❌ NO | ✅ YES |
| Timer calibration | ❌ NO | ✅ YES |
| IDT/interrupts | ❌ NO | ✅ YES |

## Migration Plan

### Phase 1: Framework Setup (Week 1)
1. Create `tests/host_test.h`
2. Add `make test` to Makefile
3. Write 3 PMM tests as proof-of-concept

### Phase 2: Add Mocks (Week 2)
1. Add `#ifdef HOST_TEST` to PMM
2. Mock kprintf, kassert, etc.
3. Verify tests run successfully

### Phase 3: Expand Coverage (Ongoing)
1. Add MMU tests (address calculations)
2. Add bitmap tests
3. Add string operation tests
4. Target: 80% coverage of non-hardware code

### Phase 4: CI Integration
1. Add to GitHub Actions (if using)
2. Block merges if tests fail
3. Generate coverage reports

## Success Metrics

### Before (In-Kernel Tests)
- Feedback time: 10-30 seconds (QEMU boot)
- Can't run if kernel crashes
- No coverage measurement
- Manual testing required

### After (Host Tests)
- Feedback time: <1 second
- Always runs (no kernel required)
- Coverage reports available
- Automated CI/CD

## Tools (Optional Enhancements)

### Level 1: Simple (What we built)
- Custom harness in `host_test.h`
- Zero external dependencies
- Works anywhere

### Level 2: Standard Tools
- Replace custom harness with [Unity](https://github.com/ThrowTheSwitch/Unity)
- Add [CMock](https://github.com/ThrowTheSwitch/CMock) for mocking
- More features, still lightweight

### Level 3: Professional
- [Google Test](https://github.com/google/googletest) (C++)
- Coverage with `gcov`/`lcov`
- Sanitizers (ASAN, UBSAN)
- Valgrind for memory leaks

## Example Complete Workflow

```bash
# Day 1: Write feature
$ vim mm/new_allocator.c
$ vim mm/new_allocator.h

# Day 1: Write tests FIRST (TDD)
$ vim tests/new_allocator_test.c

TEST(allocator_basic) {
    void* ptr = new_alloc(4096);
    TEST_ASSERT(ptr != NULL, "allocation succeeds");
    TEST_ASSERT(IS_ALIGNED(ptr, 4096), "aligned to 4K");
    new_free(ptr);
    return 1;
}

# Day 1: Run tests (they fail - feature not implemented)
$ make test
[ TEST ] allocator_basic ... FAIL
make: *** [test] Error 1

# Day 1: Implement feature
$ vim mm/new_allocator.c
# ... implementation ...

# Day 1: Run tests (now pass!)
$ make test
[ TEST ] allocator_basic ... PASS
ALL TESTS PASSED!

# Day 1: Build kernel
$ make build
Build complete: kernel.iso

# Day 1: Test in QEMU
$ make run
# Everything works!

# Day 2: Add to CI
$ git commit -m "Add new allocator with tests"
$ git push
# CI runs 'make test' automatically
# Merge only if green
```

## Conclusion

Host-side unit testing is **essential** for:
- Fast iteration
- Early bug detection
- Confidence in refactoring
- Professional development workflow

**Next Steps:**
1. Implement `tests/host_test.h`
2. Add `make test` to Makefile
3. Write 5 PMM tests
4. Verify all pass
5. Make `make build` depend on `make test`

**Time Investment:** ~4 hours to set up, saves hours per day afterward
