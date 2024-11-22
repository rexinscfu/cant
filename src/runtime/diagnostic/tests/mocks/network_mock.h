#ifndef CANT_NETWORK_MOCK_H
#define CANT_NETWORK_MOCK_H

#include <stdint.h>
#include <stdbool.h>

void Network_Mock_Init(void);
void Network_Mock_Deinit(void);

void Network_Mock_SetBaudrate(uint32_t baudrate);
void Network_Mock_SetNodeAddress(uint8_t address);
void Network_Mock_SetActiveNodeCount(uint8_t count);
void Network_Mock_SetResponseTimes(const uint16_t* times);
void Network_Mock_SetErrorCounts(const uint8_t* errors);
void Network_Mock_SetTestComplete(bool complete);
void Network_Mock_SetNetworkStatus(uint8_t status);

uint32_t Network_Mock_GetBaudrate(void);
uint8_t Network_Mock_GetNodeAddress(void);
uint8_t Network_Mock_GetActiveNodeCount(void);
void Network_Mock_GetResponseTimes(uint16_t* times);
void Network_Mock_GetErrorCounts(uint8_t* errors);
bool Network_Mock_IsTestComplete(void);
uint8_t Network_Mock_GetNetworkStatus(void);

#endif // CANT_NETWORK_MOCK_H 