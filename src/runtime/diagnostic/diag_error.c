#include "diag_error.h"
#include "diag_timer.h"
#include "../memory/memory_manager.h"
#include "../diagnostic/logging/diag_logger.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#define MAX_ERROR_HANDLERS 8
#define ERROR_HISTORY_SIZE 16

// Error manager structure
typedef struct {
    DiagError last_error;
    DiagError history[ERROR_HISTORY_SIZE];
    uint32_t history_index;
    struct {
        DiagErrorHandler handler;
        void* context;
        bool active;
    } handlers[MAX_ERROR_HANDLERS];
    uint32_t handler_count;
    bool initialized;
} ErrorManager;

static ErrorManager error_mgr;

// Error category strings
static const char* category_strings[] = {
    "None",
    "Protocol",
    "Session",
    "Security",
    "Timing",
    "Memory",
    "System",
    "Custom"
};

// Error code strings
// TODO: Add more descriptive messages
static const struct {
    DiagErrorCode code;
    const char* string;
} error_strings[] = {
    { ERROR_PROTOCOL_INVALID_FORMAT, "Invalid protocol format" },
    { ERROR_PROTOCOL_INVALID_LENGTH, "Invalid message length" },
    { ERROR_PROTOCOL_INVALID_CHECKSUM, "Invalid checksum" },
    { ERROR_PROTOCOL_UNSUPPORTED, "Unsupported protocol" },
    { ERROR_PROTOCOL_TIMEOUT, "Protocol timeout" },
    // ... add more error strings
    { 0, NULL }
};

bool DiagError_Init(void) {
    if (error_mgr.initialized) {
        return false;
    }
    
    memset(&error_mgr, 0, sizeof(ErrorManager));
    error_mgr.initialized = true;
    
    return true;
}

void DiagError_Deinit(void) {
    if (!error_mgr.initialized) return;
    
    // Clear all handlers
    memset(error_mgr.handlers, 0, sizeof(error_mgr.handlers));
    error_mgr.handler_count = 0;
    
    memset(&error_mgr, 0, sizeof(ErrorManager));
}

static void format_error_message(char* buffer, size_t size, 
                               const char* format, va_list args) 
{
    vsnprintf(buffer, size, format, args);
    buffer[size - 1] = '\0';  // Ensure null termination
}

void DiagError_Set(DiagErrorCode code, const char* format, ...) {
    if (!error_mgr.initialized) return;
    
    va_list args;
    va_start(args, format);
    
    // Clear previous error
    memset(&error_mgr.last_error, 0, sizeof(DiagError));
    
    // Set new error
    error_mgr.last_error.code = code;
    error_mgr.last_error.category = DiagError_GetCategory(code);
    error_mgr.last_error.timestamp = DiagTimer_GetTimestamp();
    
    format_error_message(error_mgr.last_error.message, 
                        sizeof(error_mgr.last_error.message),
                        format, args);
    
    va_end(args);
    
    // Add to history
    memcpy(&error_mgr.history[error_mgr.history_index], 
           &error_mgr.last_error, 
           sizeof(DiagError));
           
    error_mgr.history_index = (error_mgr.history_index + 1) % ERROR_HISTORY_SIZE;
    
    // Log error
    Logger_Log(LOG_LEVEL_ERROR, "ERROR", "[0x%04X] %s", 
              code, error_mgr.last_error.message);
    
    // Notify handlers
    for (uint32_t i = 0; i < error_mgr.handler_count; i++) {
        if (error_mgr.handlers[i].active && error_mgr.handlers[i].handler) {
            error_mgr.handlers[i].handler(&error_mgr.last_error, 
                                        error_mgr.handlers[i].context);
        }
    }
}

void DiagError_SetEx(DiagErrorCode code, const char* format, 
                    const char* file, int line, const char* function, ...) 
{
    if (!error_mgr.initialized) return;
    
    va_list args;
    va_start(args, function);
    
    // Clear previous error
    memset(&error_mgr.last_error, 0, sizeof(DiagError));
    
    // Set new error
    error_mgr.last_error.code = code;
    error_mgr.last_error.category = DiagError_GetCategory(code);
    error_mgr.last_error.timestamp = DiagTimer_GetTimestamp();
    
    #ifdef DEVELOPMENT_BUILD
    error_mgr.last_error.file = file;
    error_mgr.last_error.line = line;
    error_mgr.last_error.function = function;
    #endif
    
    format_error_message(error_mgr.last_error.message,
                        sizeof(error_mgr.last_error.message),
                        format, args);
    
    va_end(args);
    
}

