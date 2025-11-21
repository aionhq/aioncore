// VGA Subsystem - Driver Management and kprintf

#include <drivers/vga.h>
#include <kernel/types.h>
#include <stdarg.h>

// External driver
extern struct vga_ops* vga_text_get_driver(void);

// Global VGA driver pointer
struct vga_ops* vga = NULL;

// Initialize VGA subsystem (selects best driver)
int vga_subsystem_init(void) {
    // For now, always use VGA text mode
    vga = vga_text_get_driver();

    if (!vga) {
        return -ENODEV;
    }

    // Initialize the driver
    return vga->init();
}

// Helper functions

void vga_clear(void) {
    if (vga && vga->clear) {
        vga->clear();
    }
}

void vga_putchar(char c) {
    if (vga && vga->putchar) {
        vga->putchar(c);
    }
}

void vga_write(const char* str) {
    if (!str || !vga || !vga->write) {
        return;
    }

    size_t len = 0;
    while (str[len]) {
        len++;
    }

    vga->write(str, len);
}

void vga_set_color(enum vga_color fg, enum vga_color bg) {
    if (vga && vga->set_color) {
        vga->set_color(fg, bg);
    }
}

// ========== Minimal kprintf Implementation ==========

// Helper: convert integer to string
static int itoa(int32_t value, char* buf, int base) {
    char* ptr = buf;
    char* ptr1 = buf;
    char tmp_char;
    int32_t tmp_value;

    if (base < 2 || base > 36) {
        *buf = '\0';
        return 0;
    }

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"[35 + (tmp_value - value * base)];
    } while (value);

    // Handle negative numbers
    if (tmp_value < 0 && base == 10) {
        *ptr++ = '-';
    }

    *ptr-- = '\0';

    // Reverse string
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }

    return ptr1 - buf;
}

// Helper: convert unsigned integer to string
static int utoa(uint32_t value, char* buf, int base) {
    char* ptr = buf;
    char* ptr1 = buf;
    char tmp_char;
    uint32_t tmp_value;

    if (base < 2 || base > 36) {
        *buf = '\0';
        return 0;
    }

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"[tmp_value - value * base];
    } while (value);

    *ptr-- = '\0';

    // Reverse string
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }

    return ptr1 - buf;
}

// Minimal printf implementation
// Supports: %d, %u, %x, %08x, %s, %c, %%
int kprintf(const char* format, ...) {
    if (!vga) {
        return -1;
    }

    va_list args;
    va_start(args, format);

    int written = 0;
    char buf[32];

    while (*format) {
        if (*format == '%') {
            format++;

            // Parse flags and width
            bool zero_pad = false;
            int width = 0;

            // Check for '0' flag (zero padding)
            if (*format == '0') {
                zero_pad = true;
                format++;
            }

            // Parse width (e.g., "8" in "%08x")
            while (*format >= '0' && *format <= '9') {
                width = width * 10 + (*format - '0');
                format++;
            }

            switch (*format) {
                case 'd':  // Signed decimal
                case 'i': {
                    int32_t val = va_arg(args, int32_t);
                    int len = itoa(val, buf, 10);
                    // Apply padding if specified
                    if (zero_pad && width > len) {
                        for (int i = len; i < width; i++) {
                            vga->putchar('0');
                            written++;
                        }
                    }
                    vga->write(buf, len);
                    written += len;
                    break;
                }

                case 'u': {  // Unsigned decimal
                    uint32_t val = va_arg(args, uint32_t);
                    int len = utoa(val, buf, 10);
                    // Apply padding if specified
                    if (zero_pad && width > len) {
                        for (int i = len; i < width; i++) {
                            vga->putchar('0');
                            written++;
                        }
                    }
                    vga->write(buf, len);
                    written += len;
                    break;
                }

                case 'x':  // Hexadecimal (lowercase)
                case 'X': {  // Hexadecimal (uppercase)
                    uint32_t val = va_arg(args, uint32_t);
                    int len = utoa(val, buf, 16);
                    // Apply padding if specified
                    if (zero_pad && width > len) {
                        for (int i = len; i < width; i++) {
                            vga->putchar('0');
                            written++;
                        }
                    }
                    vga->write(buf, len);
                    written += len;
                    break;
                }

                case 'p': {  // Pointer
                    vga->write("0x", 2);
                    uint32_t val = va_arg(args, uint32_t);
                    int len = utoa(val, buf, 16);
                    // Pad to 8 digits
                    for (int i = len; i < 8; i++) {
                        vga->putchar('0');
                        written++;
                    }
                    vga->write(buf, len);
                    written += len + 2;
                    break;
                }

                case 's': {  // String
                    const char* str = va_arg(args, const char*);
                    if (!str) {
                        str = "(null)";
                    }
                    size_t len = 0;
                    while (str[len]) len++;
                    vga->write(str, len);
                    written += len;
                    break;
                }

                case 'c': {  // Character
                    char c = (char)va_arg(args, int);
                    vga->putchar(c);
                    written++;
                    break;
                }

                case '%': {  // Literal %
                    vga->putchar('%');
                    written++;
                    break;
                }

                default:
                    // Unknown format, print as-is
                    vga->putchar('%');
                    vga->putchar(*format);
                    written += 2;
                    break;
            }

            format++;
        } else {
            vga->putchar(*format);
            written++;
            format++;
        }
    }

    va_end(args);
    return written;
}
