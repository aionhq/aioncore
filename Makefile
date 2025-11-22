.PHONY: all clean run help analyze test

# Build configuration
CC := i686-elf-gcc
AS := i686-elf-as
LD := i686-elf-ld
GRUB_MKRESCUE := i686-elf-grub-mkrescue

# Directories
ARCH_DIR := arch/x86
CORE_DIR := core
MM_DIR := mm
DRIVERS_DIR := drivers
LIB_DIR := lib
INCLUDE_DIR := include

# Output
KERNEL := kernel.elf
ISO := kernel.iso

# Compiler flags
CFLAGS := -std=gnu99 \
          -ffreestanding \
          -O2 \
          -Wall \
          -Wextra \
          -Werror \
          -fno-exceptions \
          -fno-stack-protector \
          -nostdlib \
          -m32 \
          -I$(INCLUDE_DIR)

# Linker flags
LDFLAGS := -T $(ARCH_DIR)/linker.ld \
           -nostdlib \
           -m32

# Source files
ASM_SOURCES := $(ARCH_DIR)/boot.s \
               $(ARCH_DIR)/idt_asm.s \
               $(ARCH_DIR)/context.s

C_SOURCES := $(CORE_DIR)/init.c \
             $(CORE_DIR)/percpu.c \
             $(CORE_DIR)/task.c \
             $(CORE_DIR)/scheduler.c \
             $(ARCH_DIR)/hal.c \
             $(ARCH_DIR)/idt.c \
             $(ARCH_DIR)/timer.c \
             $(ARCH_DIR)/mmu.c \
             $(MM_DIR)/pmm.c \
             $(LIB_DIR)/string.c \
             $(DRIVERS_DIR)/vga/vga.c \
             $(DRIVERS_DIR)/vga/vga_text.c

# Test sources (optional, enabled with KERNEL_TESTS=1)
ifdef KERNEL_TESTS
C_SOURCES += $(CORE_DIR)/ktest.c \
             $(LIB_DIR)/string_test.c \
             $(ARCH_DIR)/timer_test.c
CFLAGS += -DKERNEL_TESTS=1
endif

# Object files
ASM_OBJECTS := $(ASM_SOURCES:.s=.o)
C_OBJECTS := $(C_SOURCES:.c=.o)
ALL_OBJECTS := $(ASM_OBJECTS) $(C_OBJECTS)

# Default target
all: check-style $(ISO)

# Help
help:
	@echo "Modern Modular Kernel Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  make          - Build kernel ISO"
	@echo "  make run      - Build and run in QEMU"
	@echo "  make test     - Build with tests and run in QEMU"
	@echo "  make clean    - Clean build artifacts"
	@echo "  make help     - Show this message"
	@echo ""
	@echo "Architecture: x86 (32-bit)"
	@echo "Modules: HAL, Per-CPU, VGA driver, Timer"
	@echo ""
	@echo "Testing:"
	@echo "  KERNEL_TESTS=1 make  - Build with unit tests enabled"

# Compile assembly files
%.o: %.s
	@echo "  AS    $<"
	@$(AS) $< -o $@

# Compile C files
%.o: %.c
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Link kernel
$(KERNEL): $(ALL_OBJECTS)
	@echo "  LD    $@"
	@$(CC) $(LDFLAGS) -o $@ $(ALL_OBJECTS) -lgcc

# Create ISO
$(ISO): $(KERNEL) grub.cfg
	@echo "  ISO   $@"
	@mkdir -p isodir/boot/grub
	@cp $(KERNEL) isodir/boot/
	@cp grub.cfg isodir/boot/grub/
	@$(GRUB_MKRESCUE) -o $(ISO) isodir 2>/dev/null || \
		(echo "Error: grub-mkrescue failed"; exit 1)
	@echo ""
	@echo "Build complete: $(ISO)"

# Run in QEMU
run: $(ISO)
	@echo "Starting QEMU..."
	@qemu-system-i386 -cdrom $(ISO)

# Build and run with in-kernel tests enabled
test-kernel: clean
	@echo "Building with KERNEL_TESTS=1..."
	@$(MAKE) KERNEL_TESTS=1 all
	@echo "Running in-kernel tests in QEMU..."
	@timeout 10 qemu-system-i386 -cdrom $(ISO) -nographic || true

# Clean build artifacts
clean: clean-test
	@echo "Cleaning..."
	@rm -f $(ALL_OBJECTS)
	@rm -f $(KERNEL) $(ISO)
	@rm -rf isodir
	@echo "Clean complete"

# Show dependencies
deps:
	@echo "Dependency tree:"
	@echo "  boot.s → boot.o"
	@echo "  init.c → init.o (depends on: hal.h, percpu.h, vga.h)"
	@echo "  hal.c → hal.o (depends on: hal.h, types.h)"
	@echo "  percpu.c → percpu.o (depends on: percpu.h, hal.h)"
	@echo "  string.c → string.o"
	@echo "  vga.c → vga.o (depends on: vga.h)"
	@echo "  vga_text.c → vga_text.o (depends on: vga.h, hal.h)"

# Style / static checks
.PHONY: check-style
check-style:
	@./scripts/check_kernel_c_style.sh

# Full static analysis (requires cppcheck and clang-tidy)
analyze:
	@echo "Running full static analysis..."
	@KERNEL_STATIC_ANALYSIS=1 ./scripts/check_kernel_c_style.sh
	@echo ""
	@echo "Static analysis complete!"
	@echo ""
	@echo "Tools used:"
	@command -v cppcheck &> /dev/null && echo "  ✓ cppcheck" || echo "  ✗ cppcheck (not installed)"
	@command -v clang-tidy &> /dev/null && echo "  ✓ clang-tidy" || echo "  ✗ clang-tidy (not installed)"
	@echo ""
	@echo "Install missing tools:"
	@echo "  macOS: brew install cppcheck llvm"
	@echo "  Linux: apt install cppcheck clang-tidy"

# Host-side unit tests (run without booting kernel)
# Host test configuration
HOST_CC := gcc
HOST_CFLAGS := -std=gnu99 -O0 -g -Wall -Wextra -DHOST_TEST \
               -Iinclude -fno-builtin

# Test sources
TEST_SOURCES := tests/test_main.c \
                tests/pmm_test.c \
                tests/kprintf_test.c \
                tests/scheduler_test.c

# Testable kernel code (compiled with HOST_TEST mocks)
TESTABLE_SOURCES := mm/pmm.c

# Test runner binary
TEST_RUNNER := test_build/test_runner

# Build and run host tests
test: $(TEST_RUNNER)
	@echo "Running host-side unit tests..."
	@./$(TEST_RUNNER)

$(TEST_RUNNER): $(TEST_SOURCES) $(TESTABLE_SOURCES) tests/host_test.h
	@mkdir -p test_build
	@$(HOST_CC) $(HOST_CFLAGS) -o $@ $(TEST_SOURCES) $(TESTABLE_SOURCES)

# Clean test artifacts
clean-test:
	@rm -rf test_build
