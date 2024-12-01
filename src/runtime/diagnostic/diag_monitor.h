#ifndef DIAG_MONITOR_H
#define DIAG_MONITOR_H

#include "diag_logger.h"
#include <stdint.h>
#include <stdbool.h>

// Monitor types
typedef enum {
    MONITOR_TYPE_FILE,
    MONITOR_TYPE_CONSOLE,
    MONITOR_TYPE_CUSTOM
} DiagMonitorType;

// Monitor configuration
typedef struct {
    DiagMonitorType type;
    union {
        struct {
            const char* filename;
            bool append;
        } file;
        struct {
            bool color_output;
            bool timestamp;
        } console;
        struct {
            void* context;
            void (*write)(const char* text, void* context);
        } custom;
    } config;
} DiagMonitorConfig;

// Monitor functions
bool DiagMonitor_Init(const DiagMonitorConfig* config);
void DiagMonitor_Deinit(void);

void DiagMonitor_Enable(void);
void DiagMonitor_Disable(void);

bool DiagMonitor_IsEnabled(void);

// Internal callback for logger
void DiagMonitor_HandleLog(const DiagLogEntry* entry, void* context);

#endif // DIAG_MONITOR_H 