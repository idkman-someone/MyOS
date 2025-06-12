// drivers/wifi.h - WiFi Driver Header
#ifndef WIFI_H
#define WIFI_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_SSID_LENGTH 32
#define MAX_PASSWORD_LENGTH 64

// WiFi network information structure
typedef struct {
    char ssid[MAX_SSID_LENGTH];
    int signal_strength;    // Signal strength in dBm
    int channel;           // WiFi channel (1-14)
    bool encrypted;        // Whether network is encrypted
    uint8_t bssid[6];     // MAC address of access point
} wifi_network_t;

// WiFi connection status
typedef struct {
    bool connected;
    char ssid[MAX_SSID_LENGTH];
    int signal_strength;
    int channel;
    uint32_t ip_address;
    uint8_t mac_address[6];
} wifi_status_t;

// WiFi security types
typedef enum {
    WIFI_SECURITY_OPEN,
    WIFI_SECURITY_WEP,
    WIFI_SECURITY_WPA,
    WIFI_SECURITY_WPA2,
    WIFI_SECURITY_WPA3
} wifi_security_t;

// WiFi driver functions
int wifi_init(void);
int wifi_scan(void);
int wifi_get_networks(wifi_network_t* networks, int max_count);
int wifi_connect(const char* ssid, const char* password);
int wifi_disconnect(void);
int wifi_get_status(wifi_status_t* status);

// Internal functions
static void simulate_wifi_scan_results(void);

#endif // WIFI_H