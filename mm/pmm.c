/**
 * Physical Memory Manager (PMM)
 *
 * Bitmap-based frame allocator with per-CPU free lists for O(1) allocation.
 * Tracks allocated/free 4KB frames and reserved regions.
 *
 * RT Constraints:
 * - pmm_alloc_page(): O(1) with free list, <100 cycles
 * - pmm_free_page(): O(1), <50 cycles
 * - No unbounded loops in critical paths
 */

#ifdef HOST_TEST
    // Host-side testing: Mock kernel dependencies
    #include <stdio.h>
    #include <stdlib.h>
    #include <stdint.h>
    #include <stddef.h>
    #include <stdbool.h>
    #include <assert.h>
    #include <string.h>

    // Mock kprintf as printf
    #define kprintf printf

    // Mock kassert macros
    #define kassert(cond) assert(cond)
    #define kassert_aligned(addr, align) assert(((uintptr_t)(addr) % (align)) == 0)
    #define kassert_not_null(ptr) assert((ptr) != NULL)

    // Mock invariant macros (no-op on host)
    #define PRECONDITION(msg) ((void)msg)
    #define POSTCONDITION(msg) ((void)msg)
    #define INVARIANT(msg) ((void)msg)

    // Mock HAL (not used in PMM yet, but for future)
    struct hal_ops;
    struct hal_ops* hal = NULL;

    // Include kernel headers for types only
    #include "../include/kernel/pmm.h"
    #include "../include/kernel/types.h"
#else
    // Real kernel includes
    #include <kernel/pmm.h>
    #include <kernel/types.h>
    #include <kernel/hal.h>
    #include <kernel/assert.h>
    #include <drivers/vga.h>
    #include <stdint.h>
    #include <stddef.h>
    #include <stdbool.h>
#endif

// Frame size (4KB pages)
#define FRAME_SIZE 4096

// Maximum memory we can manage (4GB for 32-bit)
#define MAX_MEMORY (4ULL * 1024 * 1024 * 1024)
#define MAX_FRAMES (MAX_MEMORY / FRAME_SIZE)

// Bitmap size (1 bit per frame)
#define BITMAP_SIZE (MAX_FRAMES / 8)

// Free list size per CPU (number of pre-allocated frames)
#define FREE_LIST_SIZE 64

// PMM state
static struct {
    uint8_t *bitmap;           // Frame allocation bitmap
    phys_addr_t bitmap_start;  // Physical address of bitmap
    size_t total_frames;       // Total number of frames
    size_t free_frames;        // Number of free frames
    size_t reserved_frames;    // Number of reserved frames
    bool initialized;          // PMM initialized flag
} pmm_state;

// Static bitmap storage (placed in .bss)
static uint8_t frame_bitmap[BITMAP_SIZE];

/**
 * Test if a bit is set in the bitmap
 */
static inline bool bitmap_test(size_t frame) {
    size_t byte = frame / 8;
    size_t bit = frame % 8;
    return (frame_bitmap[byte] & (1 << bit)) != 0;
}

/**
 * Set a bit in the bitmap (mark frame as allocated)
 */
static inline void bitmap_set(size_t frame) {
    PRECONDITION("frame must be within valid range");
    kassert(frame < MAX_FRAMES);

    size_t byte = frame / 8;
    size_t bit = frame % 8;
    frame_bitmap[byte] |= (1 << bit);

    POSTCONDITION("bit should be set");
    kassert(bitmap_test(frame));
}

/**
 * Clear a bit in the bitmap (mark frame as free)
 */
static inline void bitmap_clear(size_t frame) {
    PRECONDITION("frame must be within valid range");
    kassert(frame < MAX_FRAMES);

    size_t byte = frame / 8;
    size_t bit = frame % 8;
    frame_bitmap[byte] &= ~(1 << bit);

    POSTCONDITION("bit should be cleared");
    kassert(!bitmap_test(frame));
}

/**
 * Find the first free frame in the bitmap
 *
 * Returns frame number, or MAX_FRAMES if none found.
 *
 * Note: This is O(n) but only used during initialization
 * and as fallback. Normal allocation uses free lists.
 */
