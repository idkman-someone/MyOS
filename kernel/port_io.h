#ifndef PORT_IO_H
#define PORT_IO_H

#include <stdint.h>

// 8-bit port I/O operations
uint8_t inb(uint16_t port);
void outb(uint16_t port, uint8_t data);

// 16-bit port I/O operations
uint16_t inw(uint16_t port);
void outw(uint16_t port, uint16_t data);

// 32-bit port I/O operations
uint32_t inl(uint16_t port);
void outl(uint16_t port, uint32_t data);

// I/O delay for older hardware compatibility
void io_wait(void);

// String I/O operations (for bulk data transfer)
void insb(uint16_t port, void* buffer, uint32_t count);
void outsb(uint16_t port, const void* buffer, uint32_t count);
void insw(uint16_t port, void* buffer, uint32_t count);
void outsw(uint16_t port, const void* buffer, uint32_t count);
void insl(uint16_t port, void* buffer, uint32_t count);
void outsl(uint16_t port, const void* buffer, uint32_t count);

// Common port definitions
#define COM1_PORT           0x3F8   // Serial port 1
#define COM2_PORT           0x2F8   // Serial port 2
#define KEYBOARD_DATA_PORT  0x60    // Keyboard data
#define KEYBOARD_CMD_PORT   0x64    // Keyboard command
#define PIC1_COMMAND        0x20    // Primary PIC command
#define PIC1_DATA           0x21    // Primary PIC data
#define PIC2_COMMAND        0xA0    // Secondary PIC command
#define PIC2_DATA           0xA1    // Secondary PIC data
#define PIT_CHANNEL0        0x40    // PIT channel 0
#define PIT_CHANNEL1        0x41    // PIT channel 1
#define PIT_CHANNEL2        0x42    // PIT channel 2
#define PIT_COMMAND         0x43    // PIT command port
#define VGA_MISC_WRITE      0x3C2   // VGA miscellaneous output
#define VGA_SEQ_INDEX       0x3C4   // VGA sequencer index
#define VGA_SEQ_DATA        0x3C5   // VGA sequencer data
#define VGA_CRTC_INDEX      0x3D4   // VGA CRTC index
#define VGA_CRTC_DATA       0x3D5   // VGA CRTC data

#endif // PORT_IO_H