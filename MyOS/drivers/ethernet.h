// drivers/ethernet.h - Ethernet Driver Header
#ifndef ETHERNET_H
#define ETHERNET_H

#include <stdint.h>
#include <stdbool.h>

// Ethernet frame structure
typedef struct {
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t ethertype;
    uint8_t payload[];
} __attribute__((packed)) ethernet_frame_t;

// Ethernet statistics
typedef struct {
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint32_t tx_errors;
    uint32_t rx_errors;
    uint32_t collisions;
    uint32_t dropped_packets;
} ethernet_stats_t;

// Common Ethernet types
#define ETHERTYPE_IP    0x0800
#define ETHERTYPE_ARP   0x0806
#define ETHERTYPE_IPV6  0x86DD

// Ethernet driver functions
int ethernet_init(void);
int ethernet_send_packet(const void* data, uint16_t length);
int ethernet_receive_packet(void* buffer, uint16_t max_length);
void ethernet_get_stats(ethernet_stats_t* stats);
void ethernet_get_mac_address(uint8_t* mac);
int ethernet_set_promiscuous(bool enable);
bool ethernet_link_up(void);
void ethernet_interrupt_handler(void);

#endif // ETHERNET_H