#ifndef CANT_NET_INTERFACE_H
#define CANT_NET_INTERFACE_H

#include "net_core.h"
#include <stdint.h>
#include <stdbool.h>

// Ethernet specific configuration
typedef struct {
    char mac_address[18];
    bool dhcp_enabled;
    char static_ip[16];
    char subnet_mask[16];
    char gateway[16];
    char dns_server[16];
} EthernetConfig;

// WiFi specific configuration
typedef struct {
    char ssid[32];
    char password[64];
    uint8_t security_type;
    bool use_dhcp;
    char static_ip[16];
    char subnet_mask[16];
    char gateway[16];
} WiFiConfig;

// Cellular specific configuration
typedef struct {
    char apn[32];
    char username[32];
    char password[32];
    uint8_t network_type;
    bool roaming_enabled;
    uint16_t connection_timeout;
} CellularConfig;

// CAN specific configuration
typedef struct {
    uint32_t id;
    uint32_t bitrate;
    bool extended_id;
    bool fd_enabled;
    uint8_t data_bitrate;
} CANInterfaceConfig;

// Interface Functions
bool NetInterface_ConnectEthernet(const NetInterfaceConfig* config);
bool NetInterface_DisconnectEthernet(const NetInterfaceConfig* config);

bool NetInterface_ConnectWiFi(const NetInterfaceConfig* config);
bool NetInterface_DisconnectWiFi(const NetInterfaceConfig* config);

bool NetInterface_ConnectCellular(const NetInterfaceConfig* config);
bool NetInterface_DisconnectCellular(const NetInterfaceConfig* config);

bool NetInterface_ConnectCAN(const NetInterfaceConfig* config);
bool NetInterface_DisconnectCAN(const NetInterfaceConfig* config);

// Interface Status Functions
bool NetInterface_GetEthernetStatus(uint32_t* link_speed, bool* link_up);
bool NetInterface_GetWiFiStatus(int8_t* signal_strength, uint8_t* channel);
bool NetInterface_GetCellularStatus(int8_t* signal_strength, uint8_t* network_type);
bool NetInterface_GetCANStatus(uint32_t* error_count, bool* bus_off);

#endif // CANT_NET_INTERFACE_H 