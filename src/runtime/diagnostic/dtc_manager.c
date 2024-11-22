#include "dtc_manager.h"
#include <string.h>
#include "../utils/timer.h"
#include "../os/critical.h"
#include "../memory/memory_manager.h"

#define MAX_DTC_COUNT 1000
#define MAX_FREEZE_FRAMES 10
#define MAX_FREEZE_FRAME_SIZE 100

// Internal DTC storage
typedef struct {
    DtcRecord* records;
    uint32_t record_count;
    uint32_t max_records;
    uint8_t* freeze_frame_buffer;
    uint32_t freeze_frame_buffer_size;
    DtcConfig config;
    CriticalSection critical;
    bool initialized;
} DtcStorage;

static DtcStorage dtc_storage;

static bool validate_dtc(uint32_t dtc) {
    for (uint32_t i = 0; i < dtc_storage.record_count; i++) {
        if (dtc_storage.records[i].dtc_number == dtc) {
            return true;
        }
    }
    return false;
}

static DtcRecord* find_dtc_record(uint32_t dtc) {
    for (uint32_t i = 0; i < dtc_storage.record_count; i++) {
        if (dtc_storage.records[i].dtc_number == dtc) {
            return &dtc_storage.records[i];
        }
    }
    return NULL;
}

static bool allocate_freeze_frame(DtcRecord* record, uint16_t size) {
    if (record->freeze_frame_count >= dtc_storage.config.max_freeze_frames_per_dtc) {
        return false;
    }

    uint32_t offset = 0;
    for (uint32_t i = 0; i < dtc_storage.record_count; i++) {
        for (uint8_t j = 0; j < dtc_storage.records[i].freeze_frame_count; j++) {
            offset += dtc_storage.records[i].freeze_frames[j].data_size;
        }
    }

    if (offset + size > dtc_storage.freeze_frame_buffer_size) {
        return false;
    }

    record->freeze_frames[record->freeze_frame_count].data = 
        &dtc_storage.freeze_frame_buffer[offset];
    record->freeze_frames[record->freeze_frame_count].data_size = size;
    return true;
}

bool DTC_Init(const DtcConfig* config) {
    if (!config || config->max_dtc_count == 0 || 
        config->max_dtc_count > MAX_DTC_COUNT ||
        config->max_freeze_frames_per_dtc > MAX_FREEZE_FRAMES) {
        return false;
    }

    enter_critical(&dtc_storage.critical);

    // Allocate memory for DTC records
    dtc_storage.records = (DtcRecord*)memory_allocate(
        sizeof(DtcRecord) * config->max_dtc_count);
    if (!dtc_storage.records) {
        exit_critical(&dtc_storage.critical);
        return false;
    }

    // Allocate memory for freeze frame buffer
    uint32_t total_freeze_frame_size = config->max_dtc_count * 
        config->max_freeze_frames_per_dtc * MAX_FREEZE_FRAME_SIZE;
    dtc_storage.freeze_frame_buffer = (uint8_t*)memory_allocate(
        total_freeze_frame_size);
    if (!dtc_storage.freeze_frame_buffer) {
        memory_free(dtc_storage.records);
        exit_critical(&dtc_storage.critical);
        return false;
    }

    memset(dtc_storage.records, 0, sizeof(DtcRecord) * config->max_dtc_count);
    memset(dtc_storage.freeze_frame_buffer, 0, total_freeze_frame_size);

    memcpy(&dtc_storage.config, config, sizeof(DtcConfig));
    dtc_storage.record_count = 0;
    dtc_storage.max_records = config->max_dtc_count;
    dtc_storage.freeze_frame_buffer_size = total_freeze_frame_size;
    dtc_storage.initialized = true;

    exit_critical(&dtc_storage.critical);
    return true;
}

void DTC_DeInit(void) {
    enter_critical(&dtc_storage.critical);

    if (dtc_storage.records) {
        memory_free(dtc_storage.records);
    }
    if (dtc_storage.freeze_frame_buffer) {
        memory_free(dtc_storage.freeze_frame_buffer);
    }

    memset(&dtc_storage, 0, sizeof(DtcStorage));

    exit_critical(&dtc_storage.critical);
}

