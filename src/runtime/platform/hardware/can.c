#include "can.h"
#include "../../diagnostic/logging/diag_logger.h"
#include <string.h>

typedef struct {
    CANInit_t config;
    bool initialized;
    bool running;
    uint32_t error_count;
    bool bus_off_state;
    uint32_t filters[16];
    uint32_t filter_masks[16];
    uint8_t filter_count;
} CANContext;

static CANContext can_ctx;

bool CAN_Init(const CANInit_t* params) {
    if (!params) {
        return false;
    }

    memset(&can_ctx, 0, sizeof(CANContext));
    memcpy(&can_ctx.config, params, sizeof(CANInit_t));

    can_ctx.initialized = true;
    Logger_Log(LOG_LEVEL_INFO, "CAN", "CAN initialized with ID: %u, Bitrate: %u", 
               params->id, params->bitrate);
    return true;
}

void CAN_Deinit(void) {
    if (!can_ctx.initialized) {
        return;
    }

    if (can_ctx.running) {
        CAN_Stop();
    }

    memset(&can_ctx, 0, sizeof(CANContext));
    Logger_Log(LOG_LEVEL_INFO, "CAN", "CAN deinitialized");
}

bool CAN_Start(void) {
    if (!can_ctx.initialized || can_ctx.running) {
        return false;
    }

    can_ctx.running = true;
    can_ctx.error_count = 0;
    can_ctx.bus_off_state = false;
    Logger_Log(LOG_LEVEL_INFO, "CAN", "CAN started");
    return true;
}

bool CAN_Stop(void) {
    if (!can_ctx.initialized || !can_ctx.running) {
        return false;
    }

    can_ctx.running = false;
    Logger_Log(LOG_LEVEL_INFO, "CAN", "CAN stopped");
    return true;
}

bool CAN_GetStatus(uint32_t* error_count, bool* bus_off) {
    if (!can_ctx.initialized || !error_count || !bus_off) {
        return false;
    }

    *error_count = can_ctx.error_count;
    *bus_off = can_ctx.bus_off_state;
    return true;
}

bool CAN_SendFrame(const CANFrame_t* frame) {
    if (!can_ctx.initialized || !can_ctx.running || !frame || !frame->data) {
        return false;
    }

    Logger_Log(LOG_LEVEL_DEBUG, "CAN", "Sending frame ID: 0x%X, Length: %u", 
               frame->id, frame->length);
    return true;
}

bool CAN_ReceiveFrame(CANFrame_t* frame) {
    if (!can_ctx.initialized || !can_ctx.running || !frame) {
        return false;
    }

    // Simulate no frame available
    return false;
}

bool CAN_SetFilter(uint32_t id, uint32_t mask) {
    if (!can_ctx.initialized || can_ctx.filter_count >= 16) {
        return false;
    }

    can_ctx.filters[can_ctx.filter_count] = id;
    can_ctx.filter_masks[can_ctx.filter_count] = mask;
    can_ctx.filter_count++;

    Logger_Log(LOG_LEVEL_INFO, "CAN", "Filter added: ID=0x%X, Mask=0x%X", id, mask);
    return true;
}

void CAN_ClearFilters(void) {
    if (!can_ctx.initialized) {
        return;
    }

    memset(can_ctx.filters, 0, sizeof(can_ctx.filters));
    memset(can_ctx.filter_masks, 0, sizeof(can_ctx.filter_masks));
    can_ctx.filter_count = 0;

    Logger_Log(LOG_LEVEL_INFO, "CAN", "All filters cleared");
} 