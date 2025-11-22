/**
 * x86 Timer Driver - PIT (Programmable Interval Timer) + TSC Calibration
 *
 * This driver:
 * 1. Initializes the 8254 PIT at a specified frequency
 * 2. Calibrates the TSC (Time Stamp Counter) against the PIT
 * 3. Provides microsecond-precision timing via calibrated TSC
 *
 * Real-time constraints:
 * - Timer interrupt handler must be <100 cycles
 * - TSC read is O(1) - single RDTSC instruction
 * - No dynamic allocation in timer paths
 */

#include <kernel/timer.h>
#include <kernel/hal.h>
#include <kernel/percpu.h>
#include <kernel/scheduler.h>
#include <kernel/idt.h>
#include <drivers/vga.h>

// ========== PIT Hardware Constants ==========

// PIT I/O ports
#define PIT_CHANNEL0    0x40    // Channel 0 data port (system timer)
#define PIT_CHANNEL1    0x41    // Channel 1 data port (unused)
#define PIT_CHANNEL2    0x42    // Channel 2 data port (PC speaker)
#define PIT_COMMAND     0x43    // Mode/Command register

// PIT command byte bits
#define PIT_CMD_BINARY      0x00    // Binary mode (vs BCD)
#define PIT_CMD_MODE2       0x04    // Mode 2: Rate generator
#define PIT_CMD_MODE3       0x06    // Mode 3: Square wave
#define PIT_CMD_RW_BOTH     0x30    // Read/Write LSB then MSB
#define PIT_CMD_CHANNEL0    0x00    // Select channel 0

// PIT base frequency (Hz)
#define PIT_BASE_FREQ   1193182

// Calibration duration (in PIT ticks)
// At 1000 Hz, 50 ticks = 50ms calibration period
#define CALIBRATION_TICKS   50

// ========== Global State ==========

static uint64_t tsc_freq_hz = 0;        // TSC frequency in Hz (calibrated)
static uint32_t timer_freq_hz = 0;      // Timer interrupt frequency

// ========== PIT Operations ==========

/**
 * Initialize PIT channel 0 to generate interrupts at specified frequency
 */
static void pit_init(uint32_t frequency_hz) {
    // Calculate divisor for desired frequency
    // PIT_BASE_FREQ / divisor = frequency_hz
    uint32_t divisor = PIT_BASE_FREQ / frequency_hz;

    if (divisor > 65535) {
        divisor = 65535;  // Max 16-bit value
    }
    if (divisor < 1) {
        divisor = 1;
    }

    // Configure PIT channel 0: Rate generator, binary mode, LSB+MSB
    uint8_t command = PIT_CMD_CHANNEL0 | PIT_CMD_RW_BOTH | PIT_CMD_MODE2 | PIT_CMD_BINARY;
    hal->io_outb(PIT_COMMAND, command);

    // Send divisor (LSB first, then MSB)
    hal->io_outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    hal->io_outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}

/**
 * Wait for a specified number of PIT ticks
 * Used during TSC calibration
 */
static void pit_wait_ticks(uint32_t ticks) {
    // Read current PIT counter value
    // We need to send a latch command first
    hal->io_outb(PIT_COMMAND, 0x00);  // Latch channel 0

    uint8_t low = hal->io_inb(PIT_CHANNEL0);
    uint8_t high = hal->io_inb(PIT_CHANNEL0);
    uint16_t start_count = (high << 8) | low;

    // Calculate target count
    // Note: PIT counts DOWN, so we need to handle wraparound
    uint32_t total_ticks = 0;
    uint16_t last_count = start_count;

    while (total_ticks < ticks) {
        // Read current count
        hal->io_outb(PIT_COMMAND, 0x00);  // Latch channel 0
        low = hal->io_inb(PIT_CHANNEL0);
        high = hal->io_inb(PIT_CHANNEL0);
        uint16_t current_count = (high << 8) | low;

        // Calculate ticks elapsed (handle countdown)
        if (current_count > last_count) {
            // Wrapped around (reached 0 and reloaded)
            total_ticks += (last_count + (65536 - current_count));
        } else {
            total_ticks += (last_count - current_count);
        }

        last_count = current_count;
    }
}

/**
 * Calibrate TSC frequency against PIT
 *
 * Measures TSC cycles over a known PIT period to determine TSC frequency.
 * This is critical for accurate microsecond timing.
 */
