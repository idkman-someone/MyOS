// drivers/ethernet.c - Ethernet Driver Implementation
#include "ethernet.h"
#include "../kernel.h"
#include "../port_io.h"
#include "../mm.h"
#include <string.h>

// RTL8139 register definitions (common ethernet chip)
#define RTL8139_VENDOR_ID    0x10EC
#define RTL8139_DEVICE_ID    0x8139

// Register offsets
#define REG_MAC0        0x00    // MAC address
#define REG_MAR0        0x08    // Multicast address
#define REG_RBSTART     0x30    // Receive buffer start
#define REG_CMD         0x37    // Command register
#define REG_CAPR        0x38    // Current address of packet read
#define REG_CBR         0x3A    // Current buffer address
#define REG_IMR         0x3C    // Interrupt mask register
#define REG_ISR         0x3E    // Interrupt status register
#define REG_TCR         0x40    // Transmit configuration register
#define REG_RCR         0x44    // Receive configuration register
#define REG_CONFIG1     0x52

// Command register bits
#define CMD_RESET       0x10
#define CMD_RX_ENABLE   0x08
#define CMD_TX_ENABLE   0x04

// Buffer sizes
#define RX_BUFFER_SIZE  (8192 + 16 + 1500)
#define TX_BUFFER_SIZE  1536

static bool ethernet_initialized = false;
static uint16_t io_base = 0;
static uint8_t* rx_buffer = NULL;
static uint8_t* tx_buffer = NULL;
static uint16_t rx_buffer_offset = 0;
static ethernet_stats_t stats;
static uint8_t mac_address[6];

// PCI scanning (simplified)
static uint16_t find_ethernet_device() {
    // In a real implementation, this would scan the PCI bus
    // For now, we'll assume the device is at a standard location
    
    // Try common I/O port ranges for RTL8139
    uint16_t test_ports[] = {0xC000, 0xC100, 0xD000, 0xD100, 0};
    
    for (int i = 0; test_ports[i] != 0; i++) {
        uint16_t port = test_ports[i];
        
        // Try to read from the device
        uint32_t test_val = inl(port);
        if (test_val != 0xFFFFFFFF && test_val != 0x00000000) {
            // Device might be present, do more checks
            outb(port + REG_CONFIG1, 0x00);
            if (inb(port + REG_CONFIG1) == 0x00) {
                return port;
            }
        }
    }
    
    return 0;  // Not found
}

// Initialize ethernet driver
int ethernet_init() {
    kprintf("Initializing Ethernet driver...\n");
    
    // Find ethernet device
    io_base = find_ethernet_device();
    if (io_base == 0) {
        kprintf("No Ethernet device found\n");
        return -1;
    }
    
    kprintf("Ethernet device found at I/O base 0x%x\n", io_base);
    
    // Allocate buffers
    rx_buffer = (uint8_t*)kmalloc(RX_BUFFER_SIZE);
    tx_buffer = (uint8_t*)kmalloc(TX_BUFFER_SIZE);
    
    if (!rx_buffer || !tx_buffer) {
        kprintf("Failed to allocate Ethernet buffers\n");
        if (rx_buffer) kfree(rx_buffer);
        if (tx_buffer) kfree(tx_buffer);
        return -1;
    }
    
    // Reset the device
    outb(io_base + REG_CMD, CMD_RESET);
    
    // Wait for reset to complete
    while (inb(io_base + REG_CMD) & CMD_RESET) {
        // Wait
    }
    
    // Read MAC address
    for (int i = 0; i < 6; i++) {
        mac_address[i] = inb(io_base + REG_MAC0 + i);
    }
    
    kprintf("MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
            mac_address[0], mac_address[1], mac_address[2],
            mac_address[3], mac_address[4], mac_address[5]);
    
    // Set up receive buffer
    outl(io_base + REG_RBSTART, (uint32_t)rx_buffer);
    
    // Configure receive and transmit
    outl(io_base + REG_RCR, 0x0000000F);  // Accept all packets
    outl(io_base + REG_TCR, 0x03000000);  // Normal transmit mode
    
    // Enable interrupts
    outw(io_base + REG_IMR, 0x0005);  // RX OK and TX OK interrupts
    
    // Enable RX and TX
    outb(io_base + REG_CMD, CMD_RX_ENABLE | CMD_TX_ENABLE);
    
    // Initialize statistics
    memset(&stats, 0, sizeof(stats));
    
    ethernet_initialized = true;
    kprintf("Ethernet driver initialized successfully\n");
    
    return 0;
}

