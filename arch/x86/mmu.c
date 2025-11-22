/**
 * x86 Memory Management Unit (MMU) Implementation
 *
 * Implements virtual memory using x86 two-level page tables:
 * - Page Directory (1024 entries, each covering 4MB)
 * - Page Tables (1024 entries per table, each covering 4KB)
 *
 * Total address space: 4GB (32-bit)
 * Page size: 4KB
 *
 * RT Constraints enforced:
 * - O(1) map/unmap (direct indexing, no loops)
 * - Lazy page table allocation (allocate on first use)
 */

#include <kernel/mmu.h>
#include <kernel/pmm.h>
#include <kernel/hal.h>
#include <kernel/types.h>
#include <drivers/vga.h>

// x86 page directory entry flags
#define PDE_PRESENT    (1 << 0)
#define PDE_WRITABLE   (1 << 1)
#define PDE_USER       (1 << 2)
#define PDE_ACCESSED   (1 << 5)

// x86 page table entry flags
#define PTE_PRESENT    (1 << 0)
#define PTE_WRITABLE   (1 << 1)
#define PTE_USER       (1 << 2)
#define PTE_NOCACHE    (1 << 4)
#define PTE_ACCESSED   (1 << 5)
#define PTE_DIRTY      (1 << 6)

// Page directory/table sizes
#define ENTRIES_PER_TABLE 1024
#define PD_INDEX(virt) (((virt) >> 22) & 0x3FF)
#define PT_INDEX(virt) (((virt) >> 12) & 0x3FF)
#define PAGE_FRAME(entry) ((entry) & ~0xFFF)

// x86 page table structure (opaque to outside)
struct page_table {
    uint32_t* page_directory;   // Physical address of page directory
    phys_addr_t pd_phys;        // Physical address for CR3
};

// Kernel address space (identity-mapped)
static page_table_t* kernel_address_space = NULL;

/**
 * Convert generic MMU flags to x86 PTE flags
 */
static inline uint32_t flags_to_x86(uint32_t flags) {
    uint32_t x86_flags = 0;

    if (flags & MMU_PRESENT)  x86_flags |= PTE_PRESENT;
    if (flags & MMU_WRITABLE) x86_flags |= PTE_WRITABLE;
    if (flags & MMU_USER)     x86_flags |= PTE_USER;
    if (flags & MMU_NOCACHE)  x86_flags |= PTE_NOCACHE;

    return x86_flags;
}

/**
 * Flush TLB for a single page
 */
