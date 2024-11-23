#ifndef CANT_PLATFORM_ETHERNET_H
#define CANT_PLATFORM_ETHERNET_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char* mac_address;
    bool dhcp_enabled;
    char* static_ip;
    char* subnet_mask;
    char* gateway;
    char* dns_server;
} EthernetInit_t;

bool Ethernet_Init(const EthernetInit_t* params);
void Ethernet_Deinit(void);
bool Ethernet_Start(void);
bool Ethernet_Stop(void);
bool Ethernet_GetStatus(uint32_t* link_speed, bool* link_up);

// Data transmission
bool Ethernet_Send(const uint8_t* data, uint32_t length);
bool Ethernet_Receive(uint8_t* data, uint32_t* length);

#endif // CANT_PLATFORM_ETHERNET_H 