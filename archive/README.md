# Archive

This directory contains earlier implementations of the kernel.

## Assembly Kernel (`kernel_asm.s`)

The original pure assembly implementation of the kernel.

**Features:**
- Direct VGA text mode output
- No dependencies
- Minimal and educational

**Why archived:**
- C provides better readability and maintainability
- Higher-level language allows for easier extension
- Assembly is still used for the bootstrap (`boot.s`)

**To build the assembly kernel:**
```bash
make asm
make run
```

The assembly kernel is still fully functional and can be built using the Makefile's `asm` target.
