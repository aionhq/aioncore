# AionCore - RT Microkernel

A real-time microkernel with **units**, **capabilities**, and **message-passing** designed from the ground up for:
- Hard real-time guarantees
- Multi-core scalability
- Capability-based security
- Userspace services
- Formal verification potential

## Quick Start

```bash
# Build
make

# Run in QEMU
make run

# Clean
make clean
```

## Current Status

**Phase 1:** ✅ Foundation complete (HAL, per-CPU, IDT, VGA)
**Phase 2:** 🔨 In progress (Timer, memory, paging)

👉 **See [CURRENT_WORK.md](CURRENT_WORK.md) for today's status and next steps.**

## Architecture

This is **not** a UNIX clone. Key concepts:

- **Units:** Isolated execution containers (not "processes")
- **Threads:** Execute within units
- **Channels:** Message-passing IPC
- **Capabilities:** Explicit access rights, no ambient authority
- **No POSIX in kernel:** POSIX is a userspace personality

The kernel is <10K LOC and provides only core primitives. Everything else (filesystems, drivers, services) runs in userspace.

## Documentation

📂 **[docs/](docs/)** - All documentation

**Start here:**
- 📍 **[CURRENT_WORK.md](CURRENT_WORK.md)** - What we're working on NOW
- 📔 **[DEVELOPMENT_LOG.md](DEVELOPMENT_LOG.md)** - Development narrative and history
- 📖 [docs/DOCS.md](docs/DOCS.md) - Documentation index
- 🎯 [docs/VISION.md](docs/VISION.md) - Long-term vision and goals
- 🗺️ [docs/IMPLEMENTATION_ROADMAP.md](docs/IMPLEMENTATION_ROADMAP.md) - Phase-by-phase plan

**Design details:**
- 🏗️ [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) - Design principles
- 🔷 [docs/UNITS_ARCHITECTURE.md](docs/UNITS_ARCHITECTURE.md) - Units model
- ⏱️ [docs/RT_CONSTRAINTS.md](docs/RT_CONSTRAINTS.md) - Real-time requirements
- ✓ [docs/FORMAL_VERIFICATION.md](docs/FORMAL_VERIFICATION.md) - Verification strategy
- 🌐 [docs/MULTI_ARCH.md](docs/MULTI_ARCH.md) - Multi-architecture support

**Development:**
- 📝 [docs/KERNEL_C_STYLE.md](docs/KERNEL_C_STYLE.md) - C coding standards and rules
- 🧪 [docs/TESTING.md](docs/TESTING.md) - Unit testing guide
- 🐛 [docs/ISSUES.md](docs/ISSUES.md) - Known issues and action items

## Project Structure

```
kernel/
├── CURRENT_WORK.md          ← Start here for current status
├── DEVELOPMENT_LOG.md       ← Development narrative and history
├── README.md                ← You are here
├── Makefile                 ← Build system
├── grub.cfg                 ← GRUB configuration
├── .claude.md               ← Development workflow rules
│
├── arch/x86/                ← x86-specific code
│   ├── boot.s               │  Multiboot entry point
│   ├── hal.c                │  Hardware abstraction layer
│   ├── idt.c                │  Interrupt handling
│   ├── idt_asm.s            │  Interrupt stubs
│   └── linker.ld            │  Memory layout
│
├── core/                    ← Architecture-neutral kernel core
│   ├── init.c               │  Kernel entry and initialization
│   └── percpu.c             │  Per-CPU data structures
│
├── drivers/                 ← Device drivers (modular)
│   └── vga/                 │  VGA text mode driver
│       ├── vga.c            │  VGA subsystem
│       └── vga_text.c       │  Text mode implementation
│
├── lib/                     ← Kernel library functions
│   └── string.c             │  Safe string operations
│
├── mm/                      ← Memory management (Phase 2)
│   └── (coming soon)
│
├── include/                 ← Public headers
│   ├── kernel/              │  Core kernel headers
│   │   ├── hal.h
│   │   ├── idt.h
│   │   ├── percpu.h
│   │   └── types.h
│   └── drivers/             │  Driver interfaces
│       └── vga.h
│
└── docs/                    ← Documentation
    ├── DOCS.md              │  Documentation index
    ├── VISION.md            │  Long-term vision
    ├── IMPLEMENTATION_ROADMAP.md  │  Development plan
    ├── UNITS_ARCHITECTURE.md      │  Units model details
    ├── RT_CONSTRAINTS.md          │  RT requirements
    ├── FORMAL_VERIFICATION.md     │  Verification strategy
    ├── MULTI_ARCH.md              │  Multi-arch support
    ├── ARCHITECTURE.md            │  Design principles
    └── ISSUES.md                  │  Issue tracking
```

