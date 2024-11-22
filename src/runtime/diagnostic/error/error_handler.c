#include "error_handler.h"
#include "../logging/diag_logger.h"
#include "../os/critical.h"
#include "../os/timer.h"
#include <string.h>

#define MAX_ERROR_HISTORY 100
#define MAX_ERROR_DESC_LENGTH 256

typedef struct {
    ErrorHandlerConfig config;
    ErrorInfo error_history[MAX_ERROR_HISTORY];
    uint32_t error_count;
    uint32_t error_index;
    uint32_t severity_counts[4]; // Counts for each severity level
    bool initialized;
} ErrorHandler;

static ErrorHandler error_handler;

bool Error_Init(const ErrorHandlerConfig* config) {
    if (!config) {
        return false;
    }

    memset(&error_handler, 0, sizeof(ErrorHandler));
    memcpy(&error_handler.config, config, sizeof(ErrorHandlerConfig));

    if (error_handler.config.max_stored_errors > MAX_ERROR_HISTORY) {
        error_handler.config.max_stored_errors = MAX_ERROR_HISTORY;
    }

    error_handler.initialized = true;
    Logger_Log(LOG_LEVEL_INFO, "ERROR", "Error handler initialized");
    return true;
}

void Error_Deinit(void) {
    Logger_Log(LOG_LEVEL_INFO, "ERROR", "Error handler deinitialized");
    memset(&error_handler, 0, sizeof(ErrorHandler));
}

void Error_Report(uint32_t error_code, ErrorSeverity severity,
                 const char* module, const char* description) {
    Error_ReportWithContext(error_code, severity, module, description, NULL);
}

void Error_ReportWithContext(uint32_t error_code, ErrorSeverity severity,
                           const char* module, const char* description,
                           void* context) {
    if (!error_handler.initialized || !module || !description || 
        severity > ERROR_SEVERITY_FATAL) {
        return;
    }

    enter_critical();

    // Create new error entry
    ErrorInfo* error = &error_handler.error_history[error_handler.error_index];
    error->error_code = error_code;
    error->severity = severity;
    error->module = module;
    error->description = description;
    error->timestamp = Timer_GetMilliseconds();
    error->context = context;

    // Update counters
    error_handler.error_count++;
    error_handler.severity_counts[severity]++;
    error_handler.error_index = (error_handler.error_index + 1) % 
                               error_handler.config.max_stored_errors;

    // Log error if enabled
    if (error_handler.config.log_errors) {
        LogLevel log_level;
        switch (severity) {
            case ERROR_SEVERITY_INFO:
                log_level = LOG_LEVEL_INFO;
                break;
            case ERROR_SEVERITY_WARNING:
                log_level = LOG_LEVEL_WARNING;
                break;
            case ERROR_SEVERITY_ERROR:
            case ERROR_SEVERITY_FATAL:
                log_level = LOG_LEVEL_ERROR;
                break;
            default:
                log_level = LOG_LEVEL_INFO;
        }

        Logger_Log(log_level, module, "Error 0x%08X: %s", error_code, description);
    }

    // Call callback if enabled
    if (error_handler.config.enable_callbacks && 
        error_handler.config.global_callback) {
        error_handler.config.global_callback(error);
    }

    // Auto reset if enabled and this is a fatal error
    if (error_handler.config.auto_reset && severity == ERROR_SEVERITY_FATAL) {
        // Implementation specific reset mechanism
        // System_Reset();
    }

    exit_critical();
}

uint32_t Error_GetCount(ErrorSeverity severity) {
    if (!error_handler.initialized || severity > ERROR_SEVERITY_FATAL) {
        return 0;
    }

    return error_handler.severity_counts[severity];
}

bool Error_GetLastError(ErrorInfo* error) {
    if (!error_handler.initialized || !error || error_handler.error_count == 0) {
        return false;
    }

    enter_critical();
    
    uint32_t last_index = (error_handler.error_index == 0) ? 
                         error_handler.config.max_stored_errors - 1 : 
                         error_handler.error_index - 1;
    
    memcpy(error, &error_handler.error_history[last_index], sizeof(ErrorInfo));
    
    exit_critical();
    return true;
}

void Error_ClearAll(void) {
    if (!error_handler.initialized) {
        return;
    }

    enter_critical();
    
    memset(error_handler.error_history, 0, 
           sizeof(ErrorInfo) * error_handler.config.max_stored_errors);
    memset(error_handler.severity_counts, 0, sizeof(error_handler.severity_counts));
    error_handler.error_count = 0;
    error_handler.error_index = 0;
    
    exit_critical();
    
    Logger_Log(LOG_LEVEL_INFO, "ERROR", "Error history cleared");
} 