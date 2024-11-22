#ifndef CANT_DIAG_SYSTEM_H
#define CANT_DIAG_SYSTEM_H

#include "logging/diag_logger.h"
#include "session/session_fsm.h"
#include "security/security_manager.h"
#include "resource/resource_manager.h"
#include "timing/timing_monitor.h"
#include "performance/perf_monitor.h"
#include "data/diag_data_manager.h"
#include "config/config_manager.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    LoggerConfig logger;
    SessionFSMConfig session;
    SecurityConfig security;
    ResourceConfig resource;
    TimingConfig timing;
    PerfConfig performance;
    DiagDataConfig data;
    ConfigManagerConfig config;
} DiagSystemConfig;

bool DiagSystem_Init(const DiagSystemConfig* config);
void DiagSystem_Deinit(void);
void DiagSystem_Process(void);

// System status and health
typedef struct {
    uint32_t active_sessions;
    uint32_t security_violations;
    uint32_t resource_warnings;
    uint32_t timing_violations;
    uint32_t error_count;
    uint32_t uptime_seconds;
} DiagSystemStatus;

void DiagSystem_GetStatus(DiagSystemStatus* status);
bool DiagSystem_IsHealthy(void);

// Error handling
typedef enum {
    DIAG_ERROR_NONE = 0,
    DIAG_ERROR_INITIALIZATION,
    DIAG_ERROR_CONFIGURATION,
    DIAG_ERROR_RESOURCE,
    DIAG_ERROR_SECURITY,
    DIAG_ERROR_TIMING,
    DIAG_ERROR_COMMUNICATION
} DiagErrorCode;

const char* DiagSystem_GetLastError(void);
DiagErrorCode DiagSystem_GetLastErrorCode(void);

#endif // CANT_DIAG_SYSTEM_H 