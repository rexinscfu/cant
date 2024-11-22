#include "e2e_protection.h"
#include <string.h>
#include "../utils/crc.h"
#include "../utils/timer.h"

// CRC Calculation tables
static const uint8_t CRC8_TABLE[256] = {
    0x00, 0x1D, 0x3A, 0x27, 0x74, 0x69, 0x4E, 0x53,
    0xE8, 0xF5, 0xD2, 0xCF, 0x9C, 0x81, 0xA6, 0xBB,
    
};

static const uint16_t CRC16_TABLE[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    
};

static const uint32_t CRC32_TABLE[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
    0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    
};

// Internal helper functions
static uint32_t calculate_crc(E2EProfileType profile, const uint8_t* data, uint16_t length) {
    uint32_t crc = 0xFFFFFFFF;
    
    switch (profile) {
        case E2E_PROFILE_1:
            for (uint16_t i = 0; i < length; i++) {
                crc = CRC8_TABLE[crc ^ data[i]];
            }
            return crc;
            
        case E2E_PROFILE_2:
            for (uint16_t i = 0; i < length; i++) {
                crc = (crc << 8) ^ CRC16_TABLE[((crc >> 8) ^ data[i]) & 0xFF];
            }
            return crc & 0xFFFF;
            
        case E2E_PROFILE_4:
            for (uint16_t i = 0; i < length; i++) {
                crc = (crc << 8) ^ CRC32_TABLE[((crc >> 24) ^ data[i]) & 0xFF];
            }
            return crc;
            
        case E2E_PROFILE_5:
            // TODOImplement CRC-64 calculation
            return 0;
            
        case E2E_PROFILE_6:
            // TODOImplement custom CRC calculation
            return 0;
            
        default:
            return 0;
    }
}

static bool validate_config(const E2EConfig* config) {
    if (!config) return false;
    
    if (config->min_length > config->max_length) return false;
    if (config->max_length == 0) return false;
    if (config->timeout_ms == 0) return false;
    
    return true;
}

static bool validate_data(const E2EConfig* config, const E2EProtectedData* data) {
    if (!config || !data || !data->data) return false;
    
    if (data->length < config->min_length || 
        data->length > config->max_length) return false;
        
    return true;
}

bool E2E_Init(E2EState* state, const E2EConfig* config) {
    if (!state || !validate_config(config)) return false;
    
    memset(state, 0, sizeof(E2EState));
    state->state = E2E_STATE_INIT;
    state->initialized = true;
    
    return true;
}

E2EStatusType E2E_Protect(const E2EConfig* config, E2EState* state, 
                         E2EProtectedData* data) {
    if (!state || !state->initialized || 
        !validate_config(config) || 
        !validate_data(config, data)) {
        return E2E_ERROR_CONFIG;
    }
    
    // Update sequence counter
    state->sequence_counter = (state->sequence_counter + 1) % config->max_delta_counter;
    data->sequence = state->sequence_counter;
    
    // Update data ID
    data->data_id = config->data_id;
    
    // Calculate CRC over header and data
    uint8_t header[8];
    uint16_t header_size = 0;
    
    // Build header
    header[header_size++] = (data->data_id >> 8) & 0xFF;
    header[header_size++] = data->data_id & 0xFF;
    header[header_size++] = (data->sequence >> 24) & 0xFF;
    header[header_size++] = (data->sequence >> 16) & 0xFF;
    header[header_size++] = (data->sequence >> 8) & 0xFF;
    header[header_size++] = data->sequence & 0xFF;
    
    if (config->include_length) {
        header[header_size++] = (data->length >> 8) & 0xFF;
        header[header_size++] = data->length & 0xFF;
    }
    
    // Calculate CRC
    uint32_t crc = calculate_crc(config->profile, header, header_size);
    crc = calculate_crc(config->profile, data->data, data->length);
    data->crc = crc;
    
    // Update state
    state->last_timestamp = get_system_time_ms();
    state->state = E2E_STATE_VALID;
    
    return E2E_OK;
}

E2EStatusType E2E_Check(const E2EConfig* config, E2EState* state, 
                       const E2EProtectedData* data) {
    if (!state || !state->initialized || 
        !validate_config(config) || 
        !validate_data(config, data)) {
        return E2E_ERROR_CONFIG;
    }
    
    // Check timeout
    uint32_t current_time = get_system_time_ms();
    if (current_time - state->last_timestamp > config->timeout_ms) {
        state->state = E2E_STATE_ERROR;
        return E2E_ERROR_TIMEOUT;
    }
    
    // Verify data ID
    if (data->data_id != config->data_id) {
        state->state = E2E_STATE_ERROR;
        return E2E_ERROR_CONFIG;
    }
    
    // Check sequence counter
    uint32_t expected_sequence = (state->sequence_counter + 1) % config->max_delta_counter;
    if (data->sequence != expected_sequence) {
        state->state = E2E_STATE_INVALID;
        return E2E_ERROR_SEQUENCE;
    }
    
    // Verify CRC
    uint8_t header[8];
    uint16_t header_size = 0;
    
    // Rebuild header
    header[header_size++] = (data->data_id >> 8) & 0xFF;
    header[header_size++] = data->data_id & 0xFF;
    header[header_size++] = (data->sequence >> 24) & 0xFF;
    header[header_size++] = (data->sequence >> 16) & 0xFF;
    header[header_size++] = (data->sequence >> 8) & 0xFF;
    header[header_size++] = data->sequence & 0xFF;
    
    if (config->include_length) {
        header[header_size++] = (data->length >> 8) & 0xFF;
        header[header_size++] = data->length & 0xFF;
    }
    
    uint32_t calculated_crc = calculate_crc(config->profile, header, header_size);
    calculated_crc = calculate_crc(config->profile, data->data, data->length);
    
    if (calculated_crc != data->crc) {
        state->state = E2E_STATE_INVALID;
        return E2E_ERROR_CRC;
    }
    
    // Update state
    state->sequence_counter = data->sequence;
    state->last_timestamp = current_time;
    state->state = E2E_STATE_VALID;
    
    return E2E_OK;
}

E2EStateType E2E_GetState(const E2EState* state) {
    return state ? state->state : E2E_STATE_ERROR;
}

void E2E_Reset(E2EState* state) {
    if (state) {
        state->sequence_counter = 0;
        state->last_timestamp = 0;
        state->state = E2E_STATE_INIT;
        state->error_count = 0;
    }
} 