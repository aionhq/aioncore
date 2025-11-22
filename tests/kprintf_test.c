/**
 * Host-side unit tests for kprintf number formatting
 *
 * Tests the utoa/itoa functions that kprintf uses internally.
 * This isolates the bug: is utoa(4096) broken, or is something else wrong?
 */

#include "host_test.h"
#include <string.h>

// Forward declare the functions from vga.c
// We'll extract and test them directly
static int utoa(uint32_t value, char* buf, int base);
static int itoa(int32_t value, char* buf, int base);

/**
 * utoa - Convert unsigned integer to string
 * Copied from drivers/vga/vga.c for testing
 */
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

/**
 * itoa - Convert signed integer to string
 * Copied from drivers/vga/vga.c for testing
 */
static int itoa(int32_t value, char* buf, int base) {
    if (base == 10 && value < 0) {
        *buf++ = '-';
        return 1 + utoa((uint32_t)(-value), buf, base);
    }
    return utoa((uint32_t)value, buf, base);
}

// Test 1: utoa with 4096 (the problematic value!)
TEST(utoa_4096_decimal) {
    char buf[32];
    int len = utoa(4096, buf, 10);

    printf("\n    utoa(4096, buf, 10) = \"%s\" (len=%d)\n    ", buf, len);

    TEST_ASSERT_EQ(len, 4, "length should be 4");
    TEST_ASSERT(strcmp(buf, "4096") == 0, "should produce \"4096\"");

    return 1;
}

// Test 2: utoa with 40 (in case there's some weird corruption)
TEST(utoa_40_decimal) {
    char buf[32];
    int len = utoa(40, buf, 10);

    printf("\n    utoa(40, buf, 10) = \"%s\" (len=%d)\n    ", buf, len);

    TEST_ASSERT_EQ(len, 2, "length should be 2");
    TEST_ASSERT(strcmp(buf, "40") == 0, "should produce \"40\"");

    return 1;
}

// Test 3: utoa with various PAGE_SIZE related values
TEST(utoa_page_sizes) {
    char buf[32];

    // 4096 in hex (0x1000)
    int len_hex = utoa(0x1000, buf, 16);
    printf("\n    utoa(0x1000, buf, 16) = \"%s\" (len=%d)\n    ", buf, len_hex);
    TEST_ASSERT(strcmp(buf, "1000") == 0, "hex should be \"1000\"");

    // 4096 in decimal
    int len_dec = utoa(0x1000, buf, 10);
    printf("    utoa(0x1000, buf, 10) = \"%s\" (len=%d)\n    ", buf, len_dec);
    TEST_ASSERT(strcmp(buf, "4096") == 0, "decimal should be \"4096\"");

    return 1;
}

// Test 4: utoa with small numbers
TEST(utoa_small_numbers) {
    char buf[32];

    utoa(0, buf, 10);
    TEST_ASSERT(strcmp(buf, "0") == 0, "0 should work");

    utoa(1, buf, 10);
    TEST_ASSERT(strcmp(buf, "1") == 0, "1 should work");

    utoa(10, buf, 10);
    TEST_ASSERT(strcmp(buf, "10") == 0, "10 should work");

    utoa(100, buf, 10);
    TEST_ASSERT(strcmp(buf, "100") == 0, "100 should work");

    utoa(1000, buf, 10);
    TEST_ASSERT(strcmp(buf, "1000") == 0, "1000 should work");

    return 1;
}

// Test 5: utoa with large numbers
TEST(utoa_large_numbers) {
    char buf[32];

    utoa(65536, buf, 10);
    TEST_ASSERT(strcmp(buf, "65536") == 0, "65536 should work");

    utoa(1048576, buf, 10);
    TEST_ASSERT(strcmp(buf, "1048576") == 0, "1048576 should work");

    utoa(0xFFFFFFFF, buf, 10);
    TEST_ASSERT(strcmp(buf, "4294967295") == 0, "max uint32 should work");

    return 1;
}

// Test 6: Edge case - what if the cast to unsigned int is the problem?
TEST(cast_to_unsigned_int) {
    char buf[32];

    #define PAGE_SIZE 4096

    // Test exactly as used in mmu.c
    unsigned int val = (unsigned int)PAGE_SIZE;
    int len = utoa(val, buf, 10);

    printf("\n    (unsigned int)PAGE_SIZE = %u\n    ", val);
    printf("utoa(%u, buf, 10) = \"%s\" (len=%d)\n    ", val, buf, len);

    TEST_ASSERT_EQ(val, 4096, "cast should preserve value");
    TEST_ASSERT(strcmp(buf, "4096") == 0, "should produce \"4096\"");

    return 1;
}
