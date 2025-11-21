.PHONY: all clean run help

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
               $(ARCH_DIR)/idt_asm.s

C_SOURCES := $(CORE_DIR)/init.c \
             $(CORE_DIR)/percpu.c \
             $(ARCH_DIR)/hal.c \
             $(ARCH_DIR)/idt.c \
             $(ARCH_DIR)/timer.c \
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

# Build and run with tests enabled
test: clean
	@echo "Building with KERNEL_TESTS=1..."
	@$(MAKE) KERNEL_TESTS=1 all
	@echo "Running tests in QEMU..."
	@timeout 10 qemu-system-i386 -cdrom $(ISO) -nographic || true

# Clean
clean:
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
