#include "diag_recorder.h"
#include "diag_timer.h"
#include "../memory/memory_manager.h"
#include <string.h>
#include <stdio.h>

typedef struct {
    DiagRecorderConfig config;
    DiagRecordEntry* entries;
    uint32_t entry_count;
    uint32_t current_index;
    bool recording;
    bool initialized;
} RecorderContext;

static RecorderContext recorder;

bool DiagRecorder_Init(const DiagRecorderConfig* config) {
    if (!config || recorder.initialized) return false;
    
    memset(&recorder, 0, sizeof(RecorderContext));
    memcpy(&recorder.config, config, sizeof(DiagRecorderConfig));
    
    recorder.entries = (DiagRecordEntry*)MEMORY_ALLOC(
        sizeof(DiagRecordEntry) * config->max_entries);
    
    if (!recorder.entries) return false;
    
    recorder.initialized = true;
    
    if (config->auto_start) {
        DiagRecorder_Start();
    }
    
    return true;
}

void DiagRecorder_Deinit(void) {
    if (!recorder.initialized) return;
    
    DiagRecorder_Stop();
    
    if (recorder.entries) {
        MEMORY_FREE(recorder.entries);
        recorder.entries = NULL;
    }
    
    memset(&recorder, 0, sizeof(RecorderContext));
}

void DiagRecorder_Start(void) {
    if (!recorder.initialized) return;
    recorder.recording = true;
}

void DiagRecorder_Stop(void) {
    if (!recorder.initialized) return;
    recorder.recording = false;
}

void DiagRecorder_Clear(void) {
    if (!recorder.initialized) return;
    
    recorder.entry_count = 0;
    recorder.current_index = 0;
    memset(recorder.entries, 0, 
           sizeof(DiagRecordEntry) * recorder.config.max_entries);
}

bool DiagRecorder_IsRecording(void) {
    return recorder.initialized && recorder.recording;
}

uint32_t DiagRecorder_GetEntryCount(void) {
    return recorder.initialized ? recorder.entry_count : 0;
}

static void add_entry(const DiagRecordEntry* entry) {
    if (!recorder.initialized || !recorder.recording || !entry) return;
    
    if (recorder.current_index >= recorder.config.max_entries) {
        if (recorder.config.circular_buffer) {
            recorder.current_index = 0;
        } else {
            return;
        }
    }
    
    memcpy(&recorder.entries[recorder.current_index], entry, 
           sizeof(DiagRecordEntry));
    
    recorder.current_index++;
    if (recorder.entry_count < recorder.config.max_entries) {
        recorder.entry_count++;
    }
}

const DiagRecordEntry* DiagRecorder_GetEntry(uint32_t index) {
    if (!recorder.initialized || index >= recorder.entry_count) {
        return NULL;
    }
    
    return &recorder.entries[index];
}

bool DiagRecorder_ExportToFile(const char* filename) {
    if (!recorder.initialized || !filename) return false;
    
    FILE* file = fopen(filename, "w");
    if (!file) return false;
    
    fprintf(file, "Diagnostic Recording Export\n");
    fprintf(file, "Total Entries: %u\n\n", recorder.entry_count);
    
    for (uint32_t i = 0; i < recorder.entry_count; i++) {
        DiagRecordEntry* entry = &recorder.entries[i];
        
        fprintf(file, "Entry %u:\n", i);
        fprintf(file, "  Timestamp: %u\n", entry->timestamp);
        fprintf(file, "  Type: %d\n", entry->type);
        fprintf(file, "  Sequence: %u\n", entry->sequence);
        
        switch (entry->type) {
            case RECORD_TYPE_MESSAGE:
                fprintf(file, "  Message:\n");
                fprintf(file, "    Service ID: 0x%02X\n", 
                        entry->data.message.service_id);
                fprintf(file, "    Sub-Function: 0x%02X\n", 
                        entry->data.message.sub_function);
                fprintf(file, "    Data (%u bytes):", 
                        entry->data.message.data_length);
                for (uint32_t j = 0; j < entry->data.message.data_length; j++) {
                    if (j % 16 == 0) fprintf(file, "\n    ");
                    fprintf(file, "%02X ", entry->data.message.data[j]);
                }
                fprintf(file, "\n");
                break;
                
            case RECORD_TYPE_RESPONSE:
                fprintf(file, "  Response:\n");
                fprintf(file, "    Code: 0x%02X\n", 
                        entry->data.response.response_code);
                fprintf(file, "    Data (%u bytes):", 
                        entry->data.response.data_length);
                for (uint32_t j = 0; j < entry->data.response.data_length; j++) {
                    if (j % 16 == 0) fprintf(file, "\n    ");
                    fprintf(file, "%02X ", entry->data.response.data[j]);
                }
                fprintf(file, "\n");
                break;
                
            case RECORD_TYPE_SESSION:
                fprintf(file, "  Session Change:\n");
                fprintf(file, "    Old Session: 0x%02X\n", 
                        entry->data.session.old_session);
                fprintf(file, "    New Session: 0x%02X\n", 
                        entry->data.session.new_session);
                fprintf(file, "    Result: 0x%02X\n", 
                        entry->data.session.result);
                break;
                
            case RECORD_TYPE_SECURITY:
                fprintf(file, "  Security Event:\n");
                fprintf(file, "    Level: 0x%02X\n", 
                        entry->data.security.level);
                fprintf(file, "    Result: 0x%02X\n", 
                        entry->data.security.result);
                fprintf(file, "    Seed/Key: 0x%08X\n", 
                        entry->data.security.seed_or_key);
                break;
                
            case RECORD_TYPE_ERROR:
                fprintf(file, "  Error:\n");
                fprintf(file, "    Code: 0x%04X\n", 
                        entry->data.error.error_code);
                fprintf(file, "    Description: %s\n", 
                        entry->data.error.description);
                break;
                
            case RECORD_TYPE_CUSTOM:
                fprintf(file, "  Custom Record:\n");
                fprintf(file, "    Type: %u\n", entry->data.custom.type);
                fprintf(file, "    Data (%u bytes):", 
                        entry->data.custom.data_length);
                for (uint32_t j = 0; j < entry->data.custom.data_length; j++) {
                    if (j % 16 == 0) fprintf(file, "\n    ");
                    fprintf(file, "%02X ", entry->data.custom.data[j]);
                }
                fprintf(file, "\n");
                break;
        }
        fprintf(file, "\n");
    }
    
    fclose(file);
    return true;
}