static inline void flush_tlb_single(virt_addr_t virt) {
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

/**
 * Flush entire TLB by reloading CR3
 */
static inline void flush_tlb_all(void) {
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

/**
 * Create a new address space
 *
 * RT: O(1) - allocates one page directory frame
 */
page_table_t* mmu_create_address_space(void) {
    if (!pmm_is_initialized()) {
        kprintf("[MMU] ERROR: PMM not initialized; cannot create address space\n");
        return NULL;
    }

    // Allocate page_table structure from heap (NOT from PMM)
    // PMM returns physical addresses, not pointers!
    static page_table_t pt_storage;  // FIXME: Should use kmalloc when available
    page_table_t* pt = &pt_storage;

    // Allocate page directory (this IS a physical frame)
    phys_addr_t pd_phys = pmm_alloc_page();
    if (!pd_phys) {
        kprintf("[MMU] ERROR: Failed to allocate page directory\n");
        return NULL;
    }
    if (!IS_PAGE_ALIGNED(pd_phys)) {
        kprintf("[MMU] ERROR: Page directory not page-aligned: 0x%08x\n",
                (unsigned int)pd_phys);
        return NULL;
    }

    // Map page directory to virtual address for access
    // For now, use identity mapping (phys == virt)
    uint32_t* pd = (uint32_t*)(uintptr_t)pd_phys;

    // Zero out page directory
    for (int i = 0; i < ENTRIES_PER_TABLE; i++) {
        pd[i] = 0;
    }

    pt->page_directory = pd;
    pt->pd_phys = pd_phys;

    kprintf("[MMU] Page directory allocated at phys 0x%08x\n", (unsigned int)pd_phys);

    return pt;
}

/**
 * Destroy an address space
 *
 * RT: O(n) where n = number of page tables
 * NOT RT-safe, use only during unit cleanup
 */
void mmu_destroy_address_space(page_table_t* pt) {
    if (!pt) {
        return;
    }

    uint32_t* pd = pt->page_directory;

    // Free all page tables
    for (int i = 0; i < ENTRIES_PER_TABLE; i++) {
        if (pd[i] & PDE_PRESENT) {
            phys_addr_t pt_phys = PAGE_FRAME(pd[i]);
            pmm_free_page(pt_phys);
        }
    }

    // Free page directory
    pmm_free_page(pt->pd_phys);

    // NOTE: pt is static storage, not dynamically allocated
    // When we have kmalloc, we'll need to kfree(pt) here
}

/**
 * Map a physical page to a virtual address
 *
 * RT Constraint: O(1) operation, <200 cycles
 * - Direct 2-level indexing (no loops)
 * - Allocates at most 1 page table
 */
void* mmu_map_page(page_table_t* pt, phys_addr_t phys,
                   virt_addr_t virt, uint32_t flags) {
    if (!pt) {
        return NULL;
    }

    // Validate alignment
    if (!IS_PAGE_ALIGNED(phys) || !IS_PAGE_ALIGNED(virt)) {
        return NULL;
    }

    uint32_t* pd = pt->page_directory;

    // Calculate indices (O(1))
    uint32_t pd_index = PD_INDEX(virt);
    uint32_t pt_index = PT_INDEX(virt);

    // Check if page table exists
    if (!(pd[pd_index] & PDE_PRESENT)) {
        // Allocate new page table (O(1))
        phys_addr_t pt_phys = pmm_alloc_page();
        if (!pt_phys) {
            return NULL;
        }

        // Map page table (identity mapping for now)
        uint32_t* page_table = (uint32_t*)(uintptr_t)pt_phys;

        // Zero out page table
        for (int i = 0; i < ENTRIES_PER_TABLE; i++) {
            page_table[i] = 0;
        }

        // Install page table in directory
        pd[pd_index] = pt_phys | PDE_PRESENT | PDE_WRITABLE | PDE_USER;
    }

    // Get page table
    uint32_t* page_table = (uint32_t*)(uintptr_t)PAGE_FRAME(pd[pd_index]);

    // Convert flags to x86 format
    uint32_t x86_flags = flags_to_x86(flags);

    // Install page mapping (O(1))
    page_table[pt_index] = phys | x86_flags;

    // Flush TLB for this page (O(1))
    flush_tlb_single(virt);

    return (void*)virt;
}

/**
 * Unmap a virtual address
 *
 * RT Constraint: O(1) operation, <100 cycles
 */
void mmu_unmap_page(page_table_t* pt, virt_addr_t virt) {
    if (!pt) {
        return;
    }

    // Validate alignment
    if (!IS_PAGE_ALIGNED(virt)) {
        return;
    }

    uint32_t* pd = pt->page_directory;

    // Calculate indices (O(1))
    uint32_t pd_index = PD_INDEX(virt);
    uint32_t pt_index = PT_INDEX(virt);

    // Check if page table exists
    if (!(pd[pd_index] & PDE_PRESENT)) {
        return;  // Page not mapped
    }

    // Get page table
    uint32_t* page_table = (uint32_t*)(uintptr_t)PAGE_FRAME(pd[pd_index]);

    // Clear page table entry (O(1))
    page_table[pt_index] = 0;

    // Flush TLB for this page (O(1))
    flush_tlb_single(virt);
}

/**
 * Switch to a different address space
 *
 * RT Constraint: O(1) operation, <50 cycles
 */
void mmu_switch_address_space(page_table_t* pt) {
    if (!pt) {
        return;
    }

    // Load CR3 with physical address of page directory (O(1))
    __asm__ volatile("mov %0, %%cr3" : : "r"(pt->pd_phys) : "memory");
}

/**
 * Get current address space
 *
 * RT: O(1), <10 cycles
 */
page_table_t* mmu_get_current_address_space(void) {
    // Read CR3
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    // For now, just return kernel address space
    // TODO: Track per-CPU current address space
    return kernel_address_space;
}

/**
 * Get kernel address space
 */
page_table_t* mmu_get_kernel_address_space(void) {
    return kernel_address_space;
}

/**
 * Initialize MMU subsystem
 *
 * Sets up kernel address space with identity mapping and enables paging.
 */
void mmu_init(void) {
    kprintf("[MMU] Initializing x86 paging...\n");

    // Check PMM stats before starting
    struct pmm_stats stats;
    pmm_get_stats(&stats);
    kprintf("[MMU] PMM stats before init: %u total, %u free, %u reserved\n",
            (unsigned int)stats.total_frames,
            (unsigned int)stats.free_frames,
            (unsigned int)stats.reserved_frames);

    // Create kernel address space
    kernel_address_space = mmu_create_address_space();
    if (!kernel_address_space) {
        kprintf("[MMU] ERROR: Failed to create kernel address space\n");
        return;
    }
    if (!IS_PAGE_ALIGNED(kernel_address_space->pd_phys)) {
        kprintf("[MMU] ERROR: Kernel page directory not aligned (pd_phys=0x%08x)\n",
                (unsigned int)kernel_address_space->pd_phys);
        return;
    }

    kprintf("[MMU] Kernel address space created\n");

    // Check PMM stats after creating address space
    pmm_get_stats(&stats);
    kprintf("[MMU] PMM stats after creating address space:\n");
    kprintf("[MMU]   Total: %u frames\n", (unsigned int)stats.total_frames);
    kprintf("[MMU]   Free: %u frames (%u KB)\n",
            (unsigned int)stats.free_frames,
            (unsigned int)(stats.free_frames * 4));
    kprintf("[MMU]   Reserved: %u frames\n", (unsigned int)stats.reserved_frames);

    if (stats.free_frames == 0) {
        kprintf("[MMU] FATAL: No free frames available for page tables!\n");
        kprintf("[MMU] Cannot enable paging without memory for page tables.\n");
        return;
    }

    // Identity map first 16MB to ensure kernel, stack, GDT, and all low memory is covered
    // Skip NULL page (0x0) for safety
    kprintf("[MMU] Identity mapping 16MB (skipping NULL page)...\n");

    for (virt_addr_t virt = PAGE_SIZE; virt < 16 * 1024 * 1024; virt += PAGE_SIZE) {
        phys_addr_t phys = virt;  // Identity mapping

        void* result = mmu_map_page(kernel_address_space, phys, virt,
                                     MMU_PRESENT | MMU_WRITABLE);
        if (!result) {
            kprintf("[MMU] Failed to map 0x%08x (out of PT frames)\n", (unsigned int)virt);
            break;  // Stop if we run out of PMM frames for page tables
        }
    }

    kprintf("[MMU] Identity mapping complete\n");

    // Load CR3 with page directory BEFORE enabling paging
    kprintf("[MMU] Loading page directory into CR3...\n");
    mmu_switch_address_space(kernel_address_space);

    // Now enable paging by setting CR0.PG bit
    kprintf("[MMU] Enabling paging...\n");
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (1 << 31);  // Set PG bit
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0) : "memory");

    kprintf("[MMU] Paging enabled successfully!\n");
    kprintf("[MMU] Page size: %u bytes\n", (unsigned int)PAGE_SIZE);
    kprintf("[MMU] Kernel page_table_t struct: %p\n",
            (void*)kernel_address_space);
    kprintf("[MMU] Page directory physical address (CR3): 0x%08x\n",
            (unsigned int)kernel_address_space->pd_phys);
}