static size_t bitmap_find_free(void) {
    // Scan bitmap for first free frame
    for (size_t i = 0; i < BITMAP_SIZE; i++) {
        if (frame_bitmap[i] != 0xFF) {
            // Found a byte with free bits
            for (size_t bit = 0; bit < 8; bit++) {
                if ((frame_bitmap[i] & (1 << bit)) == 0) {
                    return i * 8 + bit;
                }
            }
        }
    }

    return MAX_FRAMES;
}

/**
 * Initialize the physical memory manager
 */
void pmm_init(uint32_t multiboot_magic, struct multiboot_info *mbi) {
    kprintf("[PMM] Initializing physical memory manager...\n");

    // Verify multiboot magic
    if (multiboot_magic != MULTIBOOT_MAGIC) {
        kprintf("[PMM] ERROR: Invalid multiboot magic: 0x%08x (expected 0x%08x)\n",
                (unsigned int)multiboot_magic, (unsigned int)MULTIBOOT_MAGIC);
        kprintf("[PMM] Continuing with fallback: assuming 128MB RAM\n");
        // Fallback: assume we have 128MB of RAM starting at 1MB
        goto fallback_init;
    }

    // Verify multiboot info pointer is valid
    if (mbi == NULL || (uintptr_t)mbi < 0x1000) {
        kprintf("[PMM] ERROR: Invalid multiboot info pointer: %p\n", mbi);
        kprintf("[PMM] Continuing with fallback: assuming 128MB RAM\n");
        goto fallback_init;
    }

    kprintf("[PMM] Multiboot flags: 0x%08x\n", (unsigned int)mbi->flags);

    // Check if we have memory map
    if ((mbi->flags & MULTIBOOT_FLAG_MMAP) == 0) {
        kprintf("[PMM] WARNING: No memory map from bootloader (bit 6 not set)\n");
        kprintf("[PMM] Continuing with fallback: assuming 128MB RAM\n");
        goto fallback_init;
    }

    kprintf("[PMM] Initializing physical memory manager...\n");

    // Initialize bitmap (mark all frames as allocated initially)
    for (size_t i = 0; i < BITMAP_SIZE; i++) {
        frame_bitmap[i] = 0xFF;
    }

    pmm_state.bitmap = frame_bitmap;
    pmm_state.total_frames = 0;
    pmm_state.free_frames = 0;
    pmm_state.reserved_frames = 0;

    // Parse multiboot memory map
    struct multiboot_mmap_entry *mmap = (struct multiboot_mmap_entry *)(uintptr_t)mbi->mmap_addr;
    struct multiboot_mmap_entry *mmap_end =
        (struct multiboot_mmap_entry *)((uintptr_t)mbi->mmap_addr + mbi->mmap_length);

    kprintf("[PMM] Parsing memory map:\n");

    while (mmap < mmap_end) {
        // Get region bounds
        uint64_t region_start = mmap->addr;
        uint64_t region_end = mmap->addr + mmap->len;
        uint32_t region_type = mmap->type;

        const char *type_str = "UNKNOWN";
        if (region_type == MULTIBOOT_MEMORY_AVAILABLE) {
            type_str = "AVAILABLE";
        } else if (region_type == MULTIBOOT_MEMORY_RESERVED) {
            type_str = "RESERVED";
        } else if (region_type == MULTIBOOT_MEMORY_ACPI_RECLAIMABLE) {
            type_str = "ACPI";
        } else if (region_type == MULTIBOOT_MEMORY_NVS) {
            type_str = "NVS";
        } else if (region_type == MULTIBOOT_MEMORY_BADRAM) {
            type_str = "BADRAM";
        }

        kprintf("[PMM]   0x%08x%08x - 0x%08x%08x: %s\n",
                (unsigned int)(region_start >> 32),
                (unsigned int)(region_start & 0xFFFFFFFF),
                (unsigned int)(region_end >> 32),
                (unsigned int)(region_end & 0xFFFFFFFF),
                type_str);

        // If available memory, mark frames as free
        if (region_type == MULTIBOOT_MEMORY_AVAILABLE) {
            // Align start up to frame boundary
            uint64_t frame_start = (region_start + FRAME_SIZE - 1) / FRAME_SIZE;
            uint64_t frame_end = region_end / FRAME_SIZE;

            // Mark frames as free
            for (uint64_t frame = frame_start; frame < frame_end; frame++) {
                if (frame < MAX_FRAMES) {
                    bitmap_clear((size_t)frame);
                    pmm_state.total_frames++;
                    pmm_state.free_frames++;
                }
            }
        }

        // Move to next entry (mmap->size is size of entry minus size field itself)
        mmap = (struct multiboot_mmap_entry *)((uintptr_t)mmap + mmap->size + sizeof(mmap->size));
    }

    goto reserve_regions;

fallback_init:
    // Fallback: assume 128MB of RAM starting at 0
    kprintf("\n");
    kprintf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    kprintf("!! WARNING: USING FALLBACK MEMORY MANAGER  !!\n");
    kprintf("!! Multiboot info invalid or missing       !!\n");
    kprintf("!! Assuming 128MB RAM - MAY BE INCORRECT   !!\n");
    kprintf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    kprintf("\n");

    // Initialize bitmap (mark all frames as allocated initially)
    for (size_t i = 0; i < BITMAP_SIZE; i++) {
        frame_bitmap[i] = 0xFF;
    }

    pmm_state.bitmap = frame_bitmap;
    pmm_state.total_frames = 0;
    pmm_state.free_frames = 0;
    pmm_state.reserved_frames = 0;

    // Mark first 128MB as available (128MB = 32768 frames)
    size_t fallback_frames = (128 * 1024 * 1024) / FRAME_SIZE;
    for (size_t i = 0; i < fallback_frames && i < MAX_FRAMES; i++) {
        bitmap_clear(i);
        pmm_state.total_frames++;
        pmm_state.free_frames++;
    }

reserve_regions:
    kprintf("[PMM] Reserving critical regions...\n");

    // Get kernel bounds from linker
#ifdef HOST_TEST
    // Mock kernel bounds for testing
    phys_addr_t kernel_start = 0x100000;  // 1MB
    phys_addr_t kernel_end = 0x200000;    // 2MB
#else
    extern char _kernel_start[];
    extern char _kernel_end[];
    phys_addr_t kernel_start = (phys_addr_t)(uintptr_t)_kernel_start;
    phys_addr_t kernel_end = (phys_addr_t)(uintptr_t)_kernel_end;
#endif

    kprintf("[PMM] Kernel at 0x%08x - 0x%08x\n",
            (unsigned int)kernel_start, (unsigned int)kernel_end);

    // Reserve only critical low-memory regions, not entire 1MB
    // This leaves more frames available for page tables

    // Reserve first page (NULL page) to catch null pointer dereferences
    pmm_reserve_region(0, FRAME_SIZE);

    // Reserve VGA text buffer: 0xb8000 - 0xc0000 (32 KB)
    pmm_reserve_region(0xb8000, 32 * 1024);

    // Reserve kernel image
    pmm_reserve_region(kernel_start, kernel_end - kernel_start);

    pmm_state.initialized = true;

    // Calculate memory in KB for accurate small values
    unsigned int total_kb = (unsigned int)((pmm_state.total_frames * FRAME_SIZE) / 1024);
    unsigned int free_kb = (unsigned int)((pmm_state.free_frames * FRAME_SIZE) / 1024);
    unsigned int reserved_kb = (unsigned int)((pmm_state.reserved_frames * FRAME_SIZE) / 1024);

    kprintf("[PMM] Total frames: %u (%u KB)\n",
            (unsigned int)pmm_state.total_frames, total_kb);
    kprintf("[PMM] Free frames: %u (%u KB)\n",
            (unsigned int)pmm_state.free_frames, free_kb);
    kprintf("[PMM] Reserved frames: %u (%u KB)\n",
            (unsigned int)pmm_state.reserved_frames, reserved_kb);
}

