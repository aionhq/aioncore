#ifndef KERNEL_PMM_H
#define KERNEL_PMM_H

#include <stdint.h>
#include <stddef.h>
#include <kernel/types.h>

/**
 * Physical Memory Manager (PMM)
 *
 * Manages physical memory frames (4KB pages) using a bitmap allocator
 * with per-CPU free lists for O(1) allocation.
 *
 * Design:
 * - Bitmap tracks allocated/free frames
 * - Per-CPU free lists for O(1) allocation (future optimization)
 * - Frame ownership tracking for unit accounting
 * - Reserved regions for kernel, per-CPU data, MMIO
 *
 * RT Constraints:
 * - pmm_alloc_page(): O(1) with free list
 * - pmm_free_page(): O(1)
 * - No unbounded loops in critical paths
 */

// Multiboot magic number
#define MULTIBOOT_MAGIC 0x2BADB002

// Multiboot flags
#define MULTIBOOT_FLAG_MEM     0x001
#define MULTIBOOT_FLAG_CMDLINE 0x004
#define MULTIBOOT_FLAG_MODS    0x008
#define MULTIBOOT_FLAG_MMAP    0x040

// Multiboot memory map entry
struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed));

// Multiboot memory map types
#define MULTIBOOT_MEMORY_AVAILABLE        1
#define MULTIBOOT_MEMORY_RESERVED         2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS              4
#define MULTIBOOT_MEMORY_BADRAM           5

// Multiboot info structure
struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
#ifdef HOST_TEST
    // Use uintptr_t for host tests to avoid pointer truncation on 64-bit systems
    uintptr_t mmap_addr;
#else
    uint32_t mmap_addr;
#endif
} __attribute__((packed));

// PMM statistics
struct pmm_stats {
    size_t total_frames;
    size_t free_frames;
    size_t reserved_frames;
    size_t kernel_frames;
};

/**
 * Initialize the physical memory manager
 *
 * Parses multiboot memory map, builds bitmap allocator, and reserves
 * kernel regions.
 *
 * @param multiboot_magic Multiboot magic number (must be 0x2BADB002)
 * @param multiboot_info Pointer to multiboot info structure
 */
void pmm_init(uint32_t multiboot_magic, struct multiboot_info *mbi);

// Whether the PMM has been initialized
bool pmm_is_initialized(void);

/**
 * Allocate a physical frame (4KB page)
 *
 * Allocates from per-CPU free list for O(1) performance.
 *
 * @return Physical address of allocated frame, or 0 on failure
 *
 * RT Constraint: O(1) operation, <100 cycles
 */
phys_addr_t pmm_alloc_page(void);

/**
 * Free a physical frame
 *
 * Returns frame to free list for future allocation.
 *
 * @param page Physical address of frame to free (must be 4KB aligned)
 *
 * RT Constraint: O(1) operation, <50 cycles
 */
void pmm_free_page(phys_addr_t page);

/**
 * Reserve a physical memory region
 *
 * Marks frames in the region as allocated. Used for kernel code/data,
 * MMIO regions, and other reserved areas.
 *
 * @param start Physical address of region start
 * @param size Size of region in bytes
 */
void pmm_reserve_region(phys_addr_t start, size_t size);

/**
 * Get PMM statistics
 *
 * Returns information about memory usage.
 *
 * @param stats Pointer to stats structure to fill
 */
void pmm_get_stats(struct pmm_stats *stats);

#endif // KERNEL_PMM_H
