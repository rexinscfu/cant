#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*CANRxCallback)(uint32_t id, uint8_t* data, uint8_t len);

bool CAN_Init(uint32_t baudrate);
void CAN_Deinit(void);
bool CAN_Transmit(uint32_t id, uint8_t* data, uint8_t len);
void CAN_RegisterRxCallback(CANRxCallback callback);
uint32_t CAN_GetErrorStatus(void);

typedef struct {
    uint32_t rx_count;
    uint32_t tx_count;
    uint32_t error_count;
    uint8_t last_error;
    uint8_t state;
} CANStats;

void CAN_GetStats(CANStats* stats);
void CAN_ResetStats(void);

#endif 