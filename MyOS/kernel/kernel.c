#include "kernel.h"
#include "drivers/wifi.h"
#include "drivers/ethernet.h"
#include "interrupt.h"
#include "mm.h"
#include "elf_loader.h"
#include "filesystem.h"
#include "network.h"
#include "port_io.h"
#include "console.h"
#include "timer.h"
#include "debug.h"
#include "task.h"

// Kernel globals
static bool kernel_initialized = false;
static uint64_t uptime_ticks = 0;

// Early initialization (runs before memory management)
static KernelStatus early_init() {
    // Initialize debug console first
    console_init();
    kprintf("[%s v%s] Booting...\n", KERNEL_NAME, KERNEL_VERSION);
    
    // Verify we're in long mode
    uint32_t unused, edx;
    asm volatile ("cpuid" : "=d"(edx) : "a"(0x80000001) : "ecx", "ebx");
    if (!(edx & (1 << 29))) {
        kernel_panic("Not running in 64-bit mode!");
    }
    
    return KERNEL_OK;
}

// Hardware initialization
static KernelStatus hardware_init() {
    kprintf("- Initializing hardware...\n");
    
    // Initialize critical subsystems
    if (init_interrupts() != 0) {
        return KERNEL_INIT_FAIL;
    }
    
    timer_init(1000);  // 1ms timer ticks
    
    // Initialize memory management
    if (init_paging() != 0 || mm_init(KERNEL_HEAP_START, KERNEL_HEAP_SIZE) != 0) {
        return KERNEL_MEM_ERROR;
    }
    
    // Initialize drivers
    if (ethernet_init() != 0) {
        kprintf("WARNING: Ethernet initialization failed\n");
    }
    
    if (wifi_scan() != 0) {
        kprintf("WARNING: WiFi initialization failed\n");
    }
    
    return KERNEL_OK;
}

// High-level subsystem initialization
static KernelStatus subsystem_init() {
    kprintf("- Initializing subsystems...\n");
    
    if (vfs_init() != 0) {
        return KERNEL_INIT_FAIL;
    }
    
    if (net_init() != 0) {
        return KERNEL_INIT_FAIL;
    }
    
    // Initialize task scheduler (if multitasking is enabled)
    // task_init();
    
    return KERNEL_OK;
}

// Kernel panic handler
__attribute__((noreturn)) void kernel_panic(const char* message) {
    console_set_color(0x4F);  // White on red
    kprintf("\nKERNEL PANIC: %s\n", message);
    kprintf("System halted\n");
    
    // Disable interrupts and halt
    asm volatile ("cli");
    for(;;) {
        asm volatile ("hlt");
    }
}

// Main kernel entry point (called from bootloader)
void kernel_entry() {
    KernelStatus status;
    
    // Phase 1: Early initialization
    if ((status = early_init()) != KERNEL_OK) {
        kernel_panic("Early initialization failed");
    }
    
    // Phase 2: Hardware initialization
    if ((status = hardware_init()) != KERNEL_OK) {
        kernel_panic("Hardware initialization failed");
    }
    
    // Phase 3: Subsystem initialization
    if ((status = subsystem_init()) != KERNEL_OK) {
        kernel_panic("Subsystem initialization failed");
    }
    
    // Mark kernel as initialized
    kernel_initialized = true;
    kprintf("\n%s v%s ready\n", KERNEL_NAME, KERNEL_VERSION);
    kprintf("> ");
    
    // Main kernel loop
    while (1) {
        // Handle interrupts and scheduling
        asm volatile ("sti");
        asm volatile ("hlt");
        
        // Increment uptime counter
        uptime_ticks++;
        
        // Add your scheduler and system calls here
    }
}