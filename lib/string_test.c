/**
 * Unit tests for kernel string library
 *
 * These tests verify the safe string functions in lib/string.c
 */

#include <kernel/ktest.h>
#include <kernel/types.h>

// Forward declarations from lib/string.c
extern size_t strlcpy(char *dst, const char *src, size_t size);
extern size_t strlcat(char *dst, const char *src, size_t size);
extern size_t strlen(const char *s);
extern void *memset(void *s, int c, size_t n);
extern void *memcpy(void *dest, const void *src, size_t n);

// Test: strlen basic functionality
static int test_strlen_basic(void) {
    KTEST_ASSERT_EQ(strlen(""), 0, "empty string length");
    KTEST_ASSERT_EQ(strlen("a"), 1, "single char");
    KTEST_ASSERT_EQ(strlen("hello"), 5, "5 chars");
    KTEST_ASSERT_EQ(strlen("hello world"), 11, "11 chars with space");
    return KTEST_PASS;
}

// Test: strlcpy basic copy
static int test_strlcpy_basic(void) {
    char dst[10];
    size_t ret;

    // Copy short string
    ret = strlcpy(dst, "hello", sizeof(dst));
    KTEST_ASSERT_EQ(ret, 5, "strlcpy return value");
    KTEST_ASSERT_EQ(strlen(dst), 5, "copied string length");
    KTEST_ASSERT_EQ(dst[0], 'h', "first char");
    KTEST_ASSERT_EQ(dst[4], 'o', "last char");
    KTEST_ASSERT_EQ(dst[5], '\0', "null terminator");

    return KTEST_PASS;
}

// Test: strlcpy truncation
static int test_strlcpy_truncate(void) {
    char dst[5];  // Only room for 4 chars + null
    size_t ret;

    // Try to copy longer string
    ret = strlcpy(dst, "hello world", sizeof(dst));
    KTEST_ASSERT_EQ(ret, 11, "strlcpy returns source length");
    KTEST_ASSERT_EQ(strlen(dst), 4, "truncated to 4 chars");
    KTEST_ASSERT_EQ(dst[4], '\0', "null terminator at boundary");

    return KTEST_PASS;
}

// Test: strlcat basic concatenation
static int test_strlcat_basic(void) {
    char dst[20] = "hello";
    size_t ret;

    ret = strlcat(dst, " world", sizeof(dst));
    KTEST_ASSERT_EQ(ret, 11, "strlcat return value");
    KTEST_ASSERT_EQ(strlen(dst), 11, "concatenated length");

    return KTEST_PASS;
}

// Test: strlcat truncation
static int test_strlcat_truncate(void) {
    char dst[10] = "hello";  // 5 chars, room for 4 more

    strlcat(dst, " world", sizeof(dst));
    // strlcat should truncate to fit
    KTEST_ASSERT_EQ(strlen(dst), 9, "truncated concatenation");
    KTEST_ASSERT_EQ(dst[9], '\0', "null terminator");

    return KTEST_PASS;
}

// Test: memset functionality
static int test_memset_basic(void) {
    char buf[10];

    memset(buf, 0, sizeof(buf));
    for (size_t i = 0; i < sizeof(buf); i++) {
        KTEST_ASSERT_EQ(buf[i], 0, "memset to zero");
    }

    memset(buf, 'A', sizeof(buf));
    for (size_t i = 0; i < sizeof(buf); i++) {
        KTEST_ASSERT_EQ(buf[i], 'A', "memset to 'A'");
    }

    return KTEST_PASS;
}

// Test: memcpy basic functionality
static int test_memcpy_basic(void) {
    char src[] = "hello world";
    char dst[20];

    memset(dst, 0, sizeof(dst));
    memcpy(dst, src, sizeof(src));

    KTEST_ASSERT_EQ(strlen(dst), strlen(src), "memcpy copied correct length");
    KTEST_ASSERT_EQ(dst[0], 'h', "first char");
    KTEST_ASSERT_EQ(dst[10], 'd', "last char");

    return KTEST_PASS;
}

// Register all tests
KTEST_DEFINE("string", strlen_basic, test_strlen_basic);
KTEST_DEFINE("string", strlcpy_basic, test_strlcpy_basic);
KTEST_DEFINE("string", strlcpy_truncate, test_strlcpy_truncate);
KTEST_DEFINE("string", strlcat_basic, test_strlcat_basic);
KTEST_DEFINE("string", strlcat_truncate, test_strlcat_truncate);
KTEST_DEFINE("string", memset_basic, test_memset_basic);
KTEST_DEFINE("string", memcpy_basic, test_memcpy_basic);
