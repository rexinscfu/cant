#include "diag_logger.h"
#include "../os/critical.h"
#include "../os/timer.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#define MAX_LOG_QUEUE_SIZE 1024
#define LOG_BUFFER_SIZE 512

typedef struct {
    FILE* file;
    LoggerConfig config;
    LogEntry queue[MAX_LOG_QUEUE_SIZE];
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t current_file_size;
    char log_buffer[LOG_BUFFER_SIZE];
    bool initialized;
} LoggerContext;

static LoggerContext logger_ctx;

static const char* level_strings[] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "CRITICAL"
};

static bool rotate_log_file(void) {
    if (!logger_ctx.file) {
        return false;
    }

    fclose(logger_ctx.file);

    char old_name[128];
    char new_name[128];

    // Remove oldest backup if exists
    snprintf(old_name, sizeof(old_name), "%s.%d", 
             logger_ctx.config.filename, 
             logger_ctx.config.max_backup_files);
    remove(old_name);

    // Rotate existing backups
    for (int i = logger_ctx.config.max_backup_files - 1; i >= 1; i--) {
        snprintf(old_name, sizeof(old_name), "%s.%d", 
                 logger_ctx.config.filename, i);
        snprintf(new_name, sizeof(new_name), "%s.%d", 
                 logger_ctx.config.filename, i + 1);
        rename(old_name, new_name);
    }

    // Rename current log file
    snprintf(new_name, sizeof(new_name), "%s.1", 
             logger_ctx.config.filename);
    rename(logger_ctx.config.filename, new_name);

    // Open new log file
    logger_ctx.file = fopen(logger_ctx.config.filename, "w");
    if (!logger_ctx.file) {
        return false;
    }

    logger_ctx.current_file_size = 0;
    return true;
}

static void write_log_entry(const LogEntry* entry) {
    if (!logger_ctx.initialized) {
        return;
    }

    enter_critical();

    int written = 0;
    char timestamp[32] = {0};

    if (logger_ctx.config.include_timestamp) {
        time_t t = entry->timestamp;
        struct tm* tm_info = localtime(&t);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    }

    // Format log message
    if (entry->data_length > 0) {
        written = snprintf(logger_ctx.log_buffer, LOG_BUFFER_SIZE,
                          "%s [%s] %s%s%s: %s - ",
                          timestamp,
                          level_strings[entry->level],
                          entry->module,
                          logger_ctx.config.include_session_id ? " [Session " : "",
                          logger_ctx.config.include_session_id ? (char*)&entry->session_id : "",
                          logger_ctx.config.include_session_id ? "]" : "",
                          entry->message);

        // Add hex dump of data
        for (uint16_t i = 0; i < entry->data_length && written < LOG_BUFFER_SIZE - 3; i++) {
            written += snprintf(logger_ctx.log_buffer + written, 
                              LOG_BUFFER_SIZE - written,
                              "%02X ", entry->data[i]);
        }
        written += snprintf(logger_ctx.log_buffer + written,
                          LOG_BUFFER_SIZE - written, "\n");
    } else {
        written = snprintf(logger_ctx.log_buffer, LOG_BUFFER_SIZE,
                          "%s [%s] %s%s%s: %s\n",
                          timestamp,
                          level_strings[entry->level],
                          entry->module,
                          logger_ctx.config.include_session_id ? " [Session " : "",
                          logger_ctx.config.include_session_id ? (char*)&entry->session_id : "",
                          logger_ctx.config.include_session_id ? "]" : "",
                          entry->message);
    }

    // Write to console if enabled
    if (logger_ctx.config.console_output) {
        printf("%s", logger_ctx.log_buffer);
    }

    // Write to file if enabled
    if (logger_ctx.file) {
        logger_ctx.current_file_size += written;
        if (logger_ctx.current_file_size >= logger_ctx.config.max_file_size) {
            rotate_log_file();
        }
        fprintf(logger_ctx.file, "%s", logger_ctx.log_buffer);
        fflush(logger_ctx.file);
    }

    exit_critical();
}

bool Logger_Init(const LoggerConfig* config) {
    if (!config) {
        return false;
    }

    memset(&logger_ctx, 0, sizeof(LoggerContext));
    memcpy(&logger_ctx.config, config, sizeof(LoggerConfig));

    if (config->filename[0] != '\0') {
        logger_ctx.file = fopen(config->filename, "w");
        if (!logger_ctx.file) {
            return false;
        }
    }

    logger_ctx.initialized = true;
    return true;
}

void Logger_Deinit(void) {
    if (logger_ctx.file) {
        fclose(logger_ctx.file);
    }
    memset(&logger_ctx, 0, sizeof(LoggerContext));
}

void Logger_Log(LogLevel level, const char* module, const char* format, ...) {
    if (!logger_ctx.initialized || level < logger_ctx.config.min_level) {
        return;
    }

    LogEntry entry;
    memset(&entry, 0, sizeof(LogEntry));
    
    entry.level = level;
    entry.timestamp = time(NULL);
    strncpy(entry.module, module, sizeof(entry.module) - 1);

    va_list args;
    va_start(args, format);
    vsnprintf(entry.message, sizeof(entry.message), format, args);
    va_end(args);

    write_log_entry(&entry);
}

void Logger_LogHex(LogLevel level, const char* module, const uint8_t* data, 
                  uint16_t length, const char* format, ...) {
    if (!logger_ctx.initialized || level < logger_ctx.config.min_level) {
        return;
    }

    LogEntry entry;
    memset(&entry, 0, sizeof(LogEntry));
    
    entry.level = level;
    entry.timestamp = time(NULL);
    strncpy(entry.module, module, sizeof(entry.module) - 1);

    va_list args;
    va_start(args, format);
    vsnprintf(entry.message, sizeof(entry.message), format, args);
    va_end(args);

    if (length > sizeof(entry.data)) {
        length = sizeof(entry.data);
    }
    memcpy(entry.data, data, length);
    entry.data_length = length;

    write_log_entry(&entry);
}

void Logger_LogSession(uint32_t session_id, LogLevel level, const char* module, 
                      const char* format, ...) {
    if (!logger_ctx.initialized || level < logger_ctx.config.min_level) {
        return;
    }

    LogEntry entry;
    memset(&entry, 0, sizeof(LogEntry));
    
    entry.session_id = session_id;
    entry.level = level;
    entry.timestamp = time(NULL);
    strncpy(entry.module, module, sizeof(entry.module) - 1);

    va_list args;
    va_start(args, format);
    vsnprintf(entry.message, sizeof(entry.message), format, args);
    va_end(args);

    write_log_entry(&entry);
}

bool Logger_SetLevel(LogLevel level) {
    if (level > LOG_LEVEL_CRITICAL) {
        return false;
    }
    logger_ctx.config.min_level = level;
    return true;
}

bool Logger_EnableFileOutput(const char* filename) {
    if (!filename || filename[0] == '\0') {
        return false;
    }

    if (logger_ctx.file) {
        fclose(logger_ctx.file);
    }

    logger_ctx.file = fopen(filename, "w");
    if (!logger_ctx.file) {
        return false;
    }

    strncpy(logger_ctx.config.filename, filename, sizeof(logger_ctx.config.filename) - 1);
    logger_ctx.current_file_size = 0;
    return true;
}

bool Logger_EnableConsoleOutput(bool enable) {
    logger_ctx.config.console_output = enable;
    return true;
}

void Logger_Flush(void) {
    if (logger_ctx.file) {
        fflush(logger_ctx.file);
    }
} 