bool pmm_is_initialized(void) {
    return pmm_state.initialized;
}

/**
 * Allocate a physical frame (4KB page)
 *
 * RT Constraint: O(1) operation, <100 cycles (with free list)
 *
 * **CURRENT IMPLEMENTATION: O(n) bitmap scan**
 * This is NOT RT-safe! Do not use in RT-critical paths.
 * Will be optimized with per-CPU free lists for O(1) allocation.
 *
 * For now, use only during initialization or non-RT code paths.
 */
phys_addr_t pmm_alloc_page(void) {
    // Runtime guard even in release builds to avoid silently returning
    // bogus addresses if initialization was skipped.
    if (!pmm_state.initialized || pmm_state.bitmap == NULL) {
        kprintf("[PMM] ERROR: pmm_alloc_page called before initialization\n");
        return 0;
    }

    PRECONDITION("PMM must be initialized");
    kassert(pmm_state.initialized);
    kassert_not_null(pmm_state.bitmap);

    INVARIANT("free_frames count must be <= total_frames");
    kassert(pmm_state.free_frames <= pmm_state.total_frames);

    if (pmm_state.free_frames == 0) {
        kprintf("[PMM] ERROR: Out of physical frames\n");
        return 0;
    }

    // Find first free frame
    size_t frame = bitmap_find_free();
    if (frame >= MAX_FRAMES) {
        // Out of memory
        return 0;
    }

    INVARIANT("found frame must be free");
    kassert(!bitmap_test(frame));

    // Mark frame as allocated
    bitmap_set(frame);
    pmm_state.free_frames--;

    // Convert frame number to physical address
    phys_addr_t addr = (phys_addr_t)(frame * FRAME_SIZE);

    // Even without DEBUG assertions, make sure we never hand back
    // a misaligned frame because that will poison CR3/PD setups.
    if ((addr & (FRAME_SIZE - 1)) != 0) {
        kprintf("[PMM] ERROR: Allocated frame not aligned: 0x%08x\n",
                (unsigned int)addr);
        return 0;
    }

    POSTCONDITION("returned address must be frame-aligned");
    kassert_aligned(addr, FRAME_SIZE);

    return addr;
}

