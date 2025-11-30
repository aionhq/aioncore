/**
 * Userspace Task Management
 *
 * Functions for creating and managing ring 3 userspace tasks.
 */

#include <kernel/user.h>
#include <kernel/task.h>
#include <kernel/mmu.h>
#include <kernel/pmm.h>
#include <kernel/scheduler.h>
#include <drivers/vga.h>
#include <lib/string.h>

// External symbols from user_test.s
extern uint8_t user_test_start[];
extern uint8_t user_test_end[];

/**
 * Create a userspace task (ring 3)
 *
 * Allocates and maps user code and stack pages with USER flag.
 * Initializes task with ring 3 segments and privilege level.
 *
 * @param name        Task name
 * @param entry_point Physical address of user code (NULL = use built-in test)
 * @param code_size   Size of user code in bytes (0 = use built-in test)
 * @return            Task pointer, or NULL on failure
 */
task_t* task_create_user(const char* name, void* entry_point, size_t code_size) {
    kprintf("[USER] Creating userspace task '%s'\n", name);

    // Use built-in test program if no code provided
    if (!entry_point || code_size == 0) {
        entry_point = (void*)user_test_start;
        code_size = user_test_end - user_test_start;
        kprintf("[USER] Using built-in test program (%u bytes)\n", (unsigned int)code_size);
    }

    // Allocate task structure
    task_t* task = task_alloc();
    if (!task) {
        kprintf("[USER] ERROR: Failed to allocate task structure\n");
        return NULL;
    }

    // Initialize basic task info
    strlcpy(task->name, name, sizeof(task->name));
    task->state = TASK_STATE_READY;
    task->priority = SCHED_DEFAULT_PRIORITY;

    // Allocate user code page
    phys_addr_t code_phys = pmm_alloc_page();
    if (!code_phys) {
        kprintf("[USER] ERROR: Failed to allocate code page\n");
        return NULL;
    }

    // Allocate user stack page
    phys_addr_t stack_phys = pmm_alloc_page();
    if (!stack_phys) {
        kprintf("[USER] ERROR: Failed to allocate stack page\n");
        pmm_free_page(code_phys);
        return NULL;
    }

    // For now, use kernel address space with user mappings
    // TODO Phase 3.4: Create separate address space per task
    task->address_space = mmu_get_kernel_address_space();

    // Map user code page with USER|PRESENT flags
    kprintf("[USER] Mapping user code at 0x%08x (phys=0x%08x)\n",
            USER_CODE_BASE, (unsigned int)code_phys);

    void* code_virt = mmu_map_page(task->address_space,
                                    code_phys,
                                    USER_CODE_BASE,
                                    MMU_PRESENT | MMU_USER | MMU_WRITABLE);
    if (!code_virt) {
        kprintf("[USER] ERROR: Failed to map user code page\n");
        pmm_free_page(code_phys);
        pmm_free_page(stack_phys);
        return NULL;
    }

    // Copy user code to mapped virtual address
    // Since we're still in kernel address space, we can access it directly
    memcpy((void*)USER_CODE_BASE, entry_point, code_size);

    // Map user stack with USER|PRESENT|WRITABLE
    uintptr_t user_stack_base = USER_STACK_TOP - USER_STACK_SIZE;
    kprintf("[USER] Mapping user stack at 0x%08lx-0x%08lx (phys=0x%08x)\n",
            (unsigned long)user_stack_base, (unsigned long)USER_STACK_TOP, (unsigned int)stack_phys);

    void* stack_virt = mmu_map_page(task->address_space,
                                     stack_phys,
                                     user_stack_base,
                                     MMU_PRESENT | MMU_USER | MMU_WRITABLE);
    if (!stack_virt) {
        kprintf("[USER] ERROR: Failed to map user stack page\n");
        mmu_unmap_page(task->address_space, USER_CODE_BASE);
        pmm_free_page(code_phys);
        pmm_free_page(stack_phys);
        return NULL;
    }

    // Allocate kernel stack for syscall entry
    task->kernel_stack_size = 4096;
    task->kernel_stack = (void*)pmm_alloc_page();
    if (!task->kernel_stack) {
        kprintf("[USER] ERROR: Failed to allocate kernel stack\n");
        mmu_unmap_page(task->address_space, USER_CODE_BASE);
        mmu_unmap_page(task->address_space, user_stack_base);
        pmm_free_page(code_phys);
        pmm_free_page(stack_phys);
        return NULL;
    }

    // Initialize CPU context for ring 3
    memset(&task->context, 0, sizeof(task->context));

    // Set ring 3 segments
    task->context.cs = USER_CS_SELECTOR;  // 0x1B (GDT entry 3, RPL=3)
    task->context.ss = USER_DS_SELECTOR;  // 0x23 (GDT entry 4, RPL=3)
    task->context.ds = USER_DS_SELECTOR;
    task->context.es = USER_DS_SELECTOR;
    task->context.fs = USER_DS_SELECTOR;
    task->context.gs = USER_DS_SELECTOR;

    // Set entry point and stack
    task->context.eip = USER_CODE_BASE;
    task->context.esp = USER_STACK_TOP;
    task->context.ebp = USER_STACK_TOP;

    // Set EFLAGS with IF=1 (interrupts enabled)
    task->context.eflags = USER_EFLAGS;

    // Note: Other registers (eax, ebx, etc.) aren't in cpu_context_t
    // They will be 0 after the memset above

    kprintf("[USER] Task '%s' initialized:\n", name);
    kprintf("[USER]   CS=0x%04lx SS=0x%04lx DS=0x%04lx\n",
            (unsigned long)task->context.cs,
            (unsigned long)task->context.ss,
            (unsigned long)task->context.ds);
    kprintf("[USER]   EIP=0x%08lx ESP=0x%08lx EFLAGS=0x%08lx\n",
            (unsigned long)task->context.eip,
            (unsigned long)task->context.esp,
            (unsigned long)task->context.eflags);
    kprintf("[USER]   Kernel stack: %p (size=%lu)\n",
            task->kernel_stack, (unsigned long)task->kernel_stack_size);

    return task;
}
