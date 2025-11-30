/*
 * Console Multiplexer
 * Provides abstraction layer for sending output to multiple console backends
 * (VGA, serial, etc.) simultaneously
 */

#ifndef KERNEL_CONSOLE_H
#define KERNEL_CONSOLE_H

#include <stddef.h>
#include <stdbool.h>

/* Forward declarations */
enum vga_color;

/* Console backend interface */
struct console_backend {
	const char* name;

	/* Initialize backend (called once at boot) */
	int (*init)(void);

	/* Write a single character */
	void (*putchar)(char c);

	/* Write a string of specified length */
	void (*write)(const char* str, size_t len);

	/* Set text color (optional, can be NULL) */
	void (*set_color)(enum vga_color fg, enum vga_color bg);

	/* Clear screen (optional, can be NULL) */
	void (*clear)(void);

	/* Backend-specific data */
	void* priv;

	/* Is this backend enabled? */
	bool enabled;
};

/*
 * Initialize console subsystem
 * Must be called before any console operations
 */
void console_init(void);

/*
 * Register a console backend
 * Backends will receive all console output
 * Returns 0 on success, -1 on failure
 */
int console_register(struct console_backend* backend);

/*
 * Unregister a console backend
 * Returns 0 on success, -1 if not found
 */
int console_unregister(struct console_backend* backend);

/*
 * Enable or disable a console backend
 */
void console_enable(struct console_backend* backend, bool enable);

/*
 * Write a character to all registered console backends
 */
void console_putchar(char c);

/*
 * Write a string to all registered console backends
 */
void console_write(const char* str, size_t len);

/*
 * Set text color on all backends that support it
 */
void console_set_color(enum vga_color fg, enum vga_color bg);

/*
 * Clear all backends that support it
 */
void console_clear(void);

#endif /* KERNEL_CONSOLE_H */
