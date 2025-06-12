# MyOS Makefile
# Comprehensive build system for a custom operating system

# ==================== CONFIGURATION ====================
# Architecture and target
ARCH := i386
TARGET := $(ARCH)-elf

# Directories
BUILD_DIR := build
BOOT_DIR := boot
KERNEL_DIR := kernel
DRIVERS_DIR := drivers
FS_DIR := fs
NET_DIR := net
LIB_DIR := lib
USERSPACE_DIR := userspace
TOOLS_DIR := tools
INCLUDE_DIR := include

# Output files
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
BOOTLOADER_BIN := $(BUILD_DIR)/boot.bin
OS_IMAGE := $(BUILD_DIR)/myos.img
ISO_IMAGE := $(BUILD_DIR)/myos.iso

# ==================== TOOLCHAIN ====================
# Cross-compilation toolchain
CC := $(TARGET)-gcc
AS := $(TARGET)-as
LD := $(TARGET)-ld
AR := $(TARGET)-ar
OBJCOPY := $(TARGET)-objcopy
OBJDUMP := $(TARGET)-objdump

# Native tools for userspace programs and tools
NATIVE_CC := gcc
NASM := nasm
QEMU := qemu-system-i386

# ==================== COMPILER FLAGS ====================
# Common flags
WARNINGS := -Wall -Wextra -Werror -Wno-unused-parameter
INCLUDES := -I$(INCLUDE_DIR) -I$(KERNEL_DIR) -I$(DRIVERS_DIR) -I$(FS_DIR) -I$(NET_DIR) -I$(LIB_DIR)

# Kernel C flags
KERNEL_CFLAGS := $(WARNINGS) $(INCLUDES) \
				 -std=gnu99 -ffreestanding -O2 \
				 -fno-builtin -fno-stack-protector \
				 -nostdlib -nostdinc -fno-pie -no-pie \
				 -mno-red-zone -mno-mmx -mno-sse -mno-sse2

# Assembly flags
ASFLAGS := --32

# NASM flags
NASMFLAGS := -f elf32

# Linker flags
LDFLAGS := -T $(BOOT_DIR)/linker.ld -nostdlib -lgcc

