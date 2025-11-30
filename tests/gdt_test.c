/**
 * Host-side unit tests for GDT descriptor encoding
 *
 * Tests GDT segment descriptor encoding without needing full kernel.
 * Validates:
 * - Descriptor encoding (base, limit, flags)
 * - Segment selector calculations
 * - TSS descriptor encoding
 */

#include "host_test.h"
#include <string.h>

// GDT descriptor structure (8 bytes)
typedef struct {
    uint16_t limit_low;      // Limit bits 0-15
    uint16_t base_low;       // Base bits 0-15
    uint8_t  base_mid;       // Base bits 16-23
    uint8_t  access;         // Access byte (P/DPL/S/Type)
    uint8_t  granularity;    // Flags and limit bits 16-19
    uint8_t  base_high;      // Base bits 24-31
} __attribute__((packed)) gdt_descriptor_t;

// Access byte flags
#define GDT_ACCESS_PRESENT      (1 << 7)  // Present bit
#define GDT_ACCESS_DPL_0        (0 << 5)  // Privilege level 0 (kernel)
#define GDT_ACCESS_DPL_3        (3 << 5)  // Privilege level 3 (user)
#define GDT_ACCESS_DESCRIPTOR   (1 << 4)  // Descriptor type (1=code/data, 0=system)
#define GDT_ACCESS_EXECUTABLE   (1 << 3)  // Executable (code segment)
#define GDT_ACCESS_RW           (1 << 1)  // Readable (code) or Writable (data)

// Granularity byte flags
#define GDT_GRAN_4K             (1 << 7)  // 4KB granularity
#define GDT_GRAN_32BIT          (1 << 6)  // 32-bit mode

/**
 * Encode a GDT descriptor
 *
 * @param base Physical base address
 * @param limit Segment limit
 * @param access Access byte (P/DPL/S/Type)
 * @param gran Granularity and flags
 * @return Encoded descriptor
 */
static gdt_descriptor_t encode_gdt_descriptor(uint32_t base, uint32_t limit,
                                               uint8_t access, uint8_t gran) {
    gdt_descriptor_t desc;

    // Encode limit (20 bits)
    desc.limit_low = limit & 0xFFFF;
    desc.granularity = (gran & 0xF0) | ((limit >> 16) & 0x0F);

    // Encode base (32 bits)
    desc.base_low = base & 0xFFFF;
    desc.base_mid = (base >> 16) & 0xFF;
    desc.base_high = (base >> 24) & 0xFF;

    // Access byte
    desc.access = access;

    return desc;
}

/**
 * Decode base address from descriptor
 */
static uint32_t decode_base(const gdt_descriptor_t* desc) {
    return desc->base_low |
           ((uint32_t)desc->base_mid << 16) |
           ((uint32_t)desc->base_high << 24);
}

/**
 * Decode limit from descriptor
 */
static uint32_t decode_limit(const gdt_descriptor_t* desc) {
    return desc->limit_low |
           ((uint32_t)(desc->granularity & 0x0F) << 16);
}

// ========== TESTS ==========

TEST(gdt_descriptor_encoding_null) {
    // Null descriptor should be all zeros
    gdt_descriptor_t desc = encode_gdt_descriptor(0, 0, 0, 0);

    TEST_ASSERT_EQ(desc.limit_low, 0, "Null descriptor limit_low should be 0");
    TEST_ASSERT_EQ(desc.base_low, 0, "Null descriptor base_low should be 0");
    TEST_ASSERT_EQ(desc.base_mid, 0, "Null descriptor base_mid should be 0");
    TEST_ASSERT_EQ(desc.access, 0, "Null descriptor access should be 0");
    TEST_ASSERT_EQ(desc.granularity, 0, "Null descriptor granularity should be 0");
    TEST_ASSERT_EQ(desc.base_high, 0, "Null descriptor base_high should be 0");

    return 1;
}

TEST(gdt_descriptor_encoding_kernel_code) {
    // Kernel code segment: base=0, limit=0xFFFFF, 32-bit, 4KB granularity
    uint8_t access = GDT_ACCESS_PRESENT | GDT_ACCESS_DPL_0 |
                     GDT_ACCESS_DESCRIPTOR | GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW;
    uint8_t gran = GDT_GRAN_4K | GDT_GRAN_32BIT;

    gdt_descriptor_t desc = encode_gdt_descriptor(0, 0xFFFFF, access, gran);

    // Verify base
    uint32_t base = decode_base(&desc);
    TEST_ASSERT_EQ(base, 0, "Kernel code base should be 0");

    // Verify limit
    uint32_t limit = decode_limit(&desc);
    TEST_ASSERT_EQ(limit, 0xFFFFF, "Kernel code limit should be 0xFFFFF");

    // Verify access byte
    TEST_ASSERT((desc.access & GDT_ACCESS_PRESENT) != 0, "Should be present");
    TEST_ASSERT((desc.access & 0x60) == GDT_ACCESS_DPL_0, "Should be DPL=0");
    TEST_ASSERT((desc.access & GDT_ACCESS_DESCRIPTOR) != 0, "Should be code/data segment");
    TEST_ASSERT((desc.access & GDT_ACCESS_EXECUTABLE) != 0, "Should be executable");

    // Verify granularity
    TEST_ASSERT((desc.granularity & GDT_GRAN_4K) != 0, "Should have 4KB granularity");
    TEST_ASSERT((desc.granularity & GDT_GRAN_32BIT) != 0, "Should be 32-bit");

    return 1;
}

