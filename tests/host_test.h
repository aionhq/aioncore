#ifndef HOST_TEST_H
#define HOST_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

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

#define TEST_ASSERT_NULL(ptr, message) do { \
    if ((ptr) != NULL) { \
        printf(COLOR_RED "  FAIL: %s (expected NULL, got %p)\n" COLOR_RESET, \
               message, (void*)(ptr)); \
        return 0; \
    } \
} while(0)

#define TEST_ASSERT_NOT_NULL(ptr, message) do { \
    if ((ptr) == NULL) { \
        printf(COLOR_RED "  FAIL: %s (expected non-NULL)\n" COLOR_RESET, message); \
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
        printf("========================================\n");
        exit(1);
    } else {
        printf(COLOR_GREEN "ALL TESTS PASSED!\n" COLOR_RESET);
        printf("========================================\n");
        exit(0);
    }
}

// Main function (empty - tests run via constructors)
// Only include this once - in test_main.c
#ifdef TEST_MAIN_IMPL
int main(void) {
    // Tests are registered and run via __attribute__((constructor))
    // This main() just exists to satisfy the linker
    return 0;
}
#endif

#endif // HOST_TEST_H
