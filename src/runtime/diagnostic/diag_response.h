#ifndef CANT_DIAG_RESPONSE_H
#define DIAG_RESPONSE_H

#include "diag_core.h"

// Response handler states
typedef enum {
    RESP_STATE_IDLE,
    RESP_STATE_WAITING,
    RESP_STATE_RECEIVED,
    RESP_STATE_TIMEOUT,
    RESP_STATE_ERROR
} ResponseState;

// Response queue entry
typedef struct {
    uint32_t msg_id;
    uint32_t timestamp;
    DiagResponse response;
    ResponseState state;
    DiagResponseCallback callback;
    void* context;
    bool active;
} ResponseQueueEntry;

// Response handler functions
bool DiagResponse_Init(void);
void DiagResponse_Deinit(void);

bool DiagResponse_QueueResponse(uint32_t msg_id, DiagResponseCallback callback, void* context);
bool DiagResponse_HandleResponse(const DiagResponse* response);
void DiagResponse_HandleTimeout(uint32_t msg_id);

ResponseState DiagResponse_GetState(uint32_t msg_id);
uint32_t DiagResponse_GetPendingCount(void);

#endif // CANT_DIAG_RESPONSE_H 