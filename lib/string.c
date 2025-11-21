// Safe String Library for Kernel
// NO unsafe functions like strcpy, strcat, sprintf!

#include <stddef.h>
#include <stdint.h>

// Get string length
size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

// Safe string copy (always null-terminates)
size_t strlcpy(char* dst, const char* src, size_t size) {
    size_t i;

    if (size == 0) {
        return 0;
    }

    for (i = 0; i < size - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }

    dst[i] = '\0';
    return i;
}

// Safe string concatenate (always null-terminates)
size_t strlcat(char* dst, const char* src, size_t size) {
    size_t dst_len = strlen(dst);
    size_t src_len = strlen(src);

    if (dst_len >= size) {
        return dst_len + src_len;
    }

    size_t copy_len = size - dst_len - 1;
    if (src_len < copy_len) {
        copy_len = src_len;
    }

    for (size_t i = 0; i < copy_len; i++) {
        dst[dst_len + i] = src[i];
    }

    dst[dst_len + copy_len] = '\0';
    return dst_len + src_len;
}

// Compare strings
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

// Compare strings (up to n characters)
int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }

    if (n == 0) {
        return 0;
    }

    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

// Memory copy
void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }

    return dest;
}

// Memory move (handles overlapping regions)
void* memmove(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    if (d < s) {
        // Copy forward
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else {
        // Copy backward
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }

    return dest;
}

// Memory set
void* memset(void* ptr, int value, size_t n) {
    uint8_t* p = (uint8_t*)ptr;
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)value;
    }
    return ptr;
}

// Memory compare
int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t* p1 = (const uint8_t*)s1;
    const uint8_t* p2 = (const uint8_t*)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }

    return 0;
}