# ==================== SOURCE FILES ====================
# Bootloader sources
BOOT_ASM_SOURCES := $(wildcard $(BOOT_DIR)/*.asm)
BOOT_OBJECTS := $(BOOT_ASM_SOURCES:$(BOOT_DIR)/%.asm=$(BUILD_DIR)/boot/%.o)

# Kernel sources
KERNEL_C_SOURCES := $(wildcard $(KERNEL_DIR)/*.c)
KERNEL_ASM_SOURCES := $(wildcard $(KERNEL_DIR)/*.s)
KERNEL_OBJECTS := $(KERNEL_C_SOURCES:$(KERNEL_DIR)/%.c=$(BUILD_DIR)/kernel/%.o) \
				  $(KERNEL_ASM_SOURCES:$(KERNEL_DIR)/%.s=$(BUILD_DIR)/kernel/%.o)

# Driver sources
DRIVER_SOURCES := $(wildcard $(DRIVERS_DIR)/*.c)
DRIVER_OBJECTS := $(DRIVER_SOURCES:$(DRIVERS_DIR)/%.c=$(BUILD_DIR)/drivers/%.o)

# Filesystem sources
FS_SOURCES := $(wildcard $(FS_DIR)/*.c)
FS_OBJECTS := $(FS_SOURCES:$(FS_DIR)/%.c=$(BUILD_DIR)/fs/%.o)

# Network sources
NET_SOURCES := $(wildcard $(NET_DIR)/*.c)
NET_OBJECTS := $(NET_SOURCES:$(NET_DIR)/%.c=$(BUILD_DIR)/net/%.o)

# Library sources
LIB_SOURCES := $(wildcard $(LIB_DIR)/*.c)
LIB_OBJECTS := $(LIB_SOURCES:$(LIB_DIR)/%.c=$(BUILD_DIR)/lib/%.o)

# Userspace sources
USERSPACE_SOURCES := $(wildcard $(USERSPACE_DIR)/*.c) $(wildcard $(USERSPACE_DIR)/programs/*.c)
USERSPACE_OBJECTS := $(USERSPACE_SOURCES:$(USERSPACE_DIR)/%.c=$(BUILD_DIR)/userspace/%.o)

# All kernel objects
ALL_KERNEL_OBJECTS := $(KERNEL_OBJECTS) $(DRIVER_OBJECTS) $(FS_OBJECTS) $(NET_OBJECTS) $(LIB_OBJECTS)

# ==================== MAIN TARGETS ====================
.PHONY: all clean kernel bootloader image iso run debug help install toolchain

# Default target
all: $(OS_IMAGE)

# Build the complete OS image
$(OS_IMAGE): $(BOOTLOADER_BIN) $(KERNEL_BIN)
	@echo "Creating OS image..."
	@mkdir -p $(BUILD_DIR)
	@dd if=/dev/zero of=$@ bs=1024 count=1440 2>/dev/null
	@dd if=$(BOOTLOADER_BIN) of=$@ conv=notrunc 2>/dev/null
	@dd if=$(KERNEL_BIN) of=$@ seek=1 conv=notrunc 2>/dev/null
	@echo "OS image created: $@"

# Create ISO image
iso: $(ISO_IMAGE)

$(ISO_IMAGE): $(OS_IMAGE)
	@echo "Creating ISO image..."
	@mkdir -p $(BUILD_DIR)/iso/boot
	@cp $(OS_IMAGE) $(BUILD_DIR)/iso/boot/
	@cp $(KERNEL_BIN) $(BUILD_DIR)/iso/boot/
	@$(TOOLS_DIR)/mkiso.sh $(BUILD_DIR)/iso $@
	@echo "ISO image created: $@"

# ==================== BOOTLOADER ====================
bootloader: $(BOOTLOADER_BIN)

$(BOOTLOADER_BIN): $(BOOT_OBJECTS)
	@echo "Linking bootloader..."
	@mkdir -p $(BUILD_DIR)
	@$(LD) $(LDFLAGS) -o $@ $^

# Bootloader assembly files
$(BUILD_DIR)/boot/%.o: $(BOOT_DIR)/%.asm
	@echo "Assembling $<..."
	@mkdir -p $(dir $@)
	@$(NASM) $(NASMFLAGS) -o $@ $<

# ==================== KERNEL ====================
kernel: $(KERNEL_BIN)

$(KERNEL_BIN): $(ALL_KERNEL_OBJECTS)
	@echo "Linking kernel..."
	@mkdir -p $(BUILD_DIR)
	@$(LD) $(LDFLAGS) -o $@ $^

# Kernel C files
$(BUILD_DIR)/kernel/%.o: $(KERNEL_DIR)/%.c
	@echo "Compiling $<..."
	@mkdir -p $(dir $@)
	@$(CC) $(KERNEL_CFLAGS) -c $< -o $@

# Kernel assembly files
$(BUILD_DIR)/kernel/%.o: $(KERNEL_DIR)/%.s
	@echo "Assembling $<..."
	@mkdir -p $(dir $@)
	@$(AS) $(ASFLAGS) -o $@ $<

# ==================== DRIVERS ====================
$(BUILD_DIR)/drivers/%.o: $(DRIVERS_DIR)/%.c
	@echo "Compiling driver $<..."
	@mkdir -p $(dir $@)
	@$(CC) $(KERNEL_CFLAGS) -c $< -o $@

# ==================== FILESYSTEM ====================
$(BUILD_DIR)/fs/%.o: $(FS_DIR)/%.c
	@echo "Compiling filesystem $<..."
	@mkdir -p $(dir $@)
	@$(CC) $(KERNEL_CFLAGS) -c $< -o $@

# ==================== NETWORKING ====================
$(BUILD_DIR)/net/%.o: $(NET_DIR)/%.c
	@echo "Compiling network $<..."
	@mkdir -p $(dir $@)
	@$(CC) $(KERNEL_CFLAGS) -c $< -o $@

# ==================== LIBRARIES ====================
$(BUILD_DIR)/lib/%.o: $(LIB_DIR)/%.c
	@echo "Compiling library $<..."
	@mkdir -p $(dir $@)
	@$(CC) $(KERNEL_CFLAGS) -c $< -o $@

# ==================== USERSPACE ====================
userspace: $(USERSPACE_OBJECTS)

$(BUILD_DIR)/userspace/%.o: $(USERSPACE_DIR)/%.c
	@echo "Compiling userspace $<..."
	@mkdir -p $(dir $@)
	@$(CC) $(KERNEL_CFLAGS) -c $< -o $@

# Individual userspace programs
$(BUILD_DIR)/userspace/programs/hello: $(BUILD_DIR)/userspace/programs/hello.o
	@echo "Linking hello program..."
	@mkdir -p $(dir $@)
	@$(LD) -o $@ $< $(LIB_OBJECTS)

$(BUILD_DIR)/userspace/programs/ls: $(BUILD_DIR)/userspace/programs/ls.o
	@echo "Linking ls program..."
	@mkdir -p $(dir $@)
	@$(LD) -o $@ $< $(LIB_OBJECTS)

$(BUILD_DIR)/userspace/programs/cat: $(BUILD_DIR)/userspace/programs/cat.o
	@echo "Linking cat program..."
	@mkdir -p $(dir $@)
	@$(LD) -o $@ $< $(LIB_OBJECTS)

# ==================== TOOLS ====================
tools: $(BUILD_DIR)/tools/myman

$(BUILD_DIR)/tools/myman: $(TOOLS_DIR)/myman.c $(TOOLS_DIR)/myman.h
	@echo "Building package manager..."
	@mkdir -p $(dir $@)
	@$(NATIVE_CC) -o $@ $< -I$(TOOLS_DIR)

# ==================== TESTING AND DEBUGGING ====================
# Run in QEMU
run: $(OS_IMAGE)
	@echo "Starting QEMU..."
	@$(QEMU) -fda $(OS_IMAGE) -boot a

# Run with debugging
debug: $(OS_IMAGE)
	@echo "Starting QEMU with GDB server..."
	@$(QEMU) -fda $(OS_IMAGE) -boot a -s -S &
	@echo "Connect GDB to localhost:1234"

# Run from ISO
run-iso: $(ISO_IMAGE)
	@echo "Starting QEMU with ISO..."
	@$(QEMU) -cdrom $(ISO_IMAGE) -boot d

# Generate disassembly for debugging
disasm: $(KERNEL_BIN)
	@echo "Generating kernel disassembly..."
	@$(OBJDUMP) -d $(KERNEL_BIN) > $(BUILD_DIR)/kernel.disasm
	@echo "Disassembly saved to $(BUILD_DIR)/kernel.disasm"

# ==================== MAINTENANCE ====================
# Clean build artifacts
clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)
	@echo "Clean complete."

# Full clean including tools
distclean: clean
	@echo "Deep cleaning..."
	@find . -name "*.o" -delete
	@find . -name "*.bin" -delete
	@find . -name "*.img" -delete
	@find . -name "*.iso" -delete

# Install toolchain (requires internet)
toolchain:
	@echo "Installing cross-compilation toolchain..."
	@scripts/install-toolchain.sh

# Show build information
info:
	@echo "MyOS Build Information"
	@echo "======================"
	@echo "Architecture: $(ARCH)"
	@echo "Target: $(TARGET)"
	@echo "Compiler: $(CC)"
	@echo "Build directory: $(BUILD_DIR)"
	@echo "Kernel objects: $(words $(ALL_KERNEL_OBJECTS))"
	@echo "Userspace objects: $(words $(USERSPACE_OBJECTS))"

# Dependency checking
check-deps:
	@echo "Checking dependencies..."
	@which $(CC) > /dev/null 2>&1 || (echo "Error: $(CC) not found. Run 'make toolchain' first." && exit 1)
	@which $(NASM) > /dev/null 2>&1 || (echo "Error: NASM not found. Please install nasm." && exit 1)
	@which $(QEMU) > /dev/null 2>&1 || (echo "Error: QEMU not found. Please install qemu." && exit 1)
	@echo "Dependencies OK."

# Help target
help:
	@echo "MyOS Build System"
	@echo "=================="
	@echo ""
	@echo "Main targets:"
	@echo "  all        - Build complete OS (default)"
	@echo "  kernel     - Build kernel only"
	@echo "  bootloader - Build bootloader only"
	@echo "  image      - Create OS disk image"
	@echo "  iso        - Create ISO image"
	@echo "  userspace  - Build userspace programs"
	@echo "  tools      - Build development tools"
	@echo ""
	@echo "Testing targets:"
	@echo "  run        - Run OS in QEMU"
	@echo "  run-iso    - Run ISO in QEMU"
	@echo "  debug      - Run with GDB debugging"
	@echo "  disasm     - Generate disassembly"
	@echo ""
	@echo "Maintenance targets:"
	@echo "  clean      - Remove build artifacts"
	@echo "  distclean  - Deep clean everything"
	@echo "  toolchain  - Install cross-compiler"
	@echo "  check-deps - Check build dependencies"
	@echo "  info       - Show build information"
	@echo "  help       - Show this help"

# ==================== DEPENDENCIES ====================
# Header dependencies (simplified - in a real project you'd use -MMD -MP)
$(KERNEL_OBJECTS): $(wildcard $(KERNEL_DIR)/*.h) $(wildcard $(INCLUDE_DIR)/*.h)
$(DRIVER_OBJECTS): $(wildcard $(DRIVERS_DIR)/*.h) $(wildcard $(KERNEL_DIR)/*.h)
$(FS_OBJECTS): $(wildcard $(FS_DIR)/*.h) $(wildcard $(KERNEL_DIR)/*.h)
$(NET_OBJECTS): $(wildcard $(NET_DIR)/*.h) $(wildcard $(KERNEL_DIR)/*.h)
$(LIB_OBJECTS): $(wildcard $(LIB_DIR)/*.h) $(wildcard $(INCLUDE_DIR)/*.h)

# Prevent intermediate file deletion
.PRECIOUS: $(BUILD_DIR)/%.o

# Make build directories as needed
$(BUILD_DIR)/%/:
	@mkdir -p $@