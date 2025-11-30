/*
 * VGA Console Backend
 * Adapter between console multiplexer and VGA driver
 */

#include <kernel/console.h>
#include <drivers/vga.h>

/* External VGA driver */
extern struct vga_ops* vga;

/* VGA console backend operations */

static int vga_console_init(void)
{
	/* VGA already initialized by vga_subsystem_init() */
	return (vga != NULL) ? 0 : -1;
}

static void vga_console_putchar(char c)
{
	if (vga && vga->putchar) {
		vga->putchar(c);
	}
}

static void vga_console_write(const char* str, size_t len)
{
	if (vga && vga->write) {
		vga->write(str, len);
	}
}

static void vga_console_set_color(enum vga_color fg, enum vga_color bg)
{
	if (vga && vga->set_color) {
		vga->set_color(fg, bg);
	}
}

static void vga_console_clear(void)
{
	if (vga && vga->clear) {
		vga->clear();
	}
}

/* VGA console backend descriptor */
static struct console_backend vga_console = {
	.name = "vga",
	.init = vga_console_init,
	.putchar = vga_console_putchar,
	.write = vga_console_write,
	.set_color = vga_console_set_color,
	.clear = vga_console_clear,
	.priv = NULL,
	.enabled = false
};

/*
 * Get VGA console backend
 */
struct console_backend* vga_get_console_backend(void)
{
	return &vga_console;
}
