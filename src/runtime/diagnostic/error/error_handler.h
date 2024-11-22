#ifndef CANT_ERROR_HANDLER_H
#define CANT_ERROR_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    ERROR_SEVERITY_INFO = 0,
    ERROR_SEVERITY_WARNING,
    ERROR_SEVERITY_ERROR,
    ERROR_SEVERITY_FATAL
} ErrorSeverity;

typedef struct {
    uint32_t error_code;
    ErrorSeverity severity;
    const char* module;
    const char* description;
    uint32_t timestamp;
    void* context;
} ErrorInfo;

typedef void (*ErrorCallback)(const ErrorInfo* error);

typedef struct {
    bool enable_callbacks;
    bool log_errors;
    bool auto_reset;
    uint32_t max_stored_errors;
    ErrorCallback global_callback;
} ErrorHandlerConfig;

bool Error_Init(const ErrorHandlerConfig* config);
void Error_Deinit(void);

void Error_Report(uint32_t error_code, ErrorSeverity severity,
                 const char* module, const char* description);
                 
void Error_ReportWithContext(uint32_t error_code, ErrorSeverity severity,
                           const char* module, const char* description,
                           void* context);

uint32_t Error_GetCount(ErrorSeverity severity);
bool Error_GetLastError(ErrorInfo* error);
void Error_ClearAll(void);

#endif // CANT_ERROR_HANDLER_H 