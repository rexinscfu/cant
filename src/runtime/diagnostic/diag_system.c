#include "diag_system.h"
#include "os/timer.h"
#include <string.h>

typedef struct {
    DiagSystemConfig config;
    DiagSystemStatus status;
    DiagErrorCode last_error_code;
    char last_error_message[256];
    uint32_t start_time;
    bool initialized;
} DiagSystem;

static DiagSystem diag_system;

static void update_system_status(void) {
    diag_system.status.uptime_seconds = 
        (Timer_GetMilliseconds() - diag_system.start_time) / 1000;
    
    diag_system.status.active_sessions = Session_FSM_GetActiveSessionCount();
    
    // Update other status fields from respective subsystems
    ResourceStats resource_stats;
    Resource_GetStats(RESOURCE_CPU, &resource_stats);
    if (resource_stats.used > resource_stats.threshold) {
        diag_system.status.resource_warnings++;
    }
}

bool DiagSystem_Init(const DiagSystemConfig* config) {
    if (!config) {
        return false;
    }

    memset(&diag_system, 0, sizeof(DiagSystem));
    memcpy(&diag_system.config, config, sizeof(DiagSystemConfig));

    // Initialize timer system first
    if (!Timer_Init()) {
        strcpy(diag_system.last_error_message, "Timer initialization failed");
        diag_system.last_error_code = DIAG_ERROR_INITIALIZATION;
        return false;
    }

    // Initialize all subsystems
    if (!Logger_Init(&config->logger)) {
        strcpy(diag_system.last_error_message, "Logger initialization failed");
        diag_system.last_error_code = DIAG_ERROR_INITIALIZATION;
        return false;
    }

    if (!Session_FSM_Init(&config->session)) {
        strcpy(diag_system.last_error_message, "Session FSM initialization failed");
        diag_system.last_error_code = DIAG_ERROR_INITIALIZATION;
        return false;
    }

    if (!Security_Init(&config->security)) {
        strcpy(diag_system.last_error_message, "Security initialization failed");
        diag_system.last_error_code = DIAG_ERROR_INITIALIZATION;
        return false;
    }

    if (!Resource_Init(&config->resource)) {
        strcpy(diag_system.last_error_message, "Resource initialization failed");
        diag_system.last_error_code = DIAG_ERROR_INITIALIZATION;
        return false;
    }

    if (!Timing_Init(&config->timing)) {
        strcpy(diag_system.last_error_message, "Timing initialization failed");
        diag_system.last_error_code = DIAG_ERROR_INITIALIZATION;
        return false;
    }

    if (!Perf_Init(&config->performance)) {
        strcpy(diag_system.last_error_message, "Performance initialization failed");
        diag_system.last_error_code = DIAG_ERROR_INITIALIZATION;
        return false;
    }

    if (!DiagData_Init(&config->data)) {
        strcpy(diag_system.last_error_message, "Diagnostic data initialization failed");
        diag_system.last_error_code = DIAG_ERROR_INITIALIZATION;
        return false;
    }

    if (!Config_Init(&config->config)) {
        strcpy(diag_system.last_error_message, "Configuration initialization failed");
        diag_system.last_error_code = DIAG_ERROR_INITIALIZATION;
        return false;
    }

    diag_system.start_time = Timer_GetMilliseconds();
    diag_system.initialized = true;

    Logger_Log(LOG_LEVEL_INFO, "SYSTEM", "Diagnostic system initialized");
    return true;
}

void DiagSystem_Deinit(void) {
    if (!diag_system.initialized) {
        return;
    }

    // Deinitialize all subsystems in reverse order
    Config_Deinit();
    DiagData_Deinit();
    Perf_Deinit();
    Timing_Deinit();
    Resource_Deinit();
    Security_Deinit();
    Session_FSM_Deinit();
    Logger_Deinit();
    Timer_Deinit();

    Logger_Log(LOG_LEVEL_INFO, "SYSTEM", "Diagnostic system deinitialized");
    memset(&diag_system, 0, sizeof(DiagSystem));
}

void DiagSystem_Process(void) {
    if (!diag_system.initialized) {
        return;
    }

    // Process all subsystems
    Timer_Process();
    Session_FSM_ProcessTimeouts();
    Security_ProcessTimeouts();
    Resource_ProcessUsage();
    Perf_ProcessMetrics();
    DiagData_ProcessCache();
    Config_ProcessAutoSave();

    // Update system status
    update_system_status();
}

void DiagSystem_GetStatus(DiagSystemStatus* status) {
    if (!diag_system.initialized || !status) {
        return;
    }

    memcpy(status, &diag_system.status, sizeof(DiagSystemStatus));
}

bool DiagSystem_IsHealthy(void) {
    if (!diag_system.initialized) {
        return false;
    }

    // Check various health indicators
    if (diag_system.status.security_violations > 0) {
        return false;
    }

    if (diag_system.status.timing_violations > 10) {
        return false;
    }

    if (diag_system.status.resource_warnings > 5) {
        return false;
    }

    if (diag_system.status.error_count > 100) {
        return false;
    }

    return true;
}

const char* DiagSystem_GetLastError(void) {
    return diag_system.last_error_message;
}

DiagErrorCode DiagSystem_GetLastErrorCode(void) {
    return diag_system.last_error_code;
} 