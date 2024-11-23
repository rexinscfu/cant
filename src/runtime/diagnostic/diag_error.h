#ifndef CANT_DIAG_ERROR_H
#define DIAG_ERROR_H

#include <stdint.h>
#include <stdbool.h>

// Error categories
typedef enum {
    ERROR_CAT_NONE = 0,
    ERROR_CAT_PROTOCOL,
    ERROR_CAT_SESSION,
    ERROR_CAT_SECURITY,
    ERROR_CAT_TIMING,
    ERROR_CAT_MEMORY,
    ERROR_CAT_SYSTEM,
    ERROR_CAT_CUSTOM
} DiagErrorCategory;

// Error codes
typedef enum {
    // Protocol errors (0x1000 - 0x1FFF)
    ERROR_PROTOCOL_INVALID_FORMAT = 0x1001,
    ERROR_PROTOCOL_INVALID_LENGTH = 0x1002,
    ERROR_PROTOCOL_INVALID_CHECKSUM = 0x1003,
    ERROR_PROTOCOL_UNSUPPORTED = 0x1004,
    ERROR_PROTOCOL_TIMEOUT = 0x1005,
    
    // Session errors (0x2000 - 0x2FFF)
    ERROR_SESSION_INVALID_STATE = 0x2001,
    ERROR_SESSION_TIMEOUT = 0x2002,
    ERROR_SESSION_UNSUPPORTED = 0x2003,
    ERROR_SESSION_CONFLICT = 0x2004,
    
    // Security errors (0x3000 - 0x3FFF)
    ERROR_SECURITY_ACCESS_DENIED = 0x3001,
    ERROR_SECURITY_INVALID_KEY = 0x3002,
    ERROR_SECURITY_LOCKED = 0x3003,
    ERROR_SECURITY_TIMEOUT = 0x3004,
    
    // Timing errors (0x4000 - 0x4FFF)
    ERROR_TIMING_P2_TIMEOUT = 0x4001,
    ERROR_TIMING_P3_TIMEOUT = 0x4002,
    ERROR_TIMING_INVALID = 0x4003,
    
    // Memory errors (0x5000 - 0x5FFF)
    ERROR_MEMORY_ALLOCATION = 0x5001,
    ERROR_MEMORY_OVERFLOW = 0x5002,
    ERROR_MEMORY_INVALID_ADDRESS = 0x5003,
    
    // System errors (0x6000 - 0x6FFF)
    ERROR_SYSTEM_NOT_INITIALIZED = 0x6001,
    ERROR_SYSTEM_ALREADY_INITIALIZED = 0x6002,
    ERROR_SYSTEM_RESOURCE_BUSY = 0x6003,
    
    // Custom errors (0xF000 - 0xFFFF)
    ERROR_CUSTOM_BASE = 0xF000
} DiagErrorCode;

// Error information structure
typedef struct {
    DiagErrorCode code;
    DiagErrorCategory category;
    uint32_t timestamp;
    char message[256];
    // For development/debugging
    #ifdef DEVELOPMENT_BUILD
    const char* file;
    int line;
    const char* function;
    #endif
} DiagError;

// Error callback type
typedef void (*DiagErrorHandler)(const DiagError* error, void* context);

// Error management functions
bool DiagError_Init(void);
void DiagError_Deinit(void);

// Error reporting
#ifdef DEVELOPMENT_BUILD
#define DIAG_ERROR_SET(code, msg, ...) \
    DiagError_SetEx(code, msg, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#else
#define DIAG_ERROR_SET(code, msg, ...) \
    DiagError_Set(code, msg, ##__VA_ARGS__)
#endif

void DiagError_Set(DiagErrorCode code, const char* format, ...);
void DiagError_SetEx(DiagErrorCode code, const char* format, 
                    const char* file, int line, const char* function, ...);

// Error handling
void DiagError_RegisterHandler(DiagErrorHandler handler, void* context);
void DiagError_UnregisterHandler(DiagErrorHandler handler);

// Error information
DiagErrorCode DiagError_GetLastCode(void);
const char* DiagError_GetLastMessage(void);
const DiagError* DiagError_GetLastError(void);

// Error utilities
const char* DiagError_GetCategoryString(DiagErrorCategory category);
const char* DiagError_GetCodeString(DiagErrorCode code);
DiagErrorCategory DiagError_GetCategory(DiagErrorCode code);

#endif // CANT_DIAG_ERROR_H 