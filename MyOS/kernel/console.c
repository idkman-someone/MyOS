#include "console.h"
#include "port_io.h"
#include <stdarg.h>

// VGA text mode constants
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

// Console state
static uint16_t* vga_buffer = (uint16_t*)VGA_MEMORY;
static size_t vga_row = 0;
static size_t vga_column = 0;
static uint8_t vga_color = 0x07; // Light grey on black

// Make a VGA entry
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

// Make a VGA color
static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}

void console_init(void) {
    vga_row = 0;
    vga_column = 0;
    vga_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    // Clear screen
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            vga_buffer[index] = vga_entry(' ', vga_color);
        }
    }
}

void console_set_color(uint8_t color) {
    vga_color = color;
}

void console_putchar(char c) {
    if (c == '\n') {
        vga_column = 0;
        if (++vga_row == VGA_HEIGHT) {
            console_scroll();
        }
    } else if (c == '\r') {
        vga_column = 0;
    } else if (c == '\t') {
        vga_column = (vga_column + 8) & ~(8 - 1);
        if (vga_column >= VGA_WIDTH) {
            vga_column = 0;
            if (++vga_row == VGA_HEIGHT) {
                console_scroll();
            }
        }
    } else if (c >= 32) { // Printable characters
        console_putentryat(c, vga_color, vga_column, vga_row);
        if (++vga_column == VGA_WIDTH) {
            vga_column = 0;
            if (++vga_row == VGA_HEIGHT) {
                console_scroll();
            }
        }
    }
}

void console_putentryat(char c, uint8_t color, size_t x, size_t y) {
    const size_t index = y * VGA_WIDTH + x;
    vga_buffer[index] = vga_entry(c, color);
}

void console_scroll(void) {
    // Move all lines up by one
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[(y - 1) * VGA_WIDTH + x] = vga_buffer[y * VGA_WIDTH + x];
        }
    }
    
    // Clear the last line
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', vga_color);
    }
    
    vga_row = VGA_HEIGHT - 1;
}

void console_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        console_putchar(data[i]);
    }
}

void console_writestring(const char* data) {
    const char* p = data;
    while (*p) {
        console_putchar(*p++);
    }
}

// Simple string functions
static size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

static void reverse_string(char* str, int length) {
    int start = 0;
    int end = length - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

static int int_to_string(int num, char* str, int base) {
    int i = 0;
    bool is_negative = false;
    
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return i;
    }
    
    if (num < 0 && base == 10) {
        is_negative = true;
        num = -num;
    }
    
    while (num != 0) {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }
    
    if (is_negative) {
        str[i++] = '-';
    }
    
    str[i] = '\0';
    reverse_string(str, i);
    return i;
}

static int uint_to_string(unsigned int num, char* str, int base) {
    int i = 0;
    
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return i;
    }
    
    while (num != 0) {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }
    
    str[i] = '\0';
    reverse_string(str, i);
    return i;
}

// Main printf implementation
void kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char buffer[64];
    const char* p = format;
    
    while (*p) {
        if (*p == '%' && *(p + 1)) {
            p++; // Skip '%'
            
            switch (*p) {
                case 'c': {
                    char c = (char)va_arg(args, int);
                    console_putchar(c);
                    break;
                }
                case 's': {
                    const char* s = va_arg(args, const char*);
                    if (s) {
                        console_writestring(s);
                    } else {
                        console_writestring("(null)");
                    }
                    break;
                }
                case 'd':
                case 'i': {
                    int num = va_arg(args, int);
                    int_to_string(num, buffer, 10);
                    console_writestring(buffer);
                    break;
                }
                case 'u': {
                    unsigned int num = va_arg(args, unsigned int);
                    uint_to_string(num, buffer, 10);
                    console_writestring(buffer);
                    break;
                }
                case 'x': {
                    unsigned int num = va_arg(args, unsigned int);
                    uint_to_string(num, buffer, 16);
                    console_writestring(buffer);
                    break;
                }
                case 'X': {
                    unsigned int num = va_arg(args, unsigned int);
                    uint_to_string(num, buffer, 16);
                    // Convert to uppercase
                    for (char* ch = buffer; *ch; ch++) {
                        if (*ch >= 'a' && *ch <= 'f') {
                            *ch = *ch - 'a' + 'A';
                        }
                    }
                    console_writestring(buffer);
                    break;
                }
                case 'p': {
                    void* ptr = va_arg(args, void*);
                    console_writestring("0x");
                    uint_to_string((uintptr_t)ptr, buffer, 16);
                    console_writestring(buffer);
                    break;
                }
                case '%': {
                    console_putchar('%');
                    break;
                }
                default: {
                    console_putchar('%');
                    console_putchar(*p);
                    break;
                }
            }
        } else {
            console_putchar(*p);
        }
        p++;
    }
    
    va_end(args);
}