## Features

### ✅ Implemented (Phase 1 & 2.1)

- Hardware Abstraction Layer (HAL)
- Per-CPU data structures (cache-line aligned)
- IDT and interrupt handling (256 entries)
- Exception handlers with register dumps
- Modular VGA driver with kprintf
- Safe string library (no strcpy/strcat)
- Lock-free per-CPU tracing
- PIT timer with TSC calibration (1000 Hz, microsecond precision)
- Unit testing framework (ktest) with example tests

### 🔨 In Progress (Phase 2.2)

- Physical memory manager (bitmap-based)
- Basic paging and address spaces

### 📋 Planned

- Phase 3: Tasks, threads, scheduler, syscalls
- Phase 4: IPC, capabilities, message passing
- Phase 5: Userspace services
- Phase 6: SMP/multicore
- Phase 7: More userspace servers
- Phase 8: Advanced features (shared memory, IRQ caps)

## Design Principles

1. **Microkernel First** - IPC and capabilities early, not late
2. **Real-Time Throughout** - Every path has bounded time
3. **Userspace by Default** - If it can be userspace, it must be
4. **Per-CPU Everything** - Minimize locking, maximize parallelism
5. **Capability Security** - No ambient authority
6. **Small TCB** - <10K LOC for verification
7. **No POSIX in Kernel** - Build as userspace personality

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for detailed rationale.

## Real-Time Guarantees

| Operation | Target | Status |
|-----------|--------|--------|
| Context switch | <200 cycles | Phase 3 |
| Scheduler pick | <100 cycles | Phase 3 |
| IPC send/recv | <500 cycles | Phase 4 |
| IRQ dispatch | <100 cycles | ✅ Ready |
| Interrupt latency | <10µs | Phase 2 |

See [docs/RT_CONSTRAINTS.md](docs/RT_CONSTRAINTS.md) for full requirements.

## Building

### Requirements

- i686-elf cross-compiler
- GNU Make
- GRUB tools (grub-mkrescue)
- QEMU (for testing)

### Build Commands

```bash
# Full build
make

# Clean build
make clean && make

# Run in QEMU
make run

# Build and run with unit tests
make test

# Show help
make help
```

## Development

**Daily workflow:**

1. Check [CURRENT_WORK.md](CURRENT_WORK.md) for current status
2. Read [DEVELOPMENT_LOG.md](DEVELOPMENT_LOG.md) to understand context and history
3. Follow [docs/IMPLEMENTATION_ROADMAP.md](docs/IMPLEMENTATION_ROADMAP.md) for APIs
4. Follow [docs/KERNEL_C_STYLE.md](docs/KERNEL_C_STYLE.md) before/after coding
5. Follow [docs/RT_CONSTRAINTS.md](docs/RT_CONSTRAINTS.md) for performance
6. Update docs when completing work

**Coding guidelines:**

- Small functions (<50 LOC)
- No undefined behavior
- Bounded execution time (O(1) in RT paths)
- Document invariants
- Keep arch code in `arch/`
- All hardware access via HAL
- Write unit tests for new subsystems

See [docs/KERNEL_C_STYLE.md](docs/KERNEL_C_STYLE.md) for complete coding standards.

## Contributing

This is an experimental kernel exploring modern OS design patterns. Key areas:

- Capability-based security
- Message-passing IPC
- Real-time scheduling
- Lock-free per-CPU patterns
- Formal verification techniques

See [docs/VISION.md](docs/VISION.md) for the full design philosophy.

## License

MIT License - Copyright (c) 2025 sistemica GmbH

See [LICENSE](LICENSE) for full details.

## References

**Influences:**
- seL4 - Formally verified microkernel
- Fuchsia - Capability-based Zircon kernel
- QNX - Real-time microkernel
- MINIX - Pioneering microkernel design

**Our twist:**
- Units instead of processes
- Built for RT from day one
- Designed for formal verification
- No POSIX in kernel
- Per-CPU lock-free patterns
- Message-passing by default

---

**Start exploring:** Read [CURRENT_WORK.md](CURRENT_WORK.md) for what's happening now!
