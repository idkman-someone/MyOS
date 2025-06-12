#include "debug.h"
#include "console.h"
#include "port_io.h"
#include <stdarg.h>
#include <stdbool.h>

// Debug configuration
static debug_level_t current_debug_level = DEBUG_INFO;
static bool debug_serial_enabled = true;
static bool debug_console_enabled = true;

// Serial port for debug output
#define DEBUG_SERIAL_PORT COM1_PORT

// Initialize debug subsystem
void debug_init(void) {
    // Initialize serial port for debug output
    outb(DEBUG_SERIAL_PORT + 1, 0x00);    // Disable all interrupts
    outb(DEBUG_SERIAL_PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(DEBUG_SERIAL_PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(DEBUG_SERIAL_PORT + 1, 0x00);    //                  (hi byte)
    outb(DEBUG_SERIAL_PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(DEBUG_SERIAL_PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(DEBUG_SERIAL_PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    
    debug_print(DEBUG_INFO, "Debug subsystem initialized\n");
}

// Set debug level
void debug_set_level(debug_level_t level) {
    current_debug_level = level;
}

// Get current debug level
debug_level_t debug_get_level(void) {
    return current_debug_level;
}

// Enable/disable debug output channels
void debug_enable_serial(bool enable) {
    debug_serial_enabled = enable;
}

void debug_enable_console(bool enable) {
    debug_console_enabled = enable;
}

// Check if serial port is ready for transmission
static bool is_serial_transmit_ready(void) {
    return inb(DEBUG_SERIAL_PORT + 5) & 0x20;
}

// Send character to serial port
static void serial_putchar(char c) {
    while (!is_serial_transmit_ready());
    outb(DEBUG_SERIAL_PORT, c);
}

// Send string to serial port
static void serial_puts(const char* str) {
    while (*str) {
        serial_putchar(*str++);
    }
}

// Convert integer to string
static void itoa(int value, char* buffer, int base) {
    static const char digits[] = "0123456789ABCDEF";
    char* ptr = buffer;
    char* ptr1 = buffer;
    char tmp_char;
    int tmp_value;
    
    if (value == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return;
    }
    
    // Handle negative numbers for decimal base
    bool negative = false;
    if (value < 0 && base == 10) {
        negative = true;
        value = -value;
    }
    
    // Convert to string (reversed)
    while (value) {
        tmp_value = value;
        value /= base;
        *ptr++ = digits[tmp_value - value * base];
    }
    
    // Add negative sign
    if (negative) {
        *ptr++ = '-';
    }
    
    *ptr-- = '\0';
    
    // Reverse string
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

// Debug printf implementation
void debug_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    char* ptr = buffer;
    
    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'd': {
                    int value = va_arg(args, int);
                    char num_buffer[32];
                    itoa(value, num_buffer, 10);
                    char* num_ptr = num_buffer;
                    while (*num_ptr) {
                        *ptr++ = *num_ptr++;
                    }
                    break;
                }
                case 'x': {
                    unsigned int value = va_arg(args, unsigned int);
                    char num_buffer[32];
                    itoa(value, num_buffer, 16);
                    char* num_ptr = num_buffer;
                    while (*num_ptr) {
                        *ptr++ = *num_ptr++;
                    }
                    break;
                }
                case 's': {
                    char* str = va_arg(args, char*);
                    while (*str) {
                        *ptr++ = *str++;
                    }
                    break;
                }
                case 'c': {
                    char c = va_arg(args, int);
                    *ptr++ = c;
                    break;
                }
                case '%': {
                    *ptr++ = '%';
                    break;
                }
                default: {
                    *ptr++ = '%';
                    *ptr++ = *format;
                    break;
                }
            }
        } else {
            *ptr++ = *format;
        }
        format++;
    }
    
    *ptr = '\0';
    
    // Output to enabled channels
    if (debug_console_enabled) {
        kprintf("%s", buffer);
    }
    
    if (debug_serial_enabled) {
        serial_puts(buffer);
    }
    
    va_end(args);
}

// Debug print with level checking
void debug_print(debug_level_t level, const char* format, ...) {
    if (level < current_debug_level) {
        return;
    }
    
    // Add level prefix
    const char* level_prefix;
    switch (level) {
        case DEBUG_TRACE:   level_prefix = "[TRACE] "; break;
        case DEBUG_DEBUG:   level_prefix = "[DEBUG] "; break;
        case DEBUG_INFO:    level_prefix = "[INFO]  "; break;
        case DEBUG_WARN:    level_prefix = "[WARN]  "; break;
        case DEBUG_ERROR:   level_prefix = "[ERROR] "; break;
        case DEBUG_FATAL:   level_prefix = "[FATAL] "; break;
        default:            level_prefix = "[????]  "; break;
    }
    
    if (debug_console_enabled) {
        kprintf("%s", level_prefix);
    }
    
    if (debug_serial_enabled) {
        serial_puts(level_prefix);
    }
    
    // Print the actual message
    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    char* ptr = buffer;
    
    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'd': {
                    int value = va_arg(args, int);
                    char num_buffer[32];
                    itoa(value, num_buffer, 10);
                    char* num_ptr = num_buffer;
                    while (*num_ptr) {
                        *ptr++ = *num_ptr++;
                    }
                    break;
                }
                case 'x': {
                    unsigned int value = va_arg(args, unsigned int);
                    char num_buffer[32];
                    itoa(value, num_buffer, 16);
                    char* num_ptr = num_buffer;
                    while (*num_ptr) {
                        *ptr++ = *num_ptr++;
                    }
                    break;
                }
                case 'p': {
                    void* ptr_val = va_arg(args, void*);
                    uintptr_t addr = (uintptr_t)ptr_val;
                    *ptr++ = '0';
                    *ptr++ = 'x';
                    char num_buffer[32];
                    itoa(addr, num_buffer, 16);
                    char* num_ptr = num_buffer;
                    while (*num_ptr) {
                        *ptr++ = *num_ptr++;
                    }
                    break;
                }
                case 's': {
                    char* str = va_arg(args, char*);
                    if (str) {
                        while (*str) {
                            *ptr++ = *str++;
                        }
                    } else {
                        char* null_str = "(null)";
                        while (*null_str) {
                            *ptr++ = *null_str++;
                        }
                    }
                    break;
                }
                case 'c': {
                    char c = va_arg(args, int);
                    *ptr++ = c;
                    break;
                }
                case '%': {
                    *ptr++ = '%';
                    break;
                }
                default: {
                    *ptr++ = '%';
                    *ptr++ = *format;
                    break;
                }
            }
        } else {
            *ptr++ = *format;
        }
        format++;
    }
    
    *ptr = '\0';
    
    // Output to enabled channels
    if (debug_console_enabled) {
        kprintf("%s", buffer);
    }
    
    if (debug_serial_enabled) {
        serial_puts(buffer);
    }
    
    va_end(args);
}

