#ifndef DRIVERS_VGA_H
#define DRIVERS_VGA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// VGA text mode constants
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

// VGA colors (4-bit color palette)
enum vga_color {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_YELLOW        = 14,
    VGA_COLOR_WHITE         = 15,
};

// VGA driver operations (abstraction layer)
// Different implementations can be swapped (VGA, framebuffer, serial)
struct vga_ops {
    // Initialize the driver
    int (*init)(void);

    // Cleanup and shutdown
    void (*shutdown)(void);

    // Clear the entire screen
    void (*clear)(void);

    // Write a single character at cursor position
    void (*putchar)(char c);

    // Write character at specific position
    void (*putchar_at)(char c, uint16_t x, uint16_t y);

    // Write string at cursor position
    void (*write)(const char* str, size_t len);

    // Write string at specific position
    void (*write_at)(const char* str, size_t len, uint16_t x, uint16_t y);

    // Set foreground and background color
    void (*set_color)(enum vga_color fg, enum vga_color bg);

    // Move hardware cursor
    void (*move_cursor)(uint16_t x, uint16_t y);

    // Get cursor position
    void (*get_cursor)(uint16_t* x, uint16_t* y);

    // Scroll the screen up by one line
    void (*scroll)(void);

    // Enable/disable hardware cursor
    void (*cursor_enable)(bool enable);
};

// Global VGA driver pointer
extern struct vga_ops* vga;

// Initialize VGA subsystem (selects best available driver)
int vga_subsystem_init(void);

// Helper functions (use the registered driver)
void vga_clear(void);
void vga_putchar(char c);
void vga_write(const char* str);
void vga_set_color(enum vga_color fg, enum vga_color bg);

// kprintf - kernel printf (minimal implementation)
int kprintf(const char* format, ...) __attribute__((format(printf, 1, 2)));

#endif // DRIVERS_VGA_H
