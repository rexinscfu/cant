#ifndef CANT_DTC_HANDLER_H
#define CANT_DTC_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include "../protocols/j1939.h"

// DTC status byte bits
#define DTC_TEST_FAILED_THIS_CYCLE    (1 << 0)
#define DTC_TEST_FAILED               (1 << 1)
#define DTC_PENDING                   (1 << 2)
#define DTC_CONFIRMED                 (1 << 3)
#define DTC_TEST_NOT_COMPLETED        (1 << 4)
#define DTC_PREVIOUSLY_ACTIVE         (1 << 5)
#define DTC_TEST_FAILED_SINCE_CLEAR   (1 << 6)
#define DTC_WARNING_INDICATOR_REQ     (1 << 7)

// DTC severity levels
typedef enum {
    DTC_SEVERITY_NO_SEVERITY = 0,
    DTC_SEVERITY_MAINTENANCE_ONLY = 1,
    DTC_SEVERITY_CHECK_AT_NEXT_HALT = 2,
    DTC_SEVERITY_CHECK_IMMEDIATELY = 3
} DTCSeverity;

// DTC record structure
typedef struct {
    uint32_t spn;
    uint8_t fmi;
    uint8_t occurrence_count;
    uint8_t status;
    DTCSeverity severity;
} DTCRecord;

// DTC handler configuration
typedef struct {
    size_t max_dtcs;
    bool support_freezeframe;
    uint32_t aging_cycles;
    bool auto_clear;
} DTCConfig;

typedef struct DTCHandler DTCHandler;

// DTC Handler API
DTCHandler* dtc_create(J1939Handler* j1939, const DTCConfig* config);
void dtc_destroy(DTCHandler* handler);
void dtc_process(DTCHandler* handler);
bool dtc_set_status(DTCHandler* handler, uint32_t spn, uint8_t fmi, uint8_t status);
bool dtc_get_record(DTCHandler* handler, uint32_t spn, uint8_t fmi, DTCRecord* record);
bool dtc_clear_all(DTCHandler* handler);
size_t dtc_get_count(const DTCHandler* handler);
bool dtc_get_records(DTCHandler* handler, DTCRecord* records, size_t* count);

#endif // CANT_DTC_HANDLER_H 