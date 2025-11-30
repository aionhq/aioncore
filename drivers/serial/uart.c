/*
 * Serial UART Driver (8250/16550 compatible)
 * Implements basic serial port I/O for debugging and console output
 */

#include <drivers/serial.h>
#include <kernel/hal.h>

/* External HAL pointer */
extern struct hal_ops* hal;

/* UART register offsets */
#define UART_DATA         0  /* Data register (R/W) */
#define UART_INT_ENABLE   1  /* Interrupt enable register */
#define UART_FIFO_CTRL    2  /* FIFO control register */
#define UART_LINE_CTRL    3  /* Line control register */
#define UART_MODEM_CTRL   4  /* Modem control register */
#define UART_LINE_STATUS  5  /* Line status register */
#define UART_MODEM_STATUS 6  /* Modem status register */

/* Line status register bits */
#define UART_LSR_DATA_READY    (1 << 0)  /* Data ready */
#define UART_LSR_TRANSMIT_EMPTY (1 << 5) /* Transmit buffer empty */

/* Line control register bits */
#define UART_LCR_DLAB          (1 << 7)  /* Divisor latch access bit */
#define UART_LCR_8BITS         0x03      /* 8 data bits */

/* FIFO control register bits */
#define UART_FCR_ENABLE        0x01      /* Enable FIFO */
#define UART_FCR_CLEAR_RX      0x02      /* Clear receive FIFO */
#define UART_FCR_CLEAR_TX      0x04      /* Clear transmit FIFO */

/* Modem control register bits */
#define UART_MCR_DTR           0x01      /* Data terminal ready */
#define UART_MCR_RTS           0x02      /* Request to send */
#define UART_MCR_OUT2          0x08      /* Out2 (enables interrupts) */

/* Baud rate divisors (for 115200 base rate) */
#define UART_BAUD_115200       1
#define UART_BAUD_57600        2
#define UART_BAUD_38400        3
#define UART_BAUD_9600         12

/*
 * Initialize serial port with default configuration
 * Default: 115200 baud, 8 data bits, 1 stop bit, no parity
 */
int serial_init(struct serial_port* serial, uint16_t port)
{
	if (!serial) {
		return -1;
	}

	serial->port = port;

	/* Disable interrupts */
	hal->io_outb(port + UART_INT_ENABLE, 0x00);

	/* Enable DLAB to set baud rate divisor */
	hal->io_outb(port + UART_LINE_CTRL, UART_LCR_DLAB);

	/* Set baud rate to 115200 (divisor = 1) */
	hal->io_outb(port + UART_DATA, UART_BAUD_115200);        /* Low byte */
	hal->io_outb(port + UART_INT_ENABLE, 0x00);              /* High byte */

	/* Configure: 8 bits, no parity, one stop bit */
	hal->io_outb(port + UART_LINE_CTRL, UART_LCR_8BITS);

	/* Enable FIFO, clear both queues */
	hal->io_outb(port + UART_FIFO_CTRL, UART_FCR_ENABLE | UART_FCR_CLEAR_RX | UART_FCR_CLEAR_TX);

	/* Enable DTR, RTS, and OUT2 */
	hal->io_outb(port + UART_MODEM_CTRL, UART_MCR_DTR | UART_MCR_RTS | UART_MCR_OUT2);

	/* Serial port initialized successfully */
	/* Note: We skip the loopback test as QEMU's serial doesn't support it */
	serial->initialized = true;
	return 0;
}

/*
 * Initialize serial port with custom configuration
 */
int serial_init_config(struct serial_port* serial, const struct serial_config* config)
{
	if (!serial || !config) {
		return -1;
	}

	/* Currently only supports default configuration */
	/* Custom baud rates and settings can be added here */
	return serial_init(serial, config->port);
}

/*
 * Check if transmit buffer is empty
 */
bool serial_transmit_empty(struct serial_port* serial)
{
	if (!serial || !serial->initialized) {
		return false;
	}

	return (hal->io_inb(serial->port + UART_LINE_STATUS) & UART_LSR_TRANSMIT_EMPTY) != 0;
}

/*
 * Write a single character to serial port
 */
void serial_putchar(struct serial_port* serial, char c)
{
	if (!serial || !serial->initialized) {
		return;
	}

	/* Wait for transmit buffer to be empty */
	while (!serial_transmit_empty(serial)) {
		/* Busy wait */
	}

	/* Send character */
	hal->io_outb(serial->port + UART_DATA, c);
}

/*
 * Write a string to serial port
 */
void serial_write(struct serial_port* serial, const char* str, size_t len)
{
	if (!serial || !str) {
		return;
	}

	for (size_t i = 0; i < len; i++) {
		/* Convert LF to CRLF for proper terminal display */
		if (str[i] == '\n') {
			serial_putchar(serial, '\r');
		}
		serial_putchar(serial, str[i]);
	}
}

/*
 * Check if data is available to read
 */
bool serial_data_available(struct serial_port* serial)
{
	if (!serial || !serial->initialized) {
		return false;
	}

	return (hal->io_inb(serial->port + UART_LINE_STATUS) & UART_LSR_DATA_READY) != 0;
}

/*
 * Read a character from serial port (non-blocking)
 */
int serial_getchar(struct serial_port* serial)
{
	if (!serial || !serial->initialized) {
		return -1;
	}

	if (!serial_data_available(serial)) {
		return -1;
	}

	return hal->io_inb(serial->port + UART_DATA);
}
