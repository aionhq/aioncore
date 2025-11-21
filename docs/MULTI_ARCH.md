# Multi-Architecture Support Plan

## Philosophy

**Validate on x86, then expand.** The HAL abstraction is designed for multiple architectures from day one, but we'll prove the design works on x86 before adding complexity.

## Current Architecture Support

- ✅ **x86 (32-bit)** - Primary development target
- 📋 **RISC-V** - Planned second architecture
- 📋 **ARM** - Future consideration

## HAL Abstraction Layer

All architecture-specific code is isolated behind `struct hal_ops`:

```c
// include/kernel/hal.h
struct hal_ops {
    // CPU Management
    void (*cpu_init)(void);
    uint32_t (*cpu_id)(void);
    void (*cpu_halt)(void);

    // Interrupts
    void (*irq_enable)(void);
    uint32_t (*irq_disable)(void);
    void (*irq_restore)(uint32_t state);

    // Memory Management
    void (*mmu_init)(void);
    void* (*mmu_map)(phys_addr_t, virt_addr_t, uint32_t flags);
    void (*mmu_flush_tlb)(virt_addr_t);

    // I/O (may not exist on all architectures)
    uint8_t (*io_inb)(uint16_t port);
    void (*io_outb)(uint16_t port, uint8_t value);

    // ... etc
};

extern struct hal_ops* hal;  // Points to architecture implementation
```

## Architecture-Specific vs Architecture-Neutral Code

### Architecture-Neutral (Core Kernel)
These components should have **zero** architecture-specific code:

- ✅ **IPC subsystem** - Pure message passing logic
- ✅ **Capability system** - Pure security policy
- ✅ **Scheduler** - Priority queues, policy (uses HAL for context switch)
- ✅ **Per-CPU infrastructure** - Data structures only
- ✅ **VGA driver** - Memory-mapped I/O (uses HAL for mapping)
- ✅ **String library** - Pure C
- ✅ **List operations** - Pure C

### Architecture-Specific (per arch/ directory)

Each architecture must implement:

1. **Boot code** (assembly)
   - Set up stack
   - Jump to `kmain()`
   - x86: multiboot header, protected mode setup
   - RISC-V: SBI setup, supervisor mode entry
   - ARM: similar

2. **HAL implementation** (C + inline assembly)
   - File: `arch/ARCH/hal.c`
   - Implements all `hal_ops` functions

3. **Interrupt handling** (assembly + C)
   - IDT/IVT setup
   - Fast interrupt stubs
   - Exception handlers

4. **MMU/Paging** (C + assembly)
   - Page table format varies by arch
   - TLB management
   - Address space switching

5. **Context switching** (assembly)
   - Save/restore registers
   - Must be <200 cycles for RT

6. **Syscall entry/exit** (assembly)
   - x86: `int 0x80` or `sysenter`
   - RISC-V: `ecall`
   - ARM: `svc`

7. **Atomics** (usually compiler builtins)
   - `__sync_*` builtins work on all modern archs

## Directory Structure

```
kernel/
├── arch/
│   ├── x86/
│   │   ├── boot.s           # x86 multiboot boot
│   │   ├── hal.c            # x86 HAL implementation
│   │   ├── idt.c            # x86 interrupt handling
│   │   ├── idt_asm.s        # x86 interrupt stubs
│   │   ├── context.s        # x86 context switch
│   │   ├── syscall.s        # x86 syscall entry
│   │   └── linker.ld        # x86 linker script
│   │
│   ├── riscv/               # Future: RISC-V port
│   │   ├── boot.s           # RISC-V boot
│   │   ├── hal.c            # RISC-V HAL
│   │   ├── trap.c           # RISC-V interrupt/exception handling
│   │   ├── trap.s           # RISC-V trap vector
│   │   ├── context.s        # RISC-V context switch
│   │   └── linker.ld        # RISC-V linker script
│   │
│   └── arm/                 # Future: ARM port
│       └── ...
│
├── core/                    # Architecture-neutral
│   ├── init.c
│   ├── percpu.c
│   ├── scheduler.c          # (future)
│   ├── ipc.c                # (future)
│   └── capability.c         # (future)
│
├── include/
│   ├── kernel/              # Architecture-neutral headers
│   └── arch/                # Architecture-specific headers
│       ├── x86/
│       └── riscv/
```

## Adding a New Architecture: Checklist

### Step 1: Boot and HAL
- [ ] Create `arch/ARCH/boot.s` - minimal boot to C
- [ ] Create `arch/ARCH/hal.c` - implement `hal_ops`
- [ ] Create `arch/ARCH/linker.ld` - memory layout
- [ ] Get to `kmain()` and print "Hello World"

### Step 2: Interrupts
- [ ] Create interrupt/exception handling
- [ ] Implement timer interrupt
- [ ] Wire up to HAL `irq_*` functions

### Step 3: Memory Management
- [ ] Implement page table creation
- [ ] Implement `mmu_map` / `mmu_unmap`
- [ ] Implement TLB flushing

### Step 4: Scheduling
- [ ] Implement context switch assembly
- [ ] Test task switching
- [ ] Verify <1µs context switch time

### Step 5: Syscalls
- [ ] Implement syscall entry/exit
- [ ] Test ring3 → ring0 → ring3 transition
- [ ] Verify IPC performance

## RISC-V Port Details (When Ready)

