#ifndef CANT_DIAG_SYSTEM_H
#define CANT_DIAG_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include "diag_transport.h"
#include "memory_manager.h"
#include "session_manager.h"
#include "service_router.h"
#include "data_manager.h"
#include "security_manager.h"
#include "routine_manager.h"
#include "comm_manager.h"

// Diagnostic System Configuration
typedef struct {
    DiagTransportConfig transport_config;
    MemoryManagerConfig memory_config;
    SessionManagerConfig session_config;
    ServiceRouterConfig router_config;
    DataManagerConfig data_config;
    SecurityManagerConfig security_config;
    RoutineManagerConfig routine_config;
    CommManagerConfig comm_config;
    void (*system_error_callback)(uint32_t error_code);
} DiagSystemConfig;

// Diagnostic System API
bool Diag_System_Init(const DiagSystemConfig* config);
void Diag_System_DeInit(void);
void Diag_System_Process(void);
bool Diag_System_HandleRequest(const uint8_t* data, uint16_t length);
bool Diag_System_IsReady(void);
uint32_t Diag_System_GetLastError(void);

#endif // CANT_DIAG_SYSTEM_H 