TEST(gdt_descriptor_encoding_user_data) {
    // User data segment: base=0, limit=0xFFFFF, 32-bit, 4KB granularity, DPL=3
    uint8_t access = GDT_ACCESS_PRESENT | GDT_ACCESS_DPL_3 |
                     GDT_ACCESS_DESCRIPTOR | GDT_ACCESS_RW;
    uint8_t gran = GDT_GRAN_4K | GDT_GRAN_32BIT;

    gdt_descriptor_t desc = encode_gdt_descriptor(0, 0xFFFFF, access, gran);

    // Verify DPL=3 (ring 3)
    TEST_ASSERT((desc.access & 0x60) == GDT_ACCESS_DPL_3, "Should be DPL=3");

    // Verify NOT executable
    TEST_ASSERT((desc.access & GDT_ACCESS_EXECUTABLE) == 0, "Data segment should not be executable");

    return 1;
}

TEST(gdt_descriptor_encoding_tss) {
    // TSS descriptor: base=0x12345678, limit=0x67, system descriptor, present, DPL=0
    uint32_t tss_base = 0x12345678;
    uint32_t tss_limit = 0x67;
    uint8_t access = GDT_ACCESS_PRESENT | GDT_ACCESS_DPL_0 | 0x09;  // Type=9 (Available TSS)
    uint8_t gran = 0;  // Byte granularity

    gdt_descriptor_t desc = encode_gdt_descriptor(tss_base, tss_limit, access, gran);

    // Verify base encoding
    uint32_t base = decode_base(&desc);
    TEST_ASSERT_EQ(base, tss_base, "TSS base should encode correctly");

    // Verify limit
    uint32_t limit = decode_limit(&desc);
    TEST_ASSERT_EQ(limit, tss_limit, "TSS limit should encode correctly");

    // Verify it's a system descriptor (bit 4 = 0)
    TEST_ASSERT((desc.access & GDT_ACCESS_DESCRIPTOR) == 0, "TSS should be system descriptor");

    // Verify type field (bits 0-3 should be 0x09 for available TSS)
    TEST_ASSERT_EQ(desc.access & 0x0F, 0x09, "TSS type should be 0x09 (Available TSS)");

    return 1;
}

TEST(gdt_selector_calculation) {
    // GDT entry 1 (kernel code) should map to selector 0x08
    // Selector format: index << 3 | TI (0 for GDT) | RPL
    uint16_t selector_kernel_code = (1 << 3) | 0 | 0;  // Entry 1, GDT, ring 0
    TEST_ASSERT_EQ(selector_kernel_code, 0x08, "Kernel code selector should be 0x08");

    // GDT entry 3 (user code) with RPL=3 should map to selector 0x1B
    uint16_t selector_user_code = (3 << 3) | 0 | 3;  // Entry 3, GDT, ring 3
    TEST_ASSERT_EQ(selector_user_code, 0x1B, "User code selector should be 0x1B");

    // GDT entry 4 (user data) with RPL=3 should map to selector 0x23
    uint16_t selector_user_data = (4 << 3) | 0 | 3;  // Entry 4, GDT, ring 3
    TEST_ASSERT_EQ(selector_user_data, 0x23, "User data selector should be 0x23");

    return 1;
}

TEST(gdt_descriptor_size) {
    // Verify descriptor is exactly 8 bytes
    TEST_ASSERT_EQ(sizeof(gdt_descriptor_t), 8, "GDT descriptor should be 8 bytes");

    return 1;
}

TEST(gdt_base_wraps_correctly) {
    // Test all 32 bits of base address
    uint32_t test_bases[] = {
        0x00000000,
        0xFFFFFFFF,
        0x12345678,
        0xABCDEF00,
        0x00001000,
        0x80000000
    };

    for (size_t i = 0; i < sizeof(test_bases) / sizeof(test_bases[0]); i++) {
        gdt_descriptor_t desc = encode_gdt_descriptor(test_bases[i], 0xFFFFF,
                                                       GDT_ACCESS_PRESENT, 0);
        uint32_t decoded = decode_base(&desc);
        TEST_ASSERT_EQ(decoded, test_bases[i], "Base address encoding/decoding should be lossless");
    }

    return 1;
}

TEST(gdt_limit_20bit_max) {
    // Limit is 20 bits, max value is 0xFFFFF
    gdt_descriptor_t desc = encode_gdt_descriptor(0, 0xFFFFF, 0, 0);
    uint32_t limit = decode_limit(&desc);
    TEST_ASSERT_EQ(limit, 0xFFFFF, "20-bit limit should encode to 0xFFFFF");

    // Test that higher bits are masked
    desc = encode_gdt_descriptor(0, 0x1FFFFF, 0, 0);  // Try to set bit 20
    limit = decode_limit(&desc);
    TEST_ASSERT_EQ(limit, 0xFFFFF, "Limit should be masked to 20 bits");

    return 1;
}
