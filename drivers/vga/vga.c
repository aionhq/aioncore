// VGA Subsystem - Driver Management and kprintf

#include <drivers/vga.h>
#include <kernel/types.h>
#include <kernel/console.h>
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

// Helper: convert 32-bit integer to string
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

    // Save length before null terminator
    int len = ptr - buf;
    *ptr-- = '\0';

    // Reverse string
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }

    return len;
}

// Helper: convert 32-bit unsigned integer to string
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

    // Save length before null terminator
    int len = ptr - buf;
    *ptr-- = '\0';

    // Reverse string
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }

    return len;
}

// Helper: convert 64-bit unsigned integer to string
static int utoa64(uint64_t value, char* buf, int base) {
    char* ptr = buf;
    char* ptr1 = buf;
    char tmp_char;
    uint64_t tmp_value;

    if (base < 2 || base > 36) {
        *buf = '\0';
        return 0;
    }

    if (value == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return 1;
    }

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"
                 [tmp_value - value * base];
    } while (value);

    // Save length before null terminator
    int len = ptr - buf;
    *ptr-- = '\0';

    // Reverse string
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }

    return len;
}

// Helper: convert 64-bit signed integer to string
static int itoa64(int64_t value, char* buf, int base) {
    if (value < 0) {
        int len = utoa64((uint64_t)(-value), buf + 1, base);
        buf[0] = '-';
        buf[len + 1] = '\0';
        return len + 1;
    }
    return utoa64((uint64_t)value, buf, base);
}

// Minimal printf implementation
// Supports:
//   - %d, %i           (signed 32-bit)
//   - %u               (unsigned 32-bit)
//   - %x, %X           (hex)
//   - %p               (pointer, 32-bit)
//   - %s, %c, %%       (string, char, literal %)
//   - Optional width / zero padding (e.g., %08x)
//   - Optional 'l' length modifier (treated as 32-bit on this 32-bit kernel)
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

            // Optional length modifier: 'l' or 'll'
            // On this 32-bit kernel, long is 32-bit and long long is 64-bit.
            bool long_mod = false;
            bool long_long_mod = false;
            if (*format == 'l') {
                if (*(format + 1) == 'l') {
                    long_long_mod = true;
                    format += 2;
                } else {
                    long_mod = true;
                    format++;
                }
            }

            switch (*format) {
                case 'd':  // Signed decimal
                case 'i': {
                    int len = 0;
                    if (long_long_mod) {
                        long long v = va_arg(args, long long);
                        len = itoa64((int64_t)v, buf, 10);
                    } else if (long_mod) {
                        long v = va_arg(args, long);
                        int32_t val = (int32_t)v;
                        len = itoa(val, buf, 10);
                    } else {
                        int v = va_arg(args, int);
                        int32_t val = (int32_t)v;
                        len = itoa(val, buf, 10);
                    }
                    // Apply padding if specified
                    if (zero_pad && width > len) {
                        for (int i = len; i < width; i++) {
                            console_putchar('0');
                            written++;
                        }
                    }
                    console_write(buf, len);
                    written += len;
                    break;
                }

                case 'u': {  // Unsigned decimal
                    int len = 0;
                    if (long_long_mod) {
                        unsigned long long v = va_arg(args, unsigned long long);
                        len = utoa64((uint64_t)v, buf, 10);
                    } else if (long_mod) {
                        unsigned long v = va_arg(args, unsigned long);
                        uint32_t val = (uint32_t)v;
                        len = utoa(val, buf, 10);
                    } else {
                        unsigned int v = va_arg(args, unsigned int);
                        uint32_t val = (uint32_t)v;
                        len = utoa(val, buf, 10);
                    }
                    // Apply padding if specified
                    if (zero_pad && width > len) {
                        for (int i = len; i < width; i++) {
                            console_putchar('0');
                            written++;
                        }
                    }
                    console_write(buf, len);
                    written += len;
                    break;
                }

                case 'x':  // Hexadecimal (lowercase)
                case 'X': {  // Hexadecimal (uppercase)
                    int len = 0;
                    if (long_long_mod) {
                        unsigned long long v = va_arg(args, unsigned long long);
                        len = utoa64((uint64_t)v, buf, 16);
                    } else if (long_mod) {
                        unsigned long v = va_arg(args, unsigned long);
                        uint32_t val = (uint32_t)v;
                        len = utoa(val, buf, 16);
                    } else {
                        unsigned int v = va_arg(args, unsigned int);
                        uint32_t val = (uint32_t)v;
                        len = utoa(val, buf, 16);
                    }
                    // Apply padding if specified
                    if (zero_pad && width > len) {
                        for (int i = len; i < width; i++) {
                            console_putchar('0');
                            written++;
                        }
                    }
                    console_write(buf, len);
                    written += len;
                    break;
                }

                case 'p': {  // Pointer
                    console_write("0x", 2);
                    uintptr_t ptr = (uintptr_t)va_arg(args, void*);
                    uint32_t val = (uint32_t)ptr;
                    int len = utoa(val, buf, 16);
                    // Pad to 8 digits
                    for (int i = len; i < 8; i++) {
                        console_putchar('0');
                        written++;
                    }
                    console_write(buf, len);
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
                    console_write(str, len);
                    written += len;
                    break;
                }

                case 'c': {  // Character
                    char c = (char)va_arg(args, int);
                    console_putchar(c);
                    written++;
                    break;
                }

                case '%': {  // Literal %
                    console_putchar('%');
                    written++;
                    break;
                }

                default:
                    // Unknown format, print as-is
                    console_putchar('%');
                    console_putchar(*format);
                    written += 2;
                    break;
            }

            format++;
        } else {
            console_putchar(*format);
            written++;
            format++;
        }
    }

    va_end(args);
    return written;
}
