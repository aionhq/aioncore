/*
 * Serial UART Driver (8250/16550 compatible)
 * Provides serial port I/O for debugging and console output
 */

#ifndef DRIVERS_SERIAL_H
#define DRIVERS_SERIAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Serial port definitions */
#define SERIAL_COM1 0x3F8
#define SERIAL_COM2 0x2F8
#define SERIAL_COM3 0x3E8
#define SERIAL_COM4 0x2E8

/* Serial port configuration */
struct serial_config {
	uint16_t port;      /* I/O port base address */
	uint32_t baud_rate; /* Baud rate (e.g., 9600, 115200) */
	uint8_t data_bits;  /* Data bits (5-8) */
	uint8_t stop_bits;  /* Stop bits (1 or 2) */
	bool parity;        /* Parity enabled */
};

/* Serial port handle */
struct serial_port {
	uint16_t port;
	bool initialized;
};

/*
 * Initialize serial port
 * Returns 0 on success, -1 on failure
 */
int serial_init(struct serial_port* serial, uint16_t port);

/*
 * Initialize serial port with custom configuration
 * Returns 0 on success, -1 on failure
 */
int serial_init_config(struct serial_port* serial, const struct serial_config* config);

/*
 * Write a single character to serial port
 * Blocks until transmit buffer is ready
 */
void serial_putchar(struct serial_port* serial, char c);

/*
 * Write a string to serial port
 */
void serial_write(struct serial_port* serial, const char* str, size_t len);

/*
 * Read a character from serial port (non-blocking)
 * Returns -1 if no data available, character otherwise
 */
int serial_getchar(struct serial_port* serial);

/*
 * Check if data is available to read
 */
bool serial_data_available(struct serial_port* serial);

/*
 * Check if transmit buffer is empty
 */
bool serial_transmit_empty(struct serial_port* serial);

/* Get serial console backend for console multiplexer */
struct console_backend* serial_get_console_backend(void);

#endif /* DRIVERS_SERIAL_H */
