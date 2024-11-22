#ifndef CANT_DIAG_TRANSPORT_H
#define CANT_DIAG_TRANSPORT_H

#include <stdint.h>
#include <stdbool.h>
#include "uds_handler.h"

// Transport Protocol Types
typedef enum {
    DIAG_TRANSPORT_CAN,
    DIAG_TRANSPORT_ETHERNET,
    DIAG_TRANSPORT_K_LINE,
    DIAG_TRANSPORT_FLEXRAY
} DiagTransportType;

// Transport Configuration
typedef struct {
    DiagTransportType type;
    uint32_t rx_id;
    uint32_t tx_id;
    uint32_t timeout_ms;
    uint32_t block_size;
    uint32_t stmin_ms;
    void (*transport_error_callback)(uint32_t error_code);
} DiagTransportConfig;

// Transport API
bool Diag_Transport_Init(const DiagTransportConfig* config);
void Diag_Transport_DeInit(void);
bool Diag_Transport_SendResponse(const uint8_t* data, uint16_t length);
void Diag_Transport_ProcessReceived(const uint8_t* data, uint16_t length);
bool Diag_Transport_IsBusy(void);
void Diag_Transport_AbortTransmission(void);
uint32_t Diag_Transport_GetLastError(void);

#endif // CANT_DIAG_TRANSPORT_H 