/**
 * Free a physical frame
 *
 * RT Constraint: O(1) operation, <50 cycles
 */
void pmm_free_page(phys_addr_t page) {
    PRECONDITION("PMM must be initialized");
    kassert(pmm_state.initialized);
    kassert_not_null(pmm_state.bitmap);

    // Validate alignment
    PRECONDITION("page address must be frame-aligned");
    kassert_aligned(page, FRAME_SIZE);

    // Convert physical address to frame number
    size_t frame = page / FRAME_SIZE;

    PRECONDITION("frame must be within valid range");
    kassert(frame < MAX_FRAMES);

    PRECONDITION("frame must be allocated (cannot free twice)");
    if (!bitmap_test(frame)) {
        kprintf("[PMM] ERROR: Attempt to free already-free frame %lu (addr 0x%08x)\n",
                (unsigned long)frame, (unsigned int)page);
        return;
    }

    // Mark frame as free
    bitmap_clear(frame);
    pmm_state.free_frames++;

    POSTCONDITION("frame is now marked free");
    kassert(!bitmap_test(frame));
    POSTCONDITION("free frame count incremented");
    INVARIANT("free_frames must be <= total_frames");
    kassert(pmm_state.free_frames <= pmm_state.total_frames);
}

/**
 * Reserve a physical memory region
 *
 * Marks frames in the region as allocated.
 */
void pmm_reserve_region(phys_addr_t start, size_t size) {
    // Align start down to frame boundary
    phys_addr_t frame_start = start / FRAME_SIZE;

    // Align end up to frame boundary
    phys_addr_t frame_end = (start + size + FRAME_SIZE - 1) / FRAME_SIZE;

    // Mark frames as allocated
    for (phys_addr_t frame = frame_start; frame < frame_end; frame++) {
        if (frame < MAX_FRAMES) {
            // Only count as reserved if it was previously free AND in our total range
            if (!bitmap_test(frame) && frame < pmm_state.total_frames) {
                if (pmm_state.free_frames > 0) {
                    pmm_state.free_frames--;
                    pmm_state.reserved_frames++;
                }
            }
            bitmap_set(frame);
        }
    }
}

/**
 * Get PMM statistics
 */
void pmm_get_stats(struct pmm_stats *stats) {
    if (!stats) {
        return;
    }

    stats->total_frames = pmm_state.total_frames;
    stats->free_frames = pmm_state.free_frames;
    stats->reserved_frames = pmm_state.reserved_frames;
    stats->kernel_frames = pmm_state.reserved_frames; // For now, same as reserved
}
