/**
 * Kernel Testing Framework Implementation
 *
 * Runs tests registered in the .ktests section at boot time.
 */

#include <kernel/ktest.h>
#include <drivers/vga.h>

// External symbols provided by linker for .ktests section
extern struct ktest __start_ktests[];
extern struct ktest __stop_ktests[];

/**
 * Run all registered kernel tests
 * Returns: Number of failed tests (0 = all pass)
 */
int ktest_run_all(void) {
    struct ktest *test;
    int total = 0;
    int passed = 0;
    int failed = 0;

    kprintf("\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("========================================\n");
    kprintf("  KERNEL TEST SUITE\n");
    kprintf("========================================\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("\n");

    // Iterate over all tests in .ktests section
    for (test = __start_ktests; test < __stop_ktests; test++) {
        total++;

        kprintf("[TEST] %s::%s ... ", test->subsystem, test->name);

        int result = test->test_fn();

        if (result == KTEST_PASS) {
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            kprintf("PASS\n");
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            passed++;
        } else {
            vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            kprintf("FAIL\n");
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            failed++;
        }
    }

    kprintf("\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("========================================\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("Tests run: %d\n", total);

    if (passed > 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        kprintf("Passed:    %d\n", passed);
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }

    if (failed > 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        kprintf("Failed:    %d\n", failed);
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    kprintf("========================================\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kprintf("\n");

    return failed;
}

// Simple string comparison helper
static int str_equal(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

/**
 * Run tests for a specific subsystem
 * Returns: Number of failed tests (0 = all pass)
 */
int ktest_run_subsystem(const char *subsystem) {
    struct ktest *test;
    int total = 0;
    int passed = 0;
    int failed = 0;

    kprintf("\n[TEST] Running tests for subsystem: %s\n", subsystem);

    // Iterate over all tests, run only matching subsystem
    for (test = __start_ktests; test < __stop_ktests; test++) {
        if (!str_equal(test->subsystem, subsystem)) {
            continue;
        }

        total++;
        kprintf("  %s ... ", test->name);

        int result = test->test_fn();

        if (result == KTEST_PASS) {
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            kprintf("PASS\n");
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            passed++;
        } else {
            vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            kprintf("FAIL\n");
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            failed++;
        }
    }

    kprintf("[TEST] %s: %d/%d passed\n\n", subsystem, passed, total);

    return failed;
}