static void calibrate_tsc(void) {
    kprintf("[TIMER] Calibrating TSC...\n");

    // Disable interrupts during calibration
    uint32_t flags = hal->irq_disable();

    // Read TSC at start
    uint64_t tsc_start = hal->timer_read_tsc();

    // Wait for CALIBRATION_TICKS PIT ticks
    pit_wait_ticks(CALIBRATION_TICKS);

    // Read TSC at end
    uint64_t tsc_end = hal->timer_read_tsc();

    // Restore interrupts
    hal->irq_restore(flags);

    // Calculate TSC cycles elapsed
    uint64_t tsc_cycles = tsc_end - tsc_start;

    // Calculate time elapsed in microseconds
    // time_us = (CALIBRATION_TICKS * 1000000) / timer_freq_hz
    uint64_t time_us = ((uint64_t)CALIBRATION_TICKS * 1000000ULL) / timer_freq_hz;

    // Calculate TSC frequency: cycles / time_in_seconds
    // tsc_freq = tsc_cycles / (time_us / 1000000)
    //          = (tsc_cycles * 1000000) / time_us
    tsc_freq_hz = (tsc_cycles * 1000000ULL) / time_us;

    kprintf("[TIMER] TSC calibrated: %lu MHz (%llu Hz)\n",
            (unsigned long)(tsc_freq_hz / 1000000),
            tsc_freq_hz);
}

// ========== Public Timer API ==========

/**
 * Initialize timer subsystem
 *
 * Steps:
 * 1. Initialize PIT at specified frequency
 * 2. Calibrate TSC against PIT
 * 3. Register timer interrupt handler
 */
void timer_init(uint32_t frequency_hz) {
    timer_freq_hz = frequency_hz;

    kprintf("[TIMER] Initializing PIT at %lu Hz\n", (unsigned long)frequency_hz);

    // Initialize PIT hardware
    pit_init(frequency_hz);

    // Calibrate TSC for microsecond timing
    calibrate_tsc();

    // Register timer interrupt handler (IRQ 0 -> INT 32)
    // Note: PIC remapping maps IRQ 0 to INT 32
    hal->irq_register(32, timer_interrupt_handler);

    // Unmask IRQ 0 in the PIC so timer interrupts can fire
    // By default, all IRQs are masked (0xFF) after pic_remap()
    irq_clear_mask(0);

    kprintf("[TIMER] Timer initialized successfully (IRQ 0 unmasked)\n");
}

/**
 * Read TSC (Time Stamp Counter)
 *
 * Returns raw CPU cycle count. Use timer_read_us() for calibrated time.
 */
uint64_t timer_read_tsc(void) {
    uint32_t low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

/**
 * Read time in microseconds
 *
 * Uses calibrated TSC frequency to convert cycles to microseconds.
 * This is the primary timing function for RT constraints.
 */
uint64_t timer_read_us(void) {
    if (tsc_freq_hz == 0) {
        // TSC not calibrated yet, return 0
        return 0;
    }

    uint64_t tsc = timer_read_tsc();

    // Convert TSC cycles to microseconds:
    // microseconds = (tsc * 1000000) / tsc_freq_hz
    //
    // To avoid overflow, we rearrange:
    // microseconds = tsc / (tsc_freq_hz / 1000000)
    return tsc / (tsc_freq_hz / 1000000ULL);
}

/**
 * Get TSC frequency in Hz
 */
uint64_t timer_get_tsc_freq(void) {
    return tsc_freq_hz;
}

/**
 * Timer interrupt handler (IRQ 0 -> INT 32)
 *
 * Called on every timer tick. Updates per-CPU tick counter and
 * sets scheduler preemption flag.
 *
 * NOTE: We do NOT call schedule() here because we're in interrupt context.
 * Calling schedule() from an interrupt leaves the interrupt frame on the
 * old task's stack without executing IRET, which corrupts the stack and
 * causes triple faults.
 *
 * Instead, scheduler_tick() sets g_scheduler.need_resched, which will be
 * checked at safe yield points (task_yield, syscalls, etc).
 *
 * TODO Phase 4: Add proper preemption by checking need_resched after IRET
 * in a safe kernel entry stub.
 *
 * RT constraint: Must complete in <100 cycles
 */
void timer_interrupt_handler(void) {
    // Update per-CPU tick counter
    struct per_cpu_data* cpu = this_cpu();
    cpu->ticks++;

    // Call scheduler tick (updates accounting, sets need_resched flag)
    // This does NOT actually schedule, just sets a flag
    scheduler_tick();

    // Send EOI to PIC (End of Interrupt)
    // IRQ 0 is on master PIC, so just send to master (port 0x20)
    hal->io_outb(0x20, 0x20);

    // Return via IRET (do NOT call schedule() here!)
    // Preemption will happen on next safe yield point
}