bool DTC_SetStatus(uint32_t dtc, uint8_t status_mask) {
    if (!dtc_storage.initialized) {
        return false;
    }

    enter_critical(&dtc_storage.critical);

    DtcRecord* record = find_dtc_record(dtc);
    if (!record) {
        if (dtc_storage.record_count >= dtc_storage.max_records) {
            exit_critical(&dtc_storage.critical);
            return false;
        }

        record = &dtc_storage.records[dtc_storage.record_count++];
        record->dtc_number = dtc;
        record->first_occurrence = get_system_time_ms();
    }

    uint8_t old_status = record->status_mask;
    record->status_mask = status_mask;
    record->last_occurrence = get_system_time_ms();
    record->occurrence_count++;

    if (dtc_storage.config.status_change_callback) {
        dtc_storage.config.status_change_callback(dtc, old_status, status_mask);
    }

    exit_critical(&dtc_storage.critical);
    return true;
}

uint8_t DTC_GetStatus(uint32_t dtc) {
    if (!dtc_storage.initialized || !validate_dtc(dtc)) {
        return 0;
    }

    DtcRecord* record = find_dtc_record(dtc);
    return record ? record->status_mask : 0;
}

bool DTC_AddFreezeFrame(uint32_t dtc, const uint8_t* data, uint16_t size) {
    if (!dtc_storage.initialized || !data || size == 0 || 
        size > MAX_FREEZE_FRAME_SIZE || !validate_dtc(dtc)) {
        return false;
    }

    enter_critical(&dtc_storage.critical);

    DtcRecord* record = find_dtc_record(dtc);
    if (!record) {
        exit_critical(&dtc_storage.critical);
        return false;
    }

    if (!allocate_freeze_frame(record, size)) {
        exit_critical(&dtc_storage.critical);
        return false;
    }

    FreezeFrameRecord* ff = &record->freeze_frames[record->freeze_frame_count];
    memcpy(ff->data, data, size);
    ff->timestamp = get_system_time_ms();
    ff->record_number = record->freeze_frame_count + 1;
    record->freeze_frame_count++;

    exit_critical(&dtc_storage.critical);
    return true;
}

bool DTC_GetFreezeFrame(uint32_t dtc, uint16_t record_number, uint8_t* data, uint16_t* size) {
    if (!dtc_storage.initialized || !data || !size || !validate_dtc(dtc)) {
        return false;
    }

    enter_critical(&dtc_storage.critical);

    DtcRecord* record = find_dtc_record(dtc);
    if (!record) {
        exit_critical(&dtc_storage.critical);
        return false;
    }

    bool found = false;
    for (uint8_t i = 0; i < record->freeze_frame_count; i++) {
        if (record->freeze_frames[i].record_number == record_number) {
            memcpy(data, record->freeze_frames[i].data, 
                   record->freeze_frames[i].data_size);
            *size = record->freeze_frames[i].data_size;
            found = true;
            break;
        }
    }

    exit_critical(&dtc_storage.critical);
    return found;
}

void DTC_ClearAll(void) {
    if (!dtc_storage.initialized) {
        return;
    }

    enter_critical(&dtc_storage.critical);
    memset(dtc_storage.records, 0, sizeof(DtcRecord) * dtc_storage.max_records);
    memset(dtc_storage.freeze_frame_buffer, 0, dtc_storage.freeze_frame_buffer_size);
    dtc_storage.record_count = 0;
    exit_critical(&dtc_storage.critical);
}

bool DTC_ClearSingle(uint32_t dtc) {
    if (!dtc_storage.initialized || !validate_dtc(dtc)) {
        return false;
    }

    enter_critical(&dtc_storage.critical);

    for (uint32_t i = 0; i < dtc_storage.record_count; i++) {
        if (dtc_storage.records[i].dtc_number == dtc) {
            if (i < dtc_storage.record_count - 1) {
                memmove(&dtc_storage.records[i], &dtc_storage.records[i + 1],
                        sizeof(DtcRecord) * (dtc_storage.record_count - i - 1));
            }
            dtc_storage.record_count--;
            exit_critical(&dtc_storage.critical);
            return true;
        }
    }

    exit_critical(&dtc_storage.critical);
    return false;
}

