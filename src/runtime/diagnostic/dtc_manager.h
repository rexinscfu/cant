#ifndef CANT_DTC_MANAGER_H
#define CANT_DTC_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

// DTC Status Mask
typedef enum {
    DTC_STATUS_TEST_FAILED            = 0x01,
    DTC_STATUS_TEST_FAILED_THIS_OP    = 0x02,
    DTC_STATUS_PENDING                = 0x04,
    DTC_STATUS_CONFIRMED              = 0x08,
    DTC_STATUS_TEST_NOT_COMPLETED     = 0x10,
    DTC_STATUS_TEST_FAILED_SINCE_CLEAR = 0x20,
    DTC_STATUS_TEST_NOT_COMPLETED_SINCE_CLEAR = 0x40,
    DTC_STATUS_WARNING_INDICATOR_REQUESTED = 0x80
} DtcStatusMask;

// DTC Severity
typedef enum {
    DTC_SEVERITY_NO_SEVERITY = 0x00,
    DTC_SEVERITY_CHECK_AT_NEXT_HALT = 0x20,
    DTC_SEVERITY_CHECK_IMMEDIATELY = 0x40,
    DTC_SEVERITY_MAINTENANCE_ONLY = 0x60,
    DTC_SEVERITY_CHECK_AT_NEXT_SERVICE = 0x80,
    DTC_SEVERITY_SAFETY_CRITICAL = 0xE0
} DtcSeverity;

// Freeze Frame Data
typedef struct {
    uint32_t timestamp;
    uint16_t record_number;
    uint8_t* data;
    uint16_t data_size;
} FreezeFrameRecord;

// DTC Record
typedef struct {
    uint32_t dtc_number;
    uint8_t status_mask;
    DtcSeverity severity;
    uint32_t first_occurrence;
    uint32_t last_occurrence;
    uint32_t occurrence_count;
    uint32_t aging_counter;
    uint32_t aged_counter;
    FreezeFrameRecord* freeze_frames;
    uint8_t freeze_frame_count;
} DtcRecord;

// DTC Configuration
typedef struct {
    uint32_t max_dtc_count;
    uint32_t max_freeze_frames_per_dtc;
    uint32_t aging_threshold;
    uint32_t aging_cycle_counter;
    bool enable_automatic_clearing;
    void (*status_change_callback)(uint32_t dtc, uint8_t old_status, uint8_t new_status);
} DtcConfig;

// DTC Manager API
bool DTC_Init(const DtcConfig* config);
void DTC_DeInit(void);
bool DTC_SetStatus(uint32_t dtc, uint8_t status_mask);
uint8_t DTC_GetStatus(uint32_t dtc);
bool DTC_AddFreezeFrame(uint32_t dtc, const uint8_t* data, uint16_t size);
bool DTC_GetFreezeFrame(uint32_t dtc, uint16_t record_number, uint8_t* data, uint16_t* size);
void DTC_ClearAll(void);
bool DTC_ClearSingle(uint32_t dtc);
uint32_t DTC_GetCount(void);
bool DTC_GetRecord(uint32_t dtc, DtcRecord* record);
bool DTC_GetRecordByIndex(uint32_t index, DtcRecord* record);
void DTC_ProcessAging(void);
bool DTC_SetSeverity(uint32_t dtc, DtcSeverity severity);
DtcSeverity DTC_GetSeverity(uint32_t dtc);
uint32_t DTC_GetOccurrenceCount(uint32_t dtc);
bool DTC_IsActive(uint32_t dtc);
void DTC_UpdateAgingCycle(void);

#endif // CANT_DTC_MANAGER_H 