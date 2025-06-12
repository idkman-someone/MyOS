#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stdbool.h>

// Kernel version information
#define KERNEL_NAME    "MyOS"
#define KERNEL_VERSION "1.0.0"
#define KERNEL_MAGIC   0x4E4C4E4B  // 'KERN' in ASCII

// Memory management
#define KERNEL_HEAP_START 0x200000
#define KERNEL_HEAP_SIZE  0x100000

// Error codes
typedef enum {
    KERNEL_OK,
    KERNEL_INIT_FAIL,
    KERNEL_MEM_ERROR,
    KERNEL_DRIVER_ERROR,
    KERNEL_PANIC
} KernelStatus;

// Kernel main functions
void kernel_entry();
void kernel_panic(const char* message);
KernelStatus kernel_init();

// Early debug output
void kprintf(const char* format, ...);

#endif // KERNEL_H