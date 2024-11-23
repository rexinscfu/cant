#ifndef CANT_PLATFORM_WIFI_H
#define CANT_PLATFORM_WIFI_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char* ssid;
    char* password;
    uint8_t security_type;
    bool use_dhcp;
    char* static_ip;
    char* subnet_mask;
    char* gateway;
} WiFiInit_t;

bool WiFi_Init(const WiFiInit_t* params);
void WiFi_Deinit(void);
bool WiFi_Connect(void);
bool WiFi_Disconnect(void);
bool WiFi_GetStatus(int8_t* signal_strength, uint8_t* channel);

// Data transmission
bool WiFi_Send(const uint8_t* data, uint32_t length);
bool WiFi_Receive(uint8_t* data, uint32_t* length);

#endif // CANT_PLATFORM_WIFI_H 