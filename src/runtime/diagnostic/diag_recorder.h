#ifndef DIAG_RECORDER_H
#define DIAG_RECORDER_H

#include "diag_logger.h"
#include <stdint.h>
#include <stdbool.h>

// Recording entry types
typedef enum {
    RECORD_TYPE_MESSAGE,
    RECORD_TYPE_RESPONSE,
    RECORD_TYPE_SESSION,
    RECORD_TYPE_SECURITY,
    RECORD_TYPE_ERROR,
    RECORD_TYPE_CUSTOM
} DiagRecordType;

// Recording entry
typedef struct {
    uint32_t timestamp;
    DiagRecordType type;
    uint32_t sequence;
    union {
        struct {
            uint8_t service_id;
            uint8_t sub_function;
            uint8_t data[64];
            uint32_t data_length;
        } message;
        struct {
            uint8_t response_code;
            uint8_t data[64];
            uint32_t data_length;
        } response;
        struct {
            uint8_t old_session;
            uint8_t new_session;
            uint8_t result;
        } session;
        struct {
            uint8_t level;
            uint8_t result;
            uint32_t seed_or_key;
        } security;
        struct {
            uint16_t error_code;
            char description[32];
        } error;
        struct {
            uint32_t type;
            uint8_t data[64];
            uint32_t data_length;
        } custom;
    } data;
} DiagRecordEntry;

// Recording configuration
typedef struct {
    uint32_t max_entries;
    bool circular_buffer;
    bool auto_start;
    const char* export_path;
} DiagRecorderConfig;

// Recorder functions
bool DiagRecorder_Init(const DiagRecorderConfig* config);
void DiagRecorder_Deinit(void);

void DiagRecorder_Start(void);
void DiagRecorder_Stop(void);
void DiagRecorder_Clear(void);

bool DiagRecorder_IsRecording(void);
uint32_t DiagRecorder_GetEntryCount(void);

// Recording access
const DiagRecordEntry* DiagRecorder_GetEntry(uint32_t index);
bool DiagRecorder_ExportToFile(const char* filename);

// Custom record types
bool DiagRecorder_AddCustomRecord(uint32_t type, 
                                const uint8_t* data, 
                                uint32_t length);

// Analysis functions
uint32_t DiagRecorder_FindSequence(const uint8_t* pattern, 
                                  uint32_t length,
                                  uint32_t start_index);

typedef bool (*DiagRecordFilter)(const DiagRecordEntry* entry, void* context);
uint32_t DiagRecorder_FilterEntries(DiagRecordFilter filter,
                                   void* context,
                                   DiagRecordEntry* output,
                                   uint32_t max_output);

// Statistics
typedef struct {
    uint32_t total_entries;
    uint32_t message_count;
    uint32_t response_count;
    uint32_t session_changes;
    uint32_t security_events;
    uint32_t error_count;
    uint32_t custom_records;
    uint32_t total_data_bytes;
    uint32_t average_response_time;
    uint32_t max_response_time;
} DiagRecorderStats;

void DiagRecorder_GetStats(DiagRecorderStats* stats);

#endif // DIAG_RECORDER_H 