void DiagError_RegisterHandler(DiagErrorHandler handler, void* context) {
    if (!error_mgr.initialized || !handler) {
        return;
    }
    
    // Check if handler already registered
    for (uint32_t i = 0; i < error_mgr.handler_count; i++) {
        if (error_mgr.handlers[i].handler == handler) {
            error_mgr.handlers[i].context = context;
            error_mgr.handlers[i].active = true;
            return;
        }
    }
    
    // Find free slot
    for (uint32_t i = 0; i < MAX_ERROR_HANDLERS; i++) {
        if (!error_mgr.handlers[i].active) {
            error_mgr.handlers[i].handler = handler;
            error_mgr.handlers[i].context = context;
            error_mgr.handlers[i].active = true;
            
            if (i >= error_mgr.handler_count) {
                error_mgr.handler_count = i + 1;
            }
            return;
        }
    }
    
    // No free slots - log warning
    Logger_Log(LOG_LEVEL_WARNING, "ERROR", 
              "Failed to register error handler - max handlers reached (%d)",
              MAX_ERROR_HANDLERS);
}

void DiagError_UnregisterHandler(DiagErrorHandler handler) {
    if (!error_mgr.initialized || !handler) {
        return;
    }
    
    for (uint32_t i = 0; i < error_mgr.handler_count; i++) {
        if (error_mgr.handlers[i].handler == handler) {
            error_mgr.handlers[i].handler = NULL;
            error_mgr.handlers[i].context = NULL;
            error_mgr.handlers[i].active = false;
            
            // Update handler count if possible
            while (error_mgr.handler_count > 0 && 
                   !error_mgr.handlers[error_mgr.handler_count - 1].active) {
                error_mgr.handler_count--;
            }
            return;
        }
    }
}

DiagErrorCode DiagError_GetLastCode(void) {
    return error_mgr.initialized ? error_mgr.last_error.code : 0;
}

const char* DiagError_GetLastMessage(void) {
    return error_mgr.initialized ? error_mgr.last_error.message : "";
}

const DiagError* DiagError_GetLastError(void) {
    return error_mgr.initialized ? &error_mgr.last_error : NULL;
}

const char* DiagError_GetCategoryString(DiagErrorCategory category) {
    if (category >= sizeof(category_strings) / sizeof(category_strings[0])) {
        return "Unknown";
    }
    return category_strings[category];
}

const char* DiagError_GetCodeString(DiagErrorCode code) {
    for (int i = 0; error_strings[i].string != NULL; i++) {
        if (error_strings[i].code == code) {
            return error_strings[i].string;
        }
    }
    
    // Handle custom error codes
    if (code >= ERROR_CUSTOM_BASE) {
        return "Custom error";
    }
    
    return "Unknown error";
}

DiagErrorCategory DiagError_GetCategory(DiagErrorCode code) {
    if (code >= ERROR_CUSTOM_BASE) {
        return ERROR_CAT_CUSTOM;
    }
    
    uint32_t category = (code >> 12) & 0xF;
    if (category >= ERROR_CAT_CUSTOM) {
        return ERROR_CAT_NONE;
    }
    
    return (DiagErrorCategory)category;
}

// Development/Debug helper functions
#ifdef DEVELOPMENT_BUILD

void DiagError_DumpHistory(void) {
    if (!error_mgr.initialized) return;
    
    printf("\nError History Dump:\n");
    printf("==================\n");
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < ERROR_HISTORY_SIZE; i++) {
        uint32_t idx = (error_mgr.history_index - 1 - i) % ERROR_HISTORY_SIZE;
        DiagError* err = &error_mgr.history[idx];
        
        if (err->code == 0) continue;
        
        printf("\nError #%d:\n", ++count);
        printf("Code: 0x%04X\n", err->code);
        printf("Category: %s\n", DiagError_GetCategoryString(err->category));
        printf("Message: %s\n", err->message);
        printf("Time: %u ms\n", err->timestamp);
        printf("File: %s:%d\n", err->file, err->line);
        printf("Function: %s\n", err->function);
    }
    
    if (count == 0) {
        printf("No errors in history.\n");
    }
}

void DiagError_ClearHistory(void) {
    if (!error_mgr.initialized) return;
    
    memset(error_mgr.history, 0, sizeof(error_mgr.history));
    error_mgr.history_index = 0;
    
    Logger_Log(LOG_LEVEL_INFO, "ERROR", "Error history cleared");
}

#endif // DEVELOPMENT_BUILD

