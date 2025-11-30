/*
 * Serial Console Backend
 * Adapter between console multiplexer and serial UART driver
 */

#include <kernel/console.h>
#include <drivers/serial.h>

/* Serial port instance (COM1) */
static struct serial_port com1;

/* Serial console backend operations */

static int serial_console_init(void)
{
	/* Initialize COM1 at 115200 baud */
	return serial_init(&com1, SERIAL_COM1);
}

static void serial_console_putchar(char c)
{
	serial_putchar(&com1, c);
}

static void serial_console_write(const char* str, size_t len)
{
	serial_write(&com1, str, len);
}

/* Serial console backend descriptor */
static struct console_backend serial_console = {
	.name = "serial",
	.init = serial_console_init,
	.putchar = serial_console_putchar,
	.write = serial_console_write,
	.set_color = NULL,  /* Serial doesn't support colors */
	.clear = NULL,      /* Serial doesn't support clear */
	.priv = NULL,
	.enabled = false
};

/*
 * Get serial console backend
 */
struct console_backend* serial_get_console_backend(void)
{
	return &serial_console;
}
