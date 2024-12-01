#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

bool MessageHandler_Init(void);
bool MessageHandler_Send(uint8_t* data, uint32_t len);
void MessageHandler_Process(void);
void MessageHandler_HandleResponse(uint8_t* data, uint32_t len);
uint32_t get_msg_count(void);

typedef struct {
    uint32_t rx_count;
    uint32_t tx_count;
    uint32_t error_count;
    uint32_t timeout_count;
} MessageStats;

void MessageHandler_GetStats(MessageStats* stats);
void MessageHandler_ResetStats(void);

#endif 