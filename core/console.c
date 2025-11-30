/*
 * Console Multiplexer Implementation
 * Routes output to multiple console backends (VGA, serial, etc.)
 */

#include <kernel/console.h>
#include <drivers/vga.h>
#include <stddef.h>

/* Maximum number of console backends */
#define MAX_CONSOLE_BACKENDS 4

/* Registered console backends */
static struct console_backend* backends[MAX_CONSOLE_BACKENDS];
static size_t backend_count = 0;

/*
 * Initialize console subsystem
 */
void console_init(void)
{
	backend_count = 0;
	for (size_t i = 0; i < MAX_CONSOLE_BACKENDS; i++) {
		backends[i] = NULL;
	}
}

/*
 * Register a console backend
 */
int console_register(struct console_backend* backend)
{
	if (!backend) {
		return -1;
	}

	if (backend_count >= MAX_CONSOLE_BACKENDS) {
		return -1;
	}

	/* Initialize backend if init function provided */
	if (backend->init && backend->init() != 0) {
		return -1;
	}

	backends[backend_count++] = backend;
	backend->enabled = true;
	return 0;
}

/*
 * Unregister a console backend
 */
int console_unregister(struct console_backend* backend)
{
	if (!backend) {
		return -1;
	}

	for (size_t i = 0; i < backend_count; i++) {
		if (backends[i] == backend) {
			/* Shift remaining backends down */
			for (size_t j = i; j < backend_count - 1; j++) {
				backends[j] = backends[j + 1];
			}
			backends[--backend_count] = NULL;
			return 0;
		}
	}

	return -1; /* Not found */
}

/*
 * Enable or disable a console backend
 */
void console_enable(struct console_backend* backend, bool enable)
{
	if (backend) {
		backend->enabled = enable;
	}
}

/*
 * Write a character to all registered console backends
 */
void console_putchar(char c)
{
	for (size_t i = 0; i < backend_count; i++) {
		if (backends[i] && backends[i]->enabled && backends[i]->putchar) {
			backends[i]->putchar(c);
		}
	}
}

/*
 * Write a string to all registered console backends
 */
void console_write(const char* str, size_t len)
{
	for (size_t i = 0; i < backend_count; i++) {
		if (backends[i] && backends[i]->enabled && backends[i]->write) {
			backends[i]->write(str, len);
		}
	}
}

/*
 * Set text color on all backends that support it
 */
void console_set_color(enum vga_color fg, enum vga_color bg)
{
	for (size_t i = 0; i < backend_count; i++) {
		if (backends[i] && backends[i]->enabled && backends[i]->set_color) {
			backends[i]->set_color(fg, bg);
		}
	}
}

/*
 * Clear all backends that support it
 */
void console_clear(void)
{
	for (size_t i = 0; i < backend_count; i++) {
		if (backends[i] && backends[i]->enabled && backends[i]->clear) {
			backends[i]->clear();
		}
	}
}
