#include "diag_logger.h"
#include "diag_timer.h"
#include "../memory/memory_manager.h"
#include <stdarg.h>
#include <string.h>

#define MAX_LOG_CALLBACKS 8
#define LOG_BUFFER_SIZE 32

typedef struct {
    DiagLogCallback callback;
    void* context;
    bool active;
} LogCallback;

typedef struct {
    DiagLogLevel current_level;
    LogCallback callbacks[MAX_LOG_CALLBACKS];
    uint32_t callback_count;
    uint32_t sequence_number;
    DiagLogEntry buffer[LOG_BUFFER_SIZE];
    uint32_t buffer_index;
    bool initialized;
} LoggerContext;

static LoggerContext logger;

// Internal functions
static void flush_buffer(void) {
    if (!logger.initialized) return;
    
    for (uint32_t i = 0; i < logger.buffer_index; i++) {
        DiagLogEntry* entry = &logger.buffer[i];
        
        for (uint32_t j = 0; j < logger.callback_count; j++) {
            if (logger.callbacks[j].active && logger.callbacks[j].callback) {
                logger.callbacks[j].callback(entry, logger.callbacks[j].context);
            }
        }
    }
    
    logger.buffer_index = 0;
}

static void buffer_log(const DiagLogEntry* entry) {
    if (logger.buffer_index >= LOG_BUFFER_SIZE) {
        flush_buffer();
    }
    
    memcpy(&logger.buffer[logger.buffer_index], entry, sizeof(DiagLogEntry));
    logger.buffer_index++;
}

bool DiagLogger_Init(void) {
    if (logger.initialized) return false;
    
    memset(&logger, 0, sizeof(LoggerContext));
    logger.current_level = LOG_LEVEL_INFO;
    logger.initialized = true;
    
    return true;
}

void DiagLogger_Deinit(void) {
    if (!logger.initialized) return;
    
    flush_buffer();
    memset(&logger, 0, sizeof(LoggerContext));
}

void DiagLogger_SetLevel(DiagLogLevel level) {
    if (!logger.initialized) return;
    logger.current_level = level;
}

DiagLogLevel DiagLogger_GetLevel(void) {
    return logger.initialized ? logger.current_level : LOG_LEVEL_NONE;
}

void DiagLogger_RegisterCallback(DiagLogCallback callback, void* context) {
    if (!logger.initialized || !callback) return;
    
    // Check if callback already registered
    for (uint32_t i = 0; i < logger.callback_count; i++) {
        if (logger.callbacks[i].callback == callback) {
            logger.callbacks[i].context = context;
            logger.callbacks[i].active = true;
            return;
        }
    }
    
    // Find free slot
    for (uint32_t i = 0; i < MAX_LOG_CALLBACKS; i++) {
        if (!logger.callbacks[i].active) {
            logger.callbacks[i].callback = callback;
            logger.callbacks[i].context = context;
            logger.callbacks[i].active = true;
            
            if (i >= logger.callback_count) {
                logger.callback_count = i + 1;
            }
            return;
        }
    }
}

void DiagLogger_UnregisterCallback(DiagLogCallback callback) {
    if (!logger.initialized || !callback) return;
    
    for (uint32_t i = 0; i < logger.callback_count; i++) {
        if (logger.callbacks[i].callback == callback) {
            logger.callbacks[i].callback = NULL;
            logger.callbacks[i].context = NULL;
            logger.callbacks[i].active = false;
            
            // Update callback count if possible
            while (logger.callback_count > 0 && 
                   !logger.callbacks[logger.callback_count - 1].active) {
                logger.callback_count--;
            }
            break;
        }
    }
}

void DiagLogger_Log(DiagLogLevel level, 
                   DiagLogCategory category, 
                   const char* format, ...) 
{
    if (!logger.initialized || level > logger.current_level || !format) return;
    
    DiagLogEntry entry;
    memset(&entry, 0, sizeof(DiagLogEntry));
    
    entry.timestamp = DiagTimer_GetTimestamp();
    entry.level = level;
    entry.category = category;
    entry.sequence = logger.sequence_number++;
    
    va_list args;
    va_start(args, format);
    vsnprintf(entry.message, sizeof(entry.message), format, args);
    va_end(args);
    
    buffer_log(&entry);
    
    // Flush immediately for error logs
    if (level == LOG_LEVEL_ERROR) {
        flush_buffer();
    }
}

void DiagLogger_LogHex(DiagLogLevel level,
                      DiagLogCategory category,
                      const char* message,
                      const uint8_t* data,
                      uint32_t length)
{
    if (!logger.initialized || level > logger.current_level || 
        !message || !data || length == 0) return;
    
    DiagLogEntry entry;
    memset(&entry, 0, sizeof(DiagLogEntry));
    
    entry.timestamp = DiagTimer_GetTimestamp();
    entry.level = level;
    entry.category = category;
    entry.sequence = logger.sequence_number++;
    
    strncpy(entry.message, message, sizeof(entry.message) - 1);
    
    // Copy hex data
    uint32_t copy_length = length;
    if (copy_length > sizeof(entry.data)) {
        copy_length = sizeof(entry.data);
    }
    
    memcpy(entry.data, data, copy_length);
    entry.data_length = copy_length;
    
    buffer_log(&entry);
    
    // Flush immediately for error logs
    if (level == LOG_LEVEL_ERROR) {
        flush_buffer();
    }
} 