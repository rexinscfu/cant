#ifndef CANT_PLATFORM_CAN_H
#define CANT_PLATFORM_CAN_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t id;
    uint32_t bitrate;
    bool extended_id;
    bool fd_enabled;
    uint8_t data_bitrate;
} CANInit_t;

typedef struct {
    uint32_t id;
    uint8_t* data;
    uint8_t length;
    bool extended_id;
    bool rtr;
    bool fd_frame;
    uint8_t dlc;
} CANFrame_t;

bool CAN_Init(const CANInit_t* params);
void CAN_Deinit(void);
bool CAN_Start(void);
bool CAN_Stop(void);
bool CAN_GetStatus(uint32_t* error_count, bool* bus_off);

// Frame transmission
bool CAN_SendFrame(const CANFrame_t* frame);
bool CAN_ReceiveFrame(CANFrame_t* frame);

// Filter management
bool CAN_SetFilter(uint32_t id, uint32_t mask);
void CAN_ClearFilters(void);

#endif // CANT_PLATFORM_CAN_H 