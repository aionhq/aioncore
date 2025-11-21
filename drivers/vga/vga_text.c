// VGA Text Mode Driver
// Modular implementation that can be replaced with other display drivers

#include <drivers/vga.h>
#include <kernel/types.h>
#include <kernel/hal.h>

// VGA text buffer address
#define VGA_MEMORY 0xB8000

// Hardware cursor control
#define VGA_CTRL_REG  0x3D4
#define VGA_DATA_REG  0x3D5

// Driver state
static struct {
    volatile uint16_t* buffer;
    uint16_t cursor_x;
    uint16_t cursor_y;
    uint8_t current_color;
    bool initialized;
} vga_state;

// Forward declarations
static int vga_text_init(void);
static void vga_text_shutdown(void);
static void vga_text_clear(void);
static void vga_text_putchar(char c);
static void vga_text_putchar_at(char c, uint16_t x, uint16_t y);
static void vga_text_write(const char* str, size_t len);
static void vga_text_write_at(const char* str, size_t len, uint16_t x, uint16_t y);
static void vga_text_set_color(enum vga_color fg, enum vga_color bg);
static void vga_text_move_cursor(uint16_t x, uint16_t y);
static void vga_text_get_cursor(uint16_t* x, uint16_t* y);
static void vga_text_scroll(void);
static void vga_text_cursor_enable(bool enable);

// Helper: create VGA color attribute
static inline uint8_t make_color(enum vga_color fg, enum vga_color bg) {
    return fg | (bg << 4);
}

// Helper: create VGA entry (character + color)
static inline uint16_t make_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

// Helper: update hardware cursor position
static void update_hardware_cursor(void) {
    if (!hal) return;  // HAL not initialized yet

    uint16_t pos = vga_state.cursor_y * VGA_WIDTH + vga_state.cursor_x;

    // Send low byte
    hal->io_outb(VGA_CTRL_REG, 0x0F);
    hal->io_outb(VGA_DATA_REG, (uint8_t)(pos & 0xFF));

    // Send high byte
    hal->io_outb(VGA_CTRL_REG, 0x0E);
    hal->io_outb(VGA_DATA_REG, (uint8_t)((pos >> 8) & 0xFF));
}

// Initialize VGA text mode driver
static int vga_text_init(void) {
    if (vga_state.initialized) {
        return 0;  // Already initialized
    }

    // Map VGA buffer (will use HAL when available)
    vga_state.buffer = (volatile uint16_t*)VGA_MEMORY;
    vga_state.cursor_x = 0;
    vga_state.cursor_y = 0;
    vga_state.current_color = make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_state.initialized = true;

    // Clear screen
    vga_text_clear();

    // Enable hardware cursor
    vga_text_cursor_enable(true);

    return 0;
}

// Shutdown VGA driver
static void vga_text_shutdown(void) {
    vga_text_clear();
    vga_state.initialized = false;
}

// Clear the entire screen
static void vga_text_clear(void) {
    uint16_t blank = make_entry(' ', vga_state.current_color);

    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            size_t index = y * VGA_WIDTH + x;
            vga_state.buffer[index] = blank;
        }
    }

    vga_state.cursor_x = 0;
    vga_state.cursor_y = 0;
    update_hardware_cursor();
}

// Scroll screen up by one line
static void vga_text_scroll(void) {
    // Move all lines up
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            size_t src = (y + 1) * VGA_WIDTH + x;
            size_t dst = y * VGA_WIDTH + x;
            vga_state.buffer[dst] = vga_state.buffer[src];
        }
    }

    // Clear bottom line
    uint16_t blank = make_entry(' ', vga_state.current_color);
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        size_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
        vga_state.buffer[index] = blank;
    }
}

// Write character at specific position (with bounds checking)
static void vga_text_putchar_at(char c, uint16_t x, uint16_t y) {
    if (x >= VGA_WIDTH || y >= VGA_HEIGHT) {
        return;  // Out of bounds - silently ignore
    }

    size_t index = y * VGA_WIDTH + x;
    vga_state.buffer[index] = make_entry(c, vga_state.current_color);
}

