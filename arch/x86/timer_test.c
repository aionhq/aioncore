/**
 * Unit tests for timer subsystem
 *
 * These tests verify timer functionality and TSC calibration
 */

#include <kernel/ktest.h>
#include <kernel/timer.h>
#include <kernel/hal.h>
#include <kernel/percpu.h>

// Test: TSC monotonicity
// Verifies that TSC always increases
static int test_tsc_monotonic(void) {
    uint64_t t1 = timer_read_tsc();
    uint64_t t2 = timer_read_tsc();
    uint64_t t3 = timer_read_tsc();

    KTEST_ASSERT(t2 > t1, "TSC is monotonic (t2 > t1)");
    KTEST_ASSERT(t3 > t2, "TSC is monotonic (t3 > t2)");

    return KTEST_PASS;
}

// Test: Timer calibration completed
// Verifies that TSC frequency was calibrated (non-zero)
static int test_timer_calibrated(void) {
    uint64_t freq = timer_get_tsc_freq();

    KTEST_ASSERT_NEQ(freq, 0, "TSC frequency is calibrated");

    // TSC frequency should be reasonable (100 MHz - 10 GHz)
    KTEST_ASSERT(freq > 100000000ULL, "TSC freq > 100 MHz");
    KTEST_ASSERT(freq < 10000000000ULL, "TSC freq < 10 GHz");

    return KTEST_PASS;
}

// Test: Microsecond timer advances
// Verifies that timer_read_us() increases over time
static int test_timer_us_advances(void) {
    uint64_t t1 = timer_read_us();

    // Busy-wait for a bit (read TSC ~1000 times)
    for (int i = 0; i < 1000; i++) {
        timer_read_tsc();
    }

    uint64_t t2 = timer_read_us();

    KTEST_ASSERT(t2 > t1, "timer_read_us() advances over time");

    return KTEST_PASS;
}

// Test: Per-CPU tick counter increments
// Verifies that timer interrupts are firing and incrementing tick counter
// Note: This test requires interrupts to be enabled
static int test_timer_ticks_increment(void) {
    struct per_cpu_data *cpu = this_cpu();
    uint64_t tick1 = cpu->ticks;

    // Enable interrupts briefly to let timer tick
    hal->irq_enable();

    // Busy-wait for multiple timer ticks
    // At 1000 Hz, waiting for 100 TSC reads should see at least one tick
    for (volatile int i = 0; i < 10000; i++) {
        if (cpu->ticks > tick1) {
            break;
        }
    }

    uint64_t tick2 = cpu->ticks;

    KTEST_ASSERT(tick2 > tick1, "Timer ticks increment with interrupts enabled");

    return KTEST_PASS;
}

// Register all timer tests
KTEST_DEFINE("timer", tsc_monotonic, test_tsc_monotonic);
KTEST_DEFINE("timer", timer_calibrated, test_timer_calibrated);
KTEST_DEFINE("timer", timer_us_advances, test_timer_us_advances);
KTEST_DEFINE("timer", timer_ticks_increment, test_timer_ticks_increment);
