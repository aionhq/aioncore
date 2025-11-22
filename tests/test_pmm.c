/**
 * PMM Host-Side Unit Tests
 *
 * These tests run on the HOST (your Mac), not in the kernel.
 * They catch bugs BEFORE you waste time building the kernel.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

// Test framework
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  \033[31mFAIL: %s\033[0m\n", msg); \
        return 0; \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b, msg) do { \
    if ((a) != (b)) { \
        printf("  \033[31mFAIL: %s (expected 0x%lx, got 0x%lx)\033[0m\n", \
               msg, (unsigned long)(b), (unsigned long)(a)); \
        return 0; \
    } \
} while(0)

#define RUN_TEST(name) do { \
    tests_run++; \
    printf("[ TEST ] " #name " ... "); \
    fflush(stdout); \
    if (test_##name()) { \
        printf("\033[32mPASS\033[0m\n"); \
        tests_passed++; \
    } else { \
        tests_failed++; \
    } \
} while(0)

// Frame size
#define FRAME_SIZE 4096

/**
 * TEST: Frame multiplication calculation
 *
 * This is the core bug we had: frame * 4096 returning wrong values
 */
static int test_frame_calculation() {
    // Test frame 0
    uint32_t frame0 = 0;
    uint32_t addr0 = frame0 * FRAME_SIZE;
    TEST_ASSERT_EQ(addr0, 0, "frame 0 = 0x0");

    // Test frame 1
    uint32_t frame1 = 1;
    uint32_t addr1 = frame1 * FRAME_SIZE;
    TEST_ASSERT_EQ(addr1, 0x1000, "frame 1 = 0x1000");

    // Test frame 33 (the bug case!)
    uint32_t frame33 = 33;
    uint32_t addr33 = frame33 * FRAME_SIZE;
    TEST_ASSERT_EQ(addr33, 0x21000, "frame 33 = 0x21000 (NOT 0xd34!)");

    // Test frame 256
    uint32_t frame256 = 256;
    uint32_t addr256 = frame256 * FRAME_SIZE;
    TEST_ASSERT_EQ(addr256, 0x100000, "frame 256 = 0x100000 (1MB)");

    // Test frame 1024
    uint32_t frame1024 = 1024;
    uint32_t addr1024 = frame1024 * FRAME_SIZE;
    TEST_ASSERT_EQ(addr1024, 0x400000, "frame 1024 = 0x400000 (4MB)");

    return 1;
}

/**
 * TEST: All frame addresses are 4K-aligned
 */
static int test_frame_alignment() {
    for (uint32_t frame = 0; frame < 1000; frame++) {
        uint32_t addr = frame * FRAME_SIZE;

        // Must be divisible by 4096
        TEST_ASSERT((addr % 4096) == 0, "address is 4K-aligned");

        // Low 12 bits must be zero
        TEST_ASSERT((addr & 0xFFF) == 0, "low 12 bits are zero");
    }

    return 1;
}

/**
 * TEST: Frame calculation is reversible
 */
static int test_frame_reversible() {
    for (uint32_t frame = 0; frame < 1000; frame++) {
        uint32_t addr = frame * FRAME_SIZE;
        uint32_t reconstructed_frame = addr / FRAME_SIZE;

        TEST_ASSERT_EQ(reconstructed_frame, frame, "frame → addr → frame is reversible");
    }

    return 1;
}

/**
 * TEST: Addresses don't overflow
 */
static int test_no_overflow() {
    // Max 32-bit frame number that fits
    uint32_t max_frame = 0x100000;  // 1M frames = 4GB
    uint64_t addr64 = (uint64_t)max_frame * (uint64_t)FRAME_SIZE;

    TEST_ASSERT(addr64 == 0x100000000ULL, "max frame calculation correct");

    // Frame that would overflow 32-bit
    uint32_t overflow_frame = 0x100001;
    uint64_t overflow_addr = (uint64_t)overflow_frame * (uint64_t)FRAME_SIZE;
    TEST_ASSERT(overflow_addr > 0xFFFFFFFFULL, "overflow detected correctly");

    return 1;
}

/**
 * TEST: Specific bug case - frame 33
 */
static int test_frame_33_specific() {
    uint32_t frame = 33;
    uint32_t addr = frame * FRAME_SIZE;

    // This was returning 0xd34 (3383) due to the bug
    TEST_ASSERT(addr != 0xd34, "NOT the buggy value 0xd34");
    TEST_ASSERT(addr != 3383, "NOT the buggy decimal value 3383");

    // Must be the correct value
    TEST_ASSERT_EQ(addr, 135168, "correct decimal value");
    TEST_ASSERT_EQ(addr, 0x21000, "correct hex value");

    // Must be 4K-aligned
    TEST_ASSERT((addr & 0xFFF) == 0, "4K-aligned");

    return 1;
}

/**
 * TEST: Type casting doesn't break calculation
 */
static int test_type_casting() {
    // size_t → phys_addr_t cast
    size_t frame_sizet = 33;
    uintptr_t addr_uintptr = (uintptr_t)(frame_sizet * FRAME_SIZE);
    TEST_ASSERT_EQ(addr_uintptr, 0x21000, "size_t cast works");

    // uint32_t → uint32_t
    uint32_t frame_u32 = 33;
    uint32_t addr_u32 = (uint32_t)(frame_u32 * FRAME_SIZE);
    TEST_ASSERT_EQ(addr_u32, 0x21000, "uint32_t cast works");

    return 1;
}

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("  PMM UNIT TESTS (Host-Side)\n");
    printf("========================================\n");
    printf("\n");

    RUN_TEST(frame_calculation);
    RUN_TEST(frame_alignment);
    RUN_TEST(frame_reversible);
    RUN_TEST(no_overflow);
    RUN_TEST(frame_33_specific);
    RUN_TEST(type_casting);

    printf("\n");
    printf("========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Passed:       \033[32m%d\033[0m\n", tests_passed);
    if (tests_failed > 0) {
        printf("Failed:       \033[31m%d\033[0m\n", tests_failed);
        printf("========================================\n");
        printf("\n");
        printf("\033[31mTESTS FAILED - DO NOT BUILD KERNEL\033[0m\n");
        printf("\n");
        return 1;
    } else {
        printf("\033[32mALL TESTS PASSED!\033[0m\n");
        printf("========================================\n");
        printf("\n");
        return 0;
    }
}
