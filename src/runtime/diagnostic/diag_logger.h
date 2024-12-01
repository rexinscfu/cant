#ifndef DIAG_LOGGER_H
#define DIAG_LOGGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// Log levels
typedef enum {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_TRACE
} DiagLogLevel;

// Log categories
typedef enum {
    LOG_CAT_CORE = 0,
    LOG_CAT_SESSION,
    LOG_CAT_SECURITY,
    LOG_CAT_STATE,
    LOG_CAT_TIMER,
    LOG_CAT_PARSER,
    LOG_CAT_ERROR,
    LOG_CAT_CUSTOM
} DiagLogCategory;

// Log entry structure
typedef struct {
    uint32_t timestamp;
    DiagLogLevel level;
    DiagLogCategory category;
    uint32_t sequence;
    char message[256];
    uint8_t data[64];
    uint32_t data_length;
} DiagLogEntry;

// Callback for log entries
typedef void (*DiagLogCallback)(const DiagLogEntry* entry, void* context);

// Logger functions
bool DiagLogger_Init(void);
void DiagLogger_Deinit(void);

void DiagLogger_SetLevel(DiagLogLevel level);
DiagLogLevel DiagLogger_GetLevel(void);

void DiagLogger_RegisterCallback(DiagLogCallback callback, void* context);
void DiagLogger_UnregisterCallback(DiagLogCallback callback);

void DiagLogger_Log(DiagLogLevel level, 
                   DiagLogCategory category, 
                   const char* format, ...);

void DiagLogger_LogHex(DiagLogLevel level,
                      DiagLogCategory category,
                      const char* message,
                      const uint8_t* data,
                      uint32_t length);

// Convenience macros
#define LOG_ERROR(cat, fmt, ...) \
    DiagLogger_Log(LOG_LEVEL_ERROR, cat, fmt, ##__VA_ARGS__)

#define LOG_WARNING(cat, fmt, ...) \
    DiagLogger_Log(LOG_LEVEL_WARNING, cat, fmt, ##__VA_ARGS__)

#define LOG_INFO(cat, fmt, ...) \
    DiagLogger_Log(LOG_LEVEL_INFO, cat, fmt, ##__VA_ARGS__)

#define LOG_DEBUG(cat, fmt, ...) \
    DiagLogger_Log(LOG_LEVEL_DEBUG, cat, fmt, ##__VA_ARGS__)

#define LOG_TRACE(cat, fmt, ...) \
    DiagLogger_Log(LOG_LEVEL_TRACE, cat, fmt, ##__VA_ARGS__)

#define LOG_HEX(level, cat, msg, data, len) \
    DiagLogger_LogHex(level, cat, msg, data, len)

#endif // DIAG_LOGGER_H 