bool DiagRecorder_AddCustomRecord(uint32_t type, 
                                const uint8_t* data, 
                                uint32_t length) 
{
    if (!recorder.initialized || !recorder.recording || 
        !data || length > sizeof(DiagRecordEntry::data.custom.data)) {
        return false;
    }
    
    DiagRecordEntry entry;
    memset(&entry, 0, sizeof(DiagRecordEntry));
    
    entry.timestamp = DiagTimer_GetTimestamp();
    entry.type = RECORD_TYPE_CUSTOM;
    entry.sequence = recorder.entry_count;
    entry.data.custom.type = type;
    entry.data.custom.data_length = length;
    memcpy(entry.data.custom.data, data, length);
    
    add_entry(&entry);
    return true;
}

uint32_t DiagRecorder_FindSequence(const uint8_t* pattern, 
                                 uint32_t length,
                                 uint32_t start_index) 
{
    if (!recorder.initialized || !pattern || length == 0 || 
        start_index >= recorder.entry_count) {
        return UINT32_MAX;
    }
    
    for (uint32_t i = start_index; i < recorder.entry_count; i++) {
        DiagRecordEntry* entry = &recorder.entries[i];
        
        if (entry->type == RECORD_TYPE_MESSAGE && 
            entry->data.message.data_length >= length) {
            
            if (memcmp(entry->data.message.data, pattern, length) == 0) {
                return i;
            }
        }
    }
    
    return UINT32_MAX;
}

uint32_t DiagRecorder_FilterEntries(DiagRecordFilter filter,
                                  void* context,
                                  DiagRecordEntry* output,
                                  uint32_t max_output) 
{
    if (!recorder.initialized || !filter || !output || max_output == 0) {
        return 0;
    }
    
    uint32_t output_count = 0;
    
    for (uint32_t i = 0; i < recorder.entry_count && output_count < max_output; i++) {
        if (filter(&recorder.entries[i], context)) {
            memcpy(&output[output_count], &recorder.entries[i], 
                   sizeof(DiagRecordEntry));
            output_count++;
        }
    }
    
    return output_count;
}

void DiagRecorder_GetStats(DiagRecorderStats* stats) {
    if (!recorder.initialized || !stats) return;
    
    memset(stats, 0, sizeof(DiagRecorderStats));
    
    stats->total_entries = recorder.entry_count;
    
    uint32_t last_response_time = 0;
    
    for (uint32_t i = 0; i < recorder.entry_count; i++) {
        DiagRecordEntry* entry = &recorder.entries[i];
        
        switch (entry->type) {
            case RECORD_TYPE_MESSAGE:
                stats->message_count++;
                stats->total_data_bytes += entry->data.message.data_length;
                last_response_time = entry->timestamp;
                break;
                
            case RECORD_TYPE_RESPONSE:
                stats->response_count++;
                stats->total_data_bytes += entry->data.response.data_length;
                if (last_response_time > 0) {
                    uint32_t response_time = entry->timestamp - last_response_time;
                    stats->average_response_time += response_time;
                    if (response_time > stats->max_response_time) {
                        stats->max_response_time = response_time;
                    }
                }
                break;
                
            case RECORD_TYPE_SESSION:
                stats->session_changes++;
                break;
                
            case RECORD_TYPE_SECURITY:
                stats->security_events++;
                break;
                
            case RECORD_TYPE_ERROR:
                stats->error_count++;
                break;
                
            case RECORD_TYPE_CUSTOM:
                stats->custom_records++;
                stats->total_data_bytes += entry->data.custom.data_length;
                break;
        }
    }
    
    if (stats->response_count > 0) {
        stats->average_response_time /= stats->response_count;
    }
} 