### Why RISC-V is a Good Second Architecture

**Simpler than x86:**
- No legacy baggage (no real mode, no segments, no TSS)
- Orthogonal instruction set
- Standard interrupt controller (PLIC)
- Clean page table design (Sv39 is elegant)

**Good for RT:**
- Deterministic instruction timing
- Simple pipeline
- No microcode
- Perfect for embedded RT systems

### RISC-V Specifics

**Boot:**
- Use OpenSBI as firmware
- Jump to supervisor mode
- Set up trap vector

**Interrupts:**
- PLIC (Platform-Level Interrupt Controller)
- Standard CSRs: `mtvec`, `stvec`, `sie`, `sip`
- Traps go to single vector, dispatch in software

**Paging:**
- Sv39: 39-bit virtual addresses, 3-level page tables
- Page table entry format different from x86
- TLB flush: `sfence.vma` instruction

**Context Switch:**
- Save/restore 31 general-purpose registers
- Save/restore CSRs (`sstatus`, `sepc`, etc.)
- Probably <150 cycles (faster than x86!)

**Syscalls:**
- `ecall` instruction
- Trap to supervisor mode
- CSRs contain syscall number and args

### RISC-V Estimated Effort

- Boot + HAL: ~2 days
- Interrupts: ~1 day
- MMU: ~2 days
- Context switch: ~1 day
- Syscalls: ~1 day

**Total: ~1 week** to port once x86 is solid.

## Build System Changes Needed

### Current (x86 only):
```makefile
ARCH := x86
CC := i686-elf-gcc
AS := i686-elf-as
```

### Future (multi-arch):
```makefile
# Set architecture (default x86)
ARCH ?= x86

# Architecture-specific toolchains
ifeq ($(ARCH),x86)
    CC := i686-elf-gcc
    AS := i686-elf-as
    CFLAGS += -m32
endif

ifeq ($(ARCH),riscv)
    CC := riscv64-unknown-elf-gcc
    AS := riscv64-unknown-elf-as
    CFLAGS += -march=rv64imac -mabi=lp64
endif

# Architecture-specific sources
ARCH_SOURCES := $(wildcard arch/$(ARCH)/*.c)
ARCH_ASM := $(wildcard arch/$(ARCH)/*.s)

# Architecture-neutral sources
CORE_SOURCES := $(wildcard core/*.c)
```

## Testing Multi-Architecture Support

### Before Adding Second Architecture:

1. **Audit core/ for architecture assumptions**
   - No hardcoded addresses
   - No inline assembly outside arch/
   - No x86-specific types

2. **Check HAL coverage**
   - All arch-specific operations go through HAL
   - No direct hardware access in core/

3. **Verify build isolation**
   - `make ARCH=x86` doesn't leak into arch-neutral code
   - Headers properly separated

### After Adding RISC-V:

1. **Ensure both architectures build**
   - `make ARCH=x86` works
   - `make ARCH=riscv` works

2. **Run same tests on both**
   - Boot test
   - Exception handling test
   - IPC performance test
   - Context switch latency test

3. **Compare performance**
   - Document any architectural differences
   - Ensure RT constraints met on both

## Current Guidelines for x86 Development

To keep our x86 code portable:

### DO:
- ✅ Use HAL for all hardware access
- ✅ Keep arch-specific code in `arch/x86/`
- ✅ Use standard C types (`uint32_t`, not `unsigned long`)
- ✅ Document any x86 assumptions in comments

### DON'T:
- ❌ Put inline assembly in `core/` or `lib/`
- ❌ Hardcode I/O ports outside arch code
- ❌ Assume little-endian (use explicit conversions if needed)
- ❌ Assume 32-bit pointers (use `uintptr_t`)

## Example: Portable vs Non-Portable Code

### ❌ Non-Portable:
```c
// core/interrupt.c - BAD!
void handle_interrupt(void) {
    // Direct x86 I/O - not portable!
    outb(0x20, 0x20);  // Send EOI to PIC
}
```

### ✅ Portable:
```c
// core/interrupt.c - GOOD!
void handle_interrupt(void) {
    // Use HAL abstraction
    hal->irq_send_eoi(irq_num);
}

// arch/x86/hal.c
static void irq_send_eoi(uint32_t irq) {
    // x86-specific: 8259 PIC
    if (irq >= 8) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
}

// arch/riscv/hal.c (future)
static void irq_send_eoi(uint32_t irq) {
    // RISC-V: clear pending bit in PLIC
    plic_complete(irq);
}
```

## When to Add RISC-V

**After completing Phase 4 (IPC + Capabilities) on x86:**
- Core microkernel is working
- HAL abstraction validated
- IPC performance measured
- RT constraints verified

**Why wait:**
- Don't complicate development with two architectures
- Validate design on one platform first
- Once working, port is quick (~1 week)

**Benefits of adding RISC-V:**
- Proves HAL abstraction works
- Exposes x86-specific assumptions
- Opens up embedded RT market
- Simpler ISA is easier to reason about for verification

## Conclusion

Multi-architecture support is **definitely feasible** with our HAL design. The strategy:

1. ✅ **Complete x86 implementation first** (Phases 1-4)
2. 📋 Port to RISC-V to validate abstraction (~1 week)
3. 📋 Consider ARM if needed for specific use cases

Keep writing portable code, and the port will be straightforward when ready!
