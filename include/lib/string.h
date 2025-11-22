#ifndef LIB_STRING_H
#define LIB_STRING_H

#include <stddef.h>

/**
 * Safe String and Memory Functions
 *
 * These are kernel-safe string functions that always null-terminate
 * and have bounded execution time.
 */

/**
 * Safe string copy (always null-terminates)
 *
 * @param dst   Destination buffer
 * @param src   Source string
 * @param size  Size of destination buffer
 * @return      Length of src (for truncation detection)
 */
size_t strlcpy(char* dst, const char* src, size_t size);

/**
 * Safe string concatenate (always null-terminates)
 *
 * @param dst   Destination buffer
 * @param src   Source string
 * @param size  Size of destination buffer
 * @return      Total length attempted (for truncation detection)
 */
size_t strlcat(char* dst, const char* src, size_t size);

/**
 * String length
 *
 * @param s  String to measure
 * @return   Length of string (not including null terminator)
 */
size_t strlen(const char* s);

/**
 * Memory copy
 *
 * @param dest  Destination
 * @param src   Source
 * @param n     Number of bytes to copy
 * @return      dest pointer
 */
void* memcpy(void* dest, const void* src, size_t n);

/**
 * Memory set
 *
 * @param ptr    Memory to set
 * @param value  Value to set (converted to unsigned char)
 * @param n      Number of bytes
 * @return       ptr pointer
 */
void* memset(void* ptr, int value, size_t n);

/**
 * Memory compare
 *
 * @param s1  First memory region
 * @param s2  Second memory region
 * @param n   Number of bytes to compare
 * @return    0 if equal, <0 if s1 < s2, >0 if s1 > s2
 */
int memcmp(const void* s1, const void* s2, size_t n);

#endif // LIB_STRING_H
