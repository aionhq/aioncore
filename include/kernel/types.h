#ifndef KERNEL_TYPES_H
#define KERNEL_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Null pointer
#ifndef NULL
#define NULL ((void*)0)
#endif

// Boolean type (if not using C99 stdbool.h)
#ifndef __cplusplus
#ifndef bool
typedef _Bool bool;
#define true 1
#define false 0
#endif
#endif

// Atomic operations wrapper
typedef struct {
    volatile uint32_t value;
} atomic_t;

// Initialize atomic variable
static inline void atomic_init(atomic_t* atom, uint32_t value) {
    atom->value = value;
}

// Atomic read
static inline uint32_t atomic_read(const atomic_t* atom) {
    return atom->value;
}

// Atomic write
static inline void atomic_write(atomic_t* atom, uint32_t value) {
    atom->value = value;
}

// Atomic increment (returns old value)
static inline uint32_t atomic_inc(atomic_t* atom) {
    return __sync_fetch_and_add(&atom->value, 1);
}

// Atomic decrement (returns old value)
static inline uint32_t atomic_dec(atomic_t* atom) {
    return __sync_fetch_and_sub(&atom->value, 1);
}

// Atomic compare-and-swap
static inline bool atomic_cas(atomic_t* atom, uint32_t expected, uint32_t desired) {
    return __sync_bool_compare_and_swap(&atom->value, expected, desired);
}

// Atomic decrement and test for zero
static inline bool atomic_dec_and_test(atomic_t* atom) {
    return __sync_sub_and_fetch(&atom->value, 1) == 0;
}

// List node for intrusive linked lists
struct list_head {
    struct list_head* next;
    struct list_head* prev;
};

// Initialize list head
static inline void list_init(struct list_head* list) {
    list->next = list;
    list->prev = list;
}

// Check if list is empty
static inline bool list_empty(const struct list_head* list) {
    return list->next == list;
}

// Compiler barriers
#define barrier() __asm__ __volatile__("": : :"memory")

// Memory barriers (for SMP)
#define mb()  __sync_synchronize()  // Full memory barrier
#define rmb() barrier()             // Read memory barrier
#define wmb() barrier()             // Write memory barrier

// Alignment macros
#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))
#define ALIGN_UP(x, a)   (((x) + (a) - 1) & ~((a) - 1))
#define IS_ALIGNED(x, a) (((x) & ((a) - 1)) == 0)

// Container of macro (get struct from member pointer)
#define container_of(ptr, type, member) ({                      \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);       \
    (type *)( (char *)__mptr - offsetof(type,member) );})

// Min/max macros
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Array size
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Likely/unlikely branch hints
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

// Error codes (compatible with Linux errno)
#define ENOMEM      12  // Out of memory
#define EINVAL      22  // Invalid argument
#define ENODEV      19  // No such device
#define EBUSY       16  // Device or resource busy
#define EIO          5  // I/O error
#define EPERM        1  // Operation not permitted

#endif // KERNEL_TYPES_H
