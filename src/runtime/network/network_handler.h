#ifndef NETWORK_HANDLER_H
#define NETWORK_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t baudrate;
    uint32_t tx_id;
    uint32_t min_tx_interval;
    uint8_t block_size;
    uint8_t st_min;
} NetworkConfig;

bool NetworkHandler_Init(const NetworkConfig* config);
bool NetworkHandler_Send(uint8_t* data, uint32_t len);
void NetworkHandler_Process(void);
uint32_t NetworkHandler_GetErrorStatus(void);
bool send_diagnostic_response(uint8_t* data, uint32_t len);

#endif 