#ifndef CANT_DIAG_LOGGER_H
#define CANT_DIAG_LOGGER_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_CRITICAL
} LogLevel;

typedef struct {
    char filename[64];
    uint32_t max_file_size;
    uint8_t max_backup_files;
    bool console_output;
    LogLevel min_level;
    bool include_timestamp;
    bool include_session_id;
} LoggerConfig;

typedef struct {
    uint32_t session_id;
    uint32_t timestamp;
    LogLevel level;
    char module[32];
    char message[256];
    uint8_t data[128];
    uint16_t data_length;
} LogEntry;

bool Logger_Init(const LoggerConfig* config);
void Logger_Deinit(void);

void Logger_Log(LogLevel level, const char* module, const char* format, ...);
void Logger_LogHex(LogLevel level, const char* module, const uint8_t* data, uint16_t length, const char* format, ...);
void Logger_LogSession(uint32_t session_id, LogLevel level, const char* module, const char* format, ...);

bool Logger_SetLevel(LogLevel level);
bool Logger_EnableFileOutput(const char* filename);
bool Logger_EnableConsoleOutput(bool enable);
void Logger_Flush(void);

#endif // CANT_DIAG_LOGGER_H 