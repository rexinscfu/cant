#include "dtc_handler.h"
#include <stdlib.h>
#include <string.h>
#include "../os/critical.h"
#include "../utils/timer.h"

struct DTCHandler {
    J1939Handler* j1939;
    DTCConfig config;
    
    struct {
        DTCRecord* records;
        size_t count;
        size_t capacity;
    } dtc_storage;
    
    struct {
        uint32_t cycle_counter;
        Timer aging_timer;
    } aging;
    
    // DM1 message management
    struct {
        Timer broadcast_timer;
        bool changes_pending;
    } dm1;
    
    CriticalSection critical;
};

static DTCRecord* find_dtc(DTCHandler* handler, uint32_t spn, uint8_t fmi) {
    for (size_t i = 0; i < handler->dtc_storage.count; i++) {
        if (handler->dtc_storage.records[i].spn == spn && 
            handler->dtc_storage.records[i].fmi == fmi) {
            return &handler->dtc_storage.records[i];
        }
    }
    return NULL;
}

static void update_dm1_message(DTCHandler* handler) {
    if (!handler->dm1.changes_pending) return;
    
    // Prepare DM1 message (SAE J1939-73)
    J1939Message msg = {
        .pgn = J1939_PGN_DM1,
        .priority = 3,
        .destination_address = 0xFF,  // Broadcast
        .length = 0
    };
    
    // First two bytes are lamp status
    msg.data[0] = 0;  // Protect lamp status
    msg.data[1] = 0;  // Amber warning lamp status
    msg.length = 2;
    
    // Add active DTCs
    for (size_t i = 0; i < handler->dtc_storage.count; i++) {
        const DTCRecord* dtc = &handler->dtc_storage.records[i];
        if (dtc->status & DTC_TEST_FAILED) {
            // Each DTC takes 4 bytes in J1939 format
            msg.data[msg.length++] = dtc->spn & 0xFF;
            msg.data[msg.length++] = (dtc->spn >> 8) & 0xFF;
            msg.data[msg.length++] = ((dtc->spn >> 16) & 0x0F) | 
                                   (dtc->fmi << 4);
            msg.data[msg.length++] = dtc->occurrence_count;
        }
    }
    
    j1939_transmit(handler->j1939, &msg);
    handler->dm1.changes_pending = false;
}

DTCHandler* dtc_create(J1939Handler* j1939, const DTCConfig* config) {
    if (!j1939 || !config || config->max_dtcs == 0) return NULL;
    
    DTCHandler* handler = calloc(1, sizeof(DTCHandler));
    if (!handler) return NULL;
    
    handler->j1939 = j1939;
    memcpy(&handler->config, config, sizeof(DTCConfig));
    
    // Allocate DTC storage
    handler->dtc_storage.records = calloc(config->max_dtcs, sizeof(DTCRecord));
    handler->dtc_storage.capacity = config->max_dtcs;
    
    if (!handler->dtc_storage.records) {
        free(handler);
        return NULL;
    }
    
    init_critical(&handler->critical);
    timer_start(&handler->dm1.broadcast_timer, 1000);  // 1s broadcast rate
    
    return handler;
}

void dtc_destroy(DTCHandler* handler) {
    if (!handler) return;
    
    free(handler->dtc_storage.records);
    destroy_critical(&handler->critical);
    free(handler);
}

void dtc_process(DTCHandler* handler) {
    if (!handler) return;
    
    enter_critical(&handler->critical);
    
    // Process aging
    if (timer_expired(&handler->aging.aging_timer)) {
        handler->aging.cycle_counter++;
        
        // Process aging for each DTC
        for (size_t i = 0; i < handler->dtc_storage.count; i++) {
            DTCRecord* dtc = &handler->dtc_storage.records[i];
            
            // Auto-clear aged DTCs
            if (handler->config.auto_clear && 
                (dtc->status & DTC_TEST_NOT_COMPLETED) &&
                handler->aging.cycle_counter >= handler->config.aging_cycles) {
                
                // Remove DTC by shifting array
                memmove(&handler->dtc_storage.records[i],
                       &handler->dtc_storage.records[i + 1],
                       (handler->dtc_storage.count - i - 1) * sizeof(DTCRecord));
                handler->dtc_storage.count--;
                i--;  // Adjust index after removal
                
                handler->dm1.changes_pending = true;
            }
        }
        
        timer_start(&handler->aging.aging_timer, 1000);  // 1s aging cycle
    }
    
    // Broadcast DM1 message if needed
    if (timer_expired(&handler->dm1.broadcast_timer)) {
        update_dm1_message(handler);
        timer_start(&handler->dm1.broadcast_timer, 1000);
    }
    
    exit_critical(&handler->critical);
}

bool dtc_set_status(DTCHandler* handler, uint32_t spn, uint8_t fmi, uint8_t status) {
    if (!handler) return false;
    
    enter_critical(&handler->critical);
    
    DTCRecord* dtc = find_dtc(handler, spn, fmi);
    if (!dtc) {
        // Create new DTC if space available
        if (handler->dtc_storage.count >= handler->dtc_storage.capacity) {
            exit_critical(&handler->critical);
            return false;
        }
        
        dtc = &handler->dtc_storage.records[handler->dtc_storage.count++];
        dtc->spn = spn;
        dtc->fmi = fmi;
        dtc->occurrence_count = 1;
    }
    
    if (dtc->status != status) {
        dtc->status = status;
        if (status & DTC_TEST_FAILED) {
            dtc->occurrence_count++;
        }
        handler->dm1.changes_pending = true;
    }
    
    exit_critical(&handler->critical);
    return true;
}

bool dtc_get_record(DTCHandler* handler, uint32_t spn, uint8_t fmi, DTCRecord* record) {
    if (!handler || !record) return false;
    
    enter_critical(&handler->critical);
    
    DTCRecord* dtc = find_dtc(handler, spn, fmi);
    if (dtc) {
        memcpy(record, dtc, sizeof(DTCRecord));
        exit_critical(&handler->critical);
        return true;
    }
    
    exit_critical(&handler->critical);
    return false;
}

bool dtc_clear_all(DTCHandler* handler) {
    if (!handler) return false;
    
    enter_critical(&handler->critical);
    handler->dtc_storage.count = 0;
    handler->dm1.changes_pending = true;
    exit_critical(&handler->critical);
    
    return true;
}

size_t dtc_get_count(const DTCHandler* handler) {
    return handler ? handler->dtc_storage.count : 0;
}

bool dtc_get_records(DTCHandler* handler, DTCRecord* records, size_t* count) {
    if (!handler || !records || !count || *count == 0) return false;
    
    enter_critical(&handler->critical);
    
    size_t copy_count = handler->dtc_storage.count;
    if (copy_count > *count) {
        copy_count = *count;
    }
    
    memcpy(records, handler->dtc_storage.records, 
           copy_count * sizeof(DTCRecord));
    *count = copy_count;
    
    exit_critical(&handler->critical);
    return true;
} 