// Memory dump utility
void debug_dump_memory(const void* addr, size_t size) {
    const uint8_t* bytes = (const uint8_t*)addr;
    
    debug_print(DEBUG_DEBUG, "Memory dump at %p (%d bytes):\n", addr, size);
    
    for (size_t i = 0; i < size; i += 16) {
        debug_printf("%08x: ", (uintptr_t)addr + i);
        
        // Print hex bytes
        for (size_t j = 0; j < 16 && (i + j) < size; j++) {
            debug_printf("%02x ", bytes[i + j]);
        }
        
        // Pad with spaces if needed
        for (size_t j = size - i; j < 16; j++) {
            debug_printf("   ");
        }
        
        debug_printf(" |");
        
        // Print ASCII representation
        for (size_t j = 0; j < 16 && (i + j) < size; j++) {
            uint8_t byte = bytes[i + j];
            if (byte >= 32 && byte <= 126) {
                debug_printf("%c", byte);
            } else {
                debug_printf(".");
            }
        }
        
        debug_printf("|\n");
    }
}

// Stack trace utility (basic implementation)
void debug_stack_trace(void) {
    debug_print(DEBUG_DEBUG, "Stack trace:\n");
    
    // Get current stack and base pointers
    uintptr_t* rbp;
    asm volatile ("mov %%rbp, %0" : "=r"(rbp));
    
    for (int i = 0; i < 10 && rbp; i++) {
        uintptr_t return_addr = *(rbp + 1);
        debug_print(DEBUG_DEBUG, "  #%d: %p\n", i, (void*)return_addr);
        rbp = (uintptr_t*)*rbp;
        
        // Sanity check to prevent infinite loops
        if ((uintptr_t)rbp < 0x1000 || (uintptr_t)rbp > 0xFFFFFFFFFFFFFFFF) {
            break;
        }
    }
}

// Assert implementation
void debug_assert_fail(const char* expr, const char* file, int line, const char* func) {
    debug_print(DEBUG_FATAL, "ASSERTION FAILED: %s\n", expr);
    debug_print(DEBUG_FATAL, "  File: %s:%d\n", file, line);
    debug_print(DEBUG_FATAL, "  Function: %s\n", func);
    debug_stack_trace();
    
    // Trigger kernel panic
    asm volatile ("cli; hlt");
    __builtin_unreachable();
}