uint32_t DTC_GetCount(void) {
    return dtc_storage.record_count;
}

bool DTC_GetRecord(uint32_t dtc, DtcRecord* record) {
    if (!dtc_storage.initialized || !record || !validate_dtc(dtc)) {
        return false;
    }

    enter_critical(&dtc_storage.critical);
    DtcRecord* found = find_dtc_record(dtc);
    if (found) {
        memcpy(record, found, sizeof(DtcRecord));
    }
    exit_critical(&dtc_storage.critical);

    return found != NULL;
}

bool DTC_GetRecordByIndex(uint32_t index, DtcRecord* record) {
    if (!dtc_storage.initialized || !record || index >= dtc_storage.record_count) {
        return false;
    }

    enter_critical(&dtc_storage.critical);
    memcpy(record, &dtc_storage.records[index], sizeof(DtcRecord));
    exit_critical(&dtc_storage.critical);

    return true;
}

void DTC_ProcessAging(void) {
    if (!dtc_storage.initialized) {
        return;
    }

    enter_critical(&dtc_storage.critical);

    for (uint32_t i = 0; i < dtc_storage.record_count; i++) {
        DtcRecord* record = &dtc_storage.records[i];
        
        if (!(record->status_mask & DTC_STATUS_CONFIRMED)) {
            record->aging_counter++;
            if (record->aging_counter >= dtc_storage.config.aging_threshold) {
                record->aged_counter++;
                record->aging_counter = 0;
                
                if (dtc_storage.config.enable_automatic_clearing && 
                    record->aged_counter >= dtc_storage.config.aging_cycle_counter) {
                    // Clear the DTC
                    if (i < dtc_storage.record_count - 1) {
                        memmove(&dtc_storage.records[i], &dtc_storage.records[i + 1],
                                sizeof(DtcRecord) * (dtc_storage.record_count - i - 1));
                    }
                    dtc_storage.record_count--;
                    i--; // Adjust index after removal
                }
            }
        }
    }

    exit_critical(&dtc_storage.critical);
}

bool DTC_SetSeverity(uint32_t dtc, DtcSeverity severity) {
    if (!dtc_storage.initialized || !validate_dtc(dtc)) {
        return false;
    }

    enter_critical(&dtc_storage.critical);
    DtcRecord* record = find_dtc_record(dtc);
    if (record) {
        record->severity = severity;
    }
    exit_critical(&dtc_storage.critical);

    return record != NULL;
}

DtcSeverity DTC_GetSeverity(uint32_t dtc) {
    if (!dtc_storage.initialized || !validate_dtc(dtc)) {
        return DTC_SEVERITY_NO_SEVERITY;
    }

    DtcRecord* record = find_dtc_record(dtc);
    return record ? record->severity : DTC_SEVERITY_NO_SEVERITY;
}

uint32_t DTC_GetOccurrenceCount(uint32_t dtc) {
    if (!dtc_storage.initialized || !validate_dtc(dtc)) {
        return 0;
    }

    DtcRecord* record = find_dtc_record(dtc);
    return record ? record->occurrence_count : 0;
}

bool DTC_IsActive(uint32_t dtc) {
    if (!dtc_storage.initialized || !validate_dtc(dtc)) {
        return false;
    }

    DtcRecord* record = find_dtc_record(dtc);
    return record ? (record->status_mask & 
           (DTC_STATUS_TEST_FAILED | DTC_STATUS_CONFIRMED)) != 0 : false;
}

void DTC_UpdateAgingCycle(void) {
    if (!dtc_storage.initialized) {
        return;
    }

    enter_critical(&dtc_storage.critical);
    DTC_ProcessAging();
    exit_critical(&dtc_storage.critical);
}
