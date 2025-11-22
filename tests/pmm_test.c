/**
 * Host-side unit tests for Physical Memory Manager (PMM)
 *
 * These tests run on the host machine (macOS/Linux) without requiring
 * the kernel to boot. Fast feedback loop (<1 second).
 */

#include "host_test.h"

// Mock multiboot structures for testing
#define MULTIBOOT_MAGIC 0x2BADB002
#define MULTIBOOT_FLAG_MMAP (1 << 6)

struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed));

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
    // Use uintptr_t for host tests to avoid pointer truncation on 64-bit
    uintptr_t mmap_addr;
} __attribute__((packed));

// Forward declare PMM functions (from mm/pmm.c)
extern void pmm_init(uint32_t multiboot_magic, struct multiboot_info* mbi);
extern uint64_t pmm_alloc_page(void);
extern void pmm_free_page(uint64_t page);

// Test memory map (simulates GRUB's memory map)
static struct multiboot_mmap_entry test_mmap[] = {
    // Entry 0: Low memory (0-640KB) - Available
    { .size = 20, .addr = 0x0, .len = 0xA0000, .type = 1 },
    // Entry 1: VGA/BIOS (640KB-1MB) - Reserved
    { .size = 20, .addr = 0xA0000, .len = 0x60000, .type = 2 },
    // Entry 2: Extended memory (1MB-128MB) - Available
    { .size = 20, .addr = 0x100000, .len = 0x7F00000, .type = 1 },
};

static struct multiboot_info test_mbi;

// Initialize test multiboot info dynamically
static void init_test_mbi(void) {
    test_mbi.flags = MULTIBOOT_FLAG_MMAP;
    test_mbi.mmap_addr = (uintptr_t)test_mmap;
    test_mbi.mmap_length = sizeof(test_mmap);
}

// Test 1: PMM initializes successfully
TEST(pmm_init_succeeds) {
    init_test_mbi();
    pmm_init(MULTIBOOT_MAGIC, &test_mbi);
    // If we get here without assertion failures, init succeeded
    return 1;
}

// Test 2: Allocated frames are 4K-aligned
TEST(pmm_frames_are_aligned) {
    init_test_mbi();
    pmm_init(MULTIBOOT_MAGIC, &test_mbi);

    // Allocate 10 frames and verify alignment
    for (int i = 0; i < 10; i++) {
        uint64_t addr = pmm_alloc_page();
        TEST_ASSERT_NEQ(addr, 0, "allocation should succeed");
        TEST_ASSERT_EQ(addr & 0xFFF, 0, "frame must be 4K-aligned");
        pmm_free_page(addr);
    }

    return 1;
}

// Test 3: Frame calculation is correct (frame * 4096)
TEST(pmm_frame_calculation_correct) {
    init_test_mbi();
    pmm_init(MULTIBOOT_MAGIC, &test_mbi);

    uint64_t addr = pmm_alloc_page();
    TEST_ASSERT_NEQ(addr, 0, "allocation should succeed");

    // Verify: frame_number * 4096 = address
    uint32_t frame_num = addr / 4096;
    uint64_t reconstructed = (uint64_t)frame_num * 4096;
    TEST_ASSERT_EQ(addr, reconstructed, "frame*4096 calculation is reversible");

    // Specifically test that frame 33 is 0x21000, NOT 0xd34
    if (frame_num == 33) {
        TEST_ASSERT_EQ(addr, 0x21000ULL, "frame 33 should be 0x21000");
    }

    pmm_free_page(addr);
    return 1;
}

// Test 4: Allocated frames are in valid memory range
TEST(pmm_frames_in_valid_range) {
    init_test_mbi();
    pmm_init(MULTIBOOT_MAGIC, &test_mbi);

    // Allocate several frames
    for (int i = 0; i < 5; i++) {
        uint64_t addr = pmm_alloc_page();
        TEST_ASSERT_NEQ(addr, 0, "allocation should succeed");

        // Should be < 128MB (our test memory map limit)
        TEST_ASSERT(addr < 0x8000000, "frame should be < 128MB");

        // Should be 4K aligned
        TEST_ASSERT_EQ(addr & 0xFFF, 0, "frame must be 4K-aligned");

        pmm_free_page(addr);
    }

    return 1;
}

// Test 5: Free and realloc works correctly
TEST(pmm_free_and_realloc) {
    init_test_mbi();
    pmm_init(MULTIBOOT_MAGIC, &test_mbi);

    // Allocate a frame
    uint64_t addr1 = pmm_alloc_page();
    TEST_ASSERT_NEQ(addr1, 0, "first allocation succeeds");

    // Free it
    pmm_free_page(addr1);

    // Allocate again - should get same frame back (or another valid one)
    uint64_t addr2 = pmm_alloc_page();
    TEST_ASSERT_NEQ(addr2, 0, "second allocation succeeds");
    TEST_ASSERT_EQ(addr2 & 0xFFF, 0, "reused frame is still aligned");

    // Note: We don't assert addr1 == addr2 because PMM might return
    // a different frame from the free list

    pmm_free_page(addr2);
    return 1;
}
