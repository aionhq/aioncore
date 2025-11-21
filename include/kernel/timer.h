#ifndef KERNEL_TIMER_H
#define KERNEL_TIMER_H

#include <stdint.h>

/**
 * Timer subsystem - Provides microsecond-precision timing via TSC calibration
 *
 * Uses PIT (Programmable Interval Timer) to calibrate the TSC (Time Stamp Counter)
 * for accurate microsecond timing.
 */

// Initialize timer hardware and calibrate TSC
// frequency_hz: Timer interrupt frequency (typically 1000 Hz)
void timer_init(uint32_t frequency_hz);

// Read TSC (Time Stamp Counter) directly
// Returns: CPU cycle count since boot
uint64_t timer_read_tsc(void);

// Read time in microseconds
// Returns: Microseconds since boot (calibrated via TSC)
uint64_t timer_read_us(void);

// Get TSC frequency in Hz (available after calibration)
uint64_t timer_get_tsc_freq(void);

// Timer interrupt handler (called by IRQ 0 -> INT 32)
void timer_interrupt_handler(void);

#endif // KERNEL_TIMER_H
