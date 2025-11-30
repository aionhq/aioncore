# Multi-Architecture Support Plan

This file consolidates the multi-arch strategy and porting playbook. The core is kept architecture-neutral; all arch-specific code lives in `arch/<arch>/`.

## Philosophy
- Validate on x86 first, then expand.
- Keep the HAL surface stable; no arch-specific code in `core/`, `drivers/`, `lib/`, or `mm/`.
- Width-agnostic types in shared code (`uintptr_t`, `size_t`, `phys_addr_t`); no inline asm outside `arch/`.

## Current Architecture Support
- ✅ x86 (32-bit) – primary development target
- 📋 x86_64 – planned long-mode port
- 📋 RISC-V – planned second architecture
- 📋 ARM – future consideration

## Repository Layout & Build Selection
- `arch/x86/`      – 32-bit x86
- `arch/x86_64/`   – long mode (planned)
- `arch/riscv/`    – RISC-V (planned)
- Makefile: add `ARCH ?= x86` to select toolchain, linker script, and arch sources.
- Per-arch run targets (e.g., `make run ARCH=x86_64`) recommended.

## HAL Abstraction Layer (Stable Surface)
All architecture-specific code is isolated behind `struct hal_ops` (see `include/kernel/hal.h`). Invariants:
- All hardware access through HAL.
- No dynamic allocation in interrupt/syscall hot paths.
- `cpu_context_t` layout documented and matched by arch asm.
- Segment selectors/CSRs set for user entry (DPL=3 or U-mode) before running userspace.

## Architecture-Neutral vs Specific Code
- **Neutral:** scheduler, IPC/capabilities, per-CPU data, string/lib helpers, drivers (via HAL), core init.
- **Specific (per `arch/<arch>/`):**
  - Boot code + linker script
  - HAL implementation
  - Interrupt/trap stubs and handlers
  - Paging (page tables, TLB management)
  - Context switch asm
  - Syscall entry/exit path
  - Atomics if needed beyond builtins

## Porting Playbook (per arch)
1. **Boot to `kmain`**
   - Minimal bootstrap, stacks, mode switch (protected/long/S-mode).
   - Install GDT/TSS (x86/x86_64) or trap vector (RISC-V).
   - Print via minimal console (serial or VGA/fb).
2. **Paging**
   - Implement arch-native tables (x86_64: PML4/PDPT/PD/PT; RISC-V: Sv39/Sv48).
   - Identity map kernel code/data/stack; map kernel virtual base if used.
   - Implement `mmu_map/unmap/switch` under arch.
3. **Interrupts + Context Switch**
   - Stubs/trap handlers for arch frame layout.
   - Context switch asm matching `cpu_context_t`.
   - Wire HAL IRQ enable/disable/register.
4. **Syscall Entry/Exit**
   - x86: INT 0x80 gate DPL=3 (interrupt gate 0xEE).
   - x86_64: SYSCALL/SYSRET; document clobbers (RCX/R11) and arg order (RDI, RSI, RDX, R10, R8, R9; RAX return).
   - RISC-V: ECALL from U-mode; a7=syscall#, a0–a5 args; a0 return.
   - Install gate/trap with user privilege; ensure per-task kernel stack (TSS.esp0 or equivalent) is set.
5. **Testing**
   - Host tests remain arch-neutral.
   - Per-arch QEMU smoke: boot banner, timer tick, simple task switch.
   - Syscall smoke: direct handler call, then real entry path.

## Syscall ABI Snapshot
- **x86 (32-bit, INT 0x80):** EAX=syscall#, EBX/ECX/EDX/ESI/EDI args; EAX return; IF cleared by gate 0xEE (DPL=3).
- **x86_64 (planned):** SYSCALL/SYSRET; RAX=syscall#, RDI/RSI/RDX/R10/R8/R9 args; RAX return; RCX/R11 clobbered by SYSCALL; per-task kernel stack via TSS.esp0.
- **RISC-V (planned):** ECALL from U-mode; a7=syscall#, a0–a5 args; a0 return; save/restore S-mode CSRs as needed.

## Milestones per Arch
1. Boot to `kmain` with console output.
2. Paging enabled with correct maps.
3. Interrupts firing; timer tick works.
4. Context switch + scheduler tick proven.
5. Syscall entry/exit path working (kernel-mode tests).
6. User-mode test task making a syscall.

## When to Switch
- Finish Phase 3 on 32-bit x86 (syscalls + first ring3 task).
- Schedule an x86_64 sprint when you need >4 GiB VA, modern protections (NX/SMEP/SMAP, PCID), or SYSCALL/SYSRET performance.
- Do RISC-V after x86_64 using the same playbook.

## RISC-V Notes (When Ready)
- Simpler boot (no legacy segments), clean Sv39 paging, PLIC for interrupts.
- Boot via OpenSBI to S-mode; set `stvec`, enable timer/interrupts, and handle ECALL.
- Use `sfence.vma` for TLB shootdowns; save/restore S-mode CSRs in context switch.