// Send ethernet packet
int ethernet_send_packet(const void* data, uint16_t length) {
    if (!ethernet_initialized || !data || length == 0 || length > 1500) {
        return -1;
    }
    
    // Copy data to transmit buffer
    memcpy(tx_buffer, data, length);
    
    // Pad packet if necessary
    if (length < 60) {
        memset(tx_buffer + length, 0, 60 - length);
        length = 60;
    }
    
    // Find available transmit descriptor (simplified - using TSD0)
    uint32_t tsd = inl(io_base + 0x10);  // TSD0
    if (tsd & 0x8000) {  // Check if previous transmission is complete
        // Set up transmission
        outl(io_base + 0x20, (uint32_t)tx_buffer);  // TSAD0
        outl(io_base + 0x10, length);               // TSD0
        
        stats.packets_sent++;
        stats.bytes_sent += length;
        
        return 0;
    }
    
    return -1;  // Transmit busy
}

// Receive ethernet packet
int ethernet_receive_packet(void* buffer, uint16_t max_length) {
    if (!ethernet_initialized || !buffer || max_length == 0) {
        return -1;
    }
    
    // Check if there's data available
    uint16_t capr = inw(io_base + REG_CAPR);
    uint16_t cbr = inw(io_base + REG_CBR);
    
    if (capr == cbr) {
        return 0;  // No data available
    }
    
    // Read packet header
    uint16_t* header = (uint16_t*)(rx_buffer + rx_buffer_offset);
    uint16_t status = header[0];
    uint16_t length = header[1];
    
    // Validate packet
    if (!(status & 0x01) || length < 60 || length > 1518) {
        // Invalid packet, skip it
        rx_buffer_offset = (rx_buffer_offset + length + 4 + 3) & ~3;
        if (rx_buffer_offset >= RX_BUFFER_SIZE) {
            rx_buffer_offset = 0;
        }
        return -1;
    }
    
    // Copy packet data
    uint16_t copy_length = (length < max_length) ? length : max_length;
    memcpy(buffer, rx_buffer + rx_buffer_offset + 4, copy_length);
    
    // Update buffer offset
    rx_buffer_offset = (rx_buffer_offset + length + 4 + 3) & ~3;
    if (rx_buffer_offset >= RX_BUFFER_SIZE) {
        rx_buffer_offset = 0;
    }
    
    // Update CAPR register
    outw(io_base + REG_CAPR, rx_buffer_offset - 16);
    
    stats.packets_received++;
    stats.bytes_received += copy_length;
    
    return copy_length;
}

// Get ethernet statistics
void ethernet_get_stats(ethernet_stats_t* out_stats) {
    if (out_stats) {
        memcpy(out_stats, &stats, sizeof(ethernet_stats_t));
    }
}

// Get MAC address
void ethernet_get_mac_address(uint8_t* mac) {
    if (mac && ethernet_initialized) {
        memcpy(mac, mac_address, 6);
    }
}

// Set promiscuous mode
int ethernet_set_promiscuous(bool enable) {
    if (!ethernet_initialized) {
        return -1;
    }
    
    uint32_t rcr = inl(io_base + REG_RCR);
    if (enable) {
        rcr |= 0x01;  // Accept all packets
    } else {
        rcr &= ~0x01;
    }
    outl(io_base + REG_RCR, rcr);
    
    return 0;
}

// Check link status
bool ethernet_link_up() {
    if (!ethernet_initialized) {
        return false;
    }
    
    // Read media status (simplified check)
    uint8_t media_status = inb(io_base + 0x58);
    return (media_status & 0x04) != 0;  // LINKB bit
}

// Ethernet interrupt handler
void ethernet_interrupt_handler() {
    if (!ethernet_initialized) {
        return;
    }
    
    uint16_t isr = inw(io_base + REG_ISR);
    
    // Clear interrupts
    outw(io_base + REG_ISR, isr);
    
    if (isr & 0x01) {  // RX OK
        // Packet received - handled by polling for now
    }
    
    if (isr & 0x04) {  // TX OK
        // Packet transmitted successfully
    }
    
    if (isr & 0x02) {  // RX Error
        stats.rx_errors++;
    }
    
    if (isr & 0x08) {  // TX Error
        stats.tx_errors++;
    }
}