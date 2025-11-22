#ifndef KERNEL_MMU_H
#define KERNEL_MMU_H

#include <stdint.h>
#include <stddef.h>
#include <kernel/types.h>

/**
 * Memory Management Unit (MMU) - Virtual Memory Management
 *
 * Provides architecture-independent interface for managing virtual memory
 * and address spaces. Each unit will have its own address space for isolation.
 *
 * Design:
 * - Opaque page_table_t type (arch-specific internals)
 * - O(1) map/unmap operations (no global sweeps)
 * - Per-unit address spaces (no global assumptions)
 * - Lazy page table allocation
 *
 * RT Constraints:
 * - mmu_map_page(): O(1), <200 cycles
 * - mmu_unmap_page(): O(1), <100 cycles
 * - mmu_switch_address_space(): O(1), <50 cycles
 */

// Opaque page table type (architecture-specific internals hidden)
typedef struct page_table page_table_t;

// Virtual address type
typedef uintptr_t virt_addr_t;

// Page flags (architecture-independent, mapped to arch-specific bits)
#define MMU_PRESENT   (1 << 0)  // Page is present in memory
#define MMU_WRITABLE  (1 << 1)  // Page is writable
#define MMU_USER      (1 << 2)  // Page is accessible from user mode
#define MMU_NOCACHE   (1 << 3)  // Page is not cached (for MMIO)
#define MMU_EXEC      (1 << 4)  // Page is executable (if arch supports NX)

/**
 * Create a new address space
 *
 * Allocates a new page table structure. The address space is initially empty.
 * The caller is responsible for mapping pages into it.
 *
 * This will become the basis for unit address spaces:
 *   unit->addr_space = mmu_create_address_space();
 *
 * @return Pointer to new address space, or NULL on failure
 *
 * RT: O(1) - allocates one page directory frame
 */
page_table_t* mmu_create_address_space(void);

/**
 * Destroy an address space
 *
 * Frees all page tables associated with the address space.
 * Does NOT free the physical pages mapped into the address space.
 * The caller is responsible for tracking and freeing those separately.
 *
 * @param pt Address space to destroy
 *
 * RT: O(n) where n = number of page tables allocated
 *     (Not RT-safe, use only during unit cleanup)
 */
void mmu_destroy_address_space(page_table_t* pt);

/**
 * Map a physical page to a virtual address
 *
 * Creates a virtual -> physical mapping in the given address space.
 * If the necessary page table does not exist, it will be allocated.
 *
 * @param pt Address space to map into
 * @param phys Physical address to map (must be page-aligned)
 * @param virt Virtual address to map at (must be page-aligned)
 * @param flags Page flags (MMU_PRESENT | MMU_WRITABLE | etc)
 * @return Virtual address on success, NULL on failure
 *
 * RT Constraint: O(1) operation, <200 cycles
 * - Direct 2-level indexing
 * - Allocates at most 1 page table
 * - No loops or global sweeps
 */
void* mmu_map_page(page_table_t* pt, phys_addr_t phys,
                   virt_addr_t virt, uint32_t flags);

/**
 * Unmap a virtual address
 *
 * Removes the virtual -> physical mapping at the given address.
 * Flushes the TLB for this address. Does NOT free the physical page.
 *
 * @param pt Address space to unmap from
 * @param virt Virtual address to unmap (must be page-aligned)
 *
 * RT Constraint: O(1) operation, <100 cycles
 * - Direct 2-level indexing
 * - No loops or global sweeps
 */
void mmu_unmap_page(page_table_t* pt, virt_addr_t virt);

/**
 * Switch to a different address space
 *
 * Loads the page table into the CPU's address space register (e.g., CR3 on x86).
 * This causes the CPU to start using the new virtual memory mappings.
 *
 * Used during context switches:
 *   mmu_switch_address_space(next_unit->addr_space);
 *
 * @param pt Address space to switch to
 *
 * RT Constraint: O(1) operation, <50 cycles
 * - Single register write + TLB flush
 */
void mmu_switch_address_space(page_table_t* pt);

/**
 * Get current address space
 *
 * Returns the currently active address space by reading the CPU's
 * address space register (e.g., CR3 on x86).
 *
 * @return Current address space
 *
 * RT: O(1), <10 cycles
 */
page_table_t* mmu_get_current_address_space(void);

/**
 * Get kernel address space
 *
 * Returns the kernel's address space. This is a special address space
 * that is shared across all units (kernel is always mapped).
 *
 * @return Kernel address space
 */
page_table_t* mmu_get_kernel_address_space(void);

/**
 * Initialize MMU subsystem
 *
 * Sets up the initial kernel address space with identity mapping.
 * Must be called once during kernel initialization.
 *
 * After this call:
 * - Paging is enabled
 * - Kernel is identity-mapped (phys == virt)
 * - mmu_get_kernel_address_space() returns valid pointer
 */
void mmu_init(void);

// Page size (architecture-independent constant)
#define PAGE_SIZE 4096

#if PAGE_SIZE != 4096
#error "PAGE_SIZE must be 4096 bytes"
#endif

// Page alignment helpers
#define PAGE_ALIGN_DOWN(addr) ((addr) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(addr)   (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define IS_PAGE_ALIGNED(addr) (((addr) & (PAGE_SIZE - 1)) == 0)

#endif // KERNEL_MMU_H