// Write character at cursor position
static void vga_text_putchar(char c) {
    // Handle special characters
    if (c == '\n') {
        // Newline
        vga_state.cursor_x = 0;
        vga_state.cursor_y++;
    } else if (c == '\r') {
        // Carriage return
        vga_state.cursor_x = 0;
    } else if (c == '\t') {
        // Tab (align to next 8-column boundary)
        vga_state.cursor_x = (vga_state.cursor_x + 8) & ~7;
    } else if (c == '\b') {
        // Backspace
        if (vga_state.cursor_x > 0) {
            vga_state.cursor_x--;
            vga_text_putchar_at(' ', vga_state.cursor_x, vga_state.cursor_y);
        }
    } else if (c >= ' ' && c <= '~') {
        // Printable ASCII
        vga_text_putchar_at(c, vga_state.cursor_x, vga_state.cursor_y);
        vga_state.cursor_x++;
    }
    // Ignore other control characters

    // Handle line wrap
    if (vga_state.cursor_x >= VGA_WIDTH) {
        vga_state.cursor_x = 0;
        vga_state.cursor_y++;
    }

    // Handle scroll
    if (vga_state.cursor_y >= VGA_HEIGHT) {
        vga_text_scroll();
        vga_state.cursor_y = VGA_HEIGHT - 1;
    }

    update_hardware_cursor();
}

// Write string at cursor position
static void vga_text_write(const char* str, size_t len) {
    for (size_t i = 0; i < len; i++) {
        vga_text_putchar(str[i]);
    }
}

// Write string at specific position
static void vga_text_write_at(const char* str, size_t len, uint16_t x, uint16_t y) {
    for (size_t i = 0; i < len && (x + i) < VGA_WIDTH && y < VGA_HEIGHT; i++) {
        vga_text_putchar_at(str[i], x + i, y);
    }
}

// Set foreground and background color
static void vga_text_set_color(enum vga_color fg, enum vga_color bg) {
    vga_state.current_color = make_color(fg, bg);
}

// Move cursor to specific position
static void vga_text_move_cursor(uint16_t x, uint16_t y) {
    if (x < VGA_WIDTH && y < VGA_HEIGHT) {
        vga_state.cursor_x = x;
        vga_state.cursor_y = y;
        update_hardware_cursor();
    }
}

// Get current cursor position
static void vga_text_get_cursor(uint16_t* x, uint16_t* y) {
    if (x) *x = vga_state.cursor_x;
    if (y) *y = vga_state.cursor_y;
}

// Enable or disable hardware cursor
static void vga_text_cursor_enable(bool enable) {
    if (!hal) return;

    if (enable) {
        // Enable cursor (scan lines 0-15)
        hal->io_outb(VGA_CTRL_REG, 0x0A);
        hal->io_outb(VGA_DATA_REG, 0x00);
        hal->io_outb(VGA_CTRL_REG, 0x0B);
        hal->io_outb(VGA_DATA_REG, 0x0F);
    } else {
        // Disable cursor
        hal->io_outb(VGA_CTRL_REG, 0x0A);
        hal->io_outb(VGA_DATA_REG, 0x20);
    }
}

// VGA text mode operations table
static struct vga_ops vga_text_ops = {
    .init = vga_text_init,
    .shutdown = vga_text_shutdown,
    .clear = vga_text_clear,
    .putchar = vga_text_putchar,
    .putchar_at = vga_text_putchar_at,
    .write = vga_text_write,
    .write_at = vga_text_write_at,
    .set_color = vga_text_set_color,
    .move_cursor = vga_text_move_cursor,
    .get_cursor = vga_text_get_cursor,
    .scroll = vga_text_scroll,
    .cursor_enable = vga_text_cursor_enable,
};

// Get VGA text mode driver
struct vga_ops* vga_text_get_driver(void) {
    return &vga_text_ops;
}
