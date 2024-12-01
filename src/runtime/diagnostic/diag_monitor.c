#include "diag_monitor.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

#define MAX_LINE_LENGTH 512

typedef struct {
    DiagMonitorConfig config;
    FILE* file;
    bool enabled;
    bool initialized;
} MonitorContext;

static MonitorContext monitor;

// ANSI color codes for console output
static const char* level_colors[] = {
    "\x1B[0m",     // NONE    - Reset
    "\x1B[31m",    // ERROR   - Red
    "\x1B[33m",    // WARNING - Yellow
    "\x1B[32m",    // INFO    - Green
    "\x1B[36m",    // DEBUG   - Cyan
    "\x1B[35m"     // TRACE   - Magenta
};

static const char* level_strings[] = {
    "NONE", "ERROR", "WARNING", "INFO", "DEBUG", "TRACE"
};

static const char* category_strings[] = {
    "CORE", "SESSION", "SECURITY", "STATE", "TIMER", "PARSER", "ERROR", "CUSTOM"
};

static void format_timestamp(char* buffer, size_t size, uint32_t timestamp) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    
    snprintf(buffer, size, "%02d:%02d:%02d.%03u",
             tm_info->tm_hour,
             tm_info->tm_min,
             tm_info->tm_sec,
             timestamp % 1000);
}

static void write_console(const DiagLogEntry* entry) {
    char line[MAX_LINE_LENGTH];
    char timestamp[32];
    
    if (monitor.config.config.console.timestamp) {
        format_timestamp(timestamp, sizeof(timestamp), entry->timestamp);
    }
    
    if (monitor.config.config.console.color_output) {
        snprintf(line, sizeof(line), "%s%s [%s] [%s] %s\x1B[0m\n",
                 monitor.config.config.console.timestamp ? timestamp : "",
                 level_colors[entry->level],
                 level_strings[entry->level],
                 category_strings[entry->category],
                 entry->message);
    } else {
        snprintf(line, sizeof(line), "%s[%s] [%s] %s\n",
                 monitor.config.config.console.timestamp ? timestamp : "",
                 level_strings[entry->level],
                 category_strings[entry->category],
                 entry->message);
    }
    
    printf("%s", line);
    
    // Print hex dump if available
    if (entry->data_length > 0) {
        printf("Data (%u bytes):\n", entry->data_length);
        for (uint32_t i = 0; i < entry->data_length; i++) {
            printf("%02X ", entry->data[i]);
            if ((i + 1) % 16 == 0) printf("\n");
        }
        if (entry->data_length % 16 != 0) printf("\n");
    }
}

static void write_file(const DiagLogEntry* entry) {
    if (!monitor.file) return;
    
    char timestamp[32];
    format_timestamp(timestamp, sizeof(timestamp), entry->timestamp);
    
    fprintf(monitor.file, "%s [%s] [%s] %s\n",
            timestamp,
            level_strings[entry->level],
            category_strings[entry->category],
            entry->message);
    
    if (entry->data_length > 0) {
        fprintf(monitor.file, "Data (%u bytes):\n", entry->data_length);
        for (uint32_t i = 0; i < entry->data_length; i++) {
            fprintf(monitor.file, "%02X ", entry->data[i]);
            if ((i + 1) % 16 == 0) fprintf(monitor.file, "\n");
        }
        if (entry->data_length % 16 != 0) fprintf(monitor.file, "\n");
    }
    
    fflush(monitor.file);
}

bool DiagMonitor_Init(const DiagMonitorConfig* config) {
    if (!config || monitor.initialized) return false;
    
    memset(&monitor, 0, sizeof(MonitorContext));
    memcpy(&monitor.config, config, sizeof(DiagMonitorConfig));
    
    if (config->type == MONITOR_TYPE_FILE) {
        monitor.file = fopen(config->config.file.filename,
                           config->config.file.append ? "a" : "w");
        if (!monitor.file) return false;
    }
    
    monitor.enabled = true;
    monitor.initialized = true;
    
    // Register with logger
    DiagLogger_RegisterCallback(DiagMonitor_HandleLog, NULL);
    
    return true;
}

void DiagMonitor_Deinit(void) {
    if (!monitor.initialized) return;
    
    DiagLogger_UnregisterCallback(DiagMonitor_HandleLog);
    
    if (monitor.file) {
        fclose(monitor.file);
        monitor.file = NULL;
    }
    
    memset(&monitor, 0, sizeof(MonitorContext));
}

void DiagMonitor_Enable(void) {
    if (monitor.initialized) {
        monitor.enabled = true;
    }
}

void DiagMonitor_Disable(void) {
    if (monitor.initialized) {
        monitor.enabled = false;
    }
}

bool DiagMonitor_IsEnabled(void) {
    return monitor.initialized && monitor.enabled;
}

void DiagMonitor_HandleLog(const DiagLogEntry* entry, void* context) {
    (void)context;
    
    if (!monitor.initialized || !monitor.enabled || !entry) return;
    
    switch (monitor.config.type) {
        case MONITOR_TYPE_FILE:
            write_file(entry);
            break;
            
        case MONITOR_TYPE_CONSOLE:
            write_console(entry);
            break;
            
        case MONITOR_TYPE_CUSTOM:
            if (monitor.config.config.custom.write) {
                char line[MAX_LINE_LENGTH];
                snprintf(line, sizeof(line), "[%s] [%s] %s\n",
                         level_strings[entry->level],
                         category_strings[entry->category],
                         entry->message);
                monitor.config.config.custom.write(line,
                    monitor.config.config.custom.context);
            }
            break;
    }
} 