#ifndef CANT_PLATFORM_CELLULAR_H
#define CANT_PLATFORM_CELLULAR_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char* apn;
    char* username;
    char* password;
    uint8_t network_type;
    bool roaming_enabled;
    uint16_t connection_timeout;
} CellularInit_t;

bool Cellular_Init(const CellularInit_t* params);
void Cellular_Deinit(void);
bool Cellular_Connect(void);
bool Cellular_Disconnect(void);
bool Cellular_GetStatus(int8_t* signal_strength, uint8_t* network_type);

// Data transmission
bool Cellular_Send(const uint8_t* data, uint32_t length);
bool Cellular_Receive(uint8_t* data, uint32_t* length);

#endif // CANT_PLATFORM_CELLULAR_H 