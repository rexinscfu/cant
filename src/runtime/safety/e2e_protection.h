#ifndef CANT_E2E_PROTECTION_H
#define CANT_E2E_PROTECTION_H

#include <stdint.h>
#include <stdbool.h>

// E2E Profile Types
typedef enum {
    E2E_PROFILE_1,    // CRC-8/SAE J1850
    E2E_PROFILE_2,    // CRC-16/CCITT
    E2E_PROFILE_4,    // CRC-32/AUTOSAR
    E2E_PROFILE_5,    // CRC-64/ISO
    E2E_PROFILE_6     // Custom profile
} E2EProfileType;

// E2E State Machine States
typedef enum {
    E2E_STATE_INIT,
    E2E_STATE_VALID,
    E2E_STATE_INVALID,
    E2E_STATE_ERROR
} E2EStateType;

// E2E Check Status
typedef enum {
    E2E_OK,
    E2E_ERROR_CRC,
    E2E_ERROR_SEQUENCE,
    E2E_ERROR_REPEATED,
    E2E_ERROR_TIMEOUT,
    E2E_ERROR_WRONG_LENGTH,
    E2E_ERROR_CONFIG
} E2EStatusType;

// E2E Configuration
typedef struct {
    E2EProfileType profile;
    uint16_t data_id;
    uint16_t min_length;
    uint16_t max_length;
    uint16_t max_delta_counter;
    uint32_t timeout_ms;
    bool include_length;
} E2EConfig;

// E2E Runtime State
typedef struct {
    uint32_t sequence_counter;
    uint32_t last_timestamp;
    E2EStateType state;
    uint8_t error_count;
    bool initialized;
} E2EState;

// E2E Protected Data
typedef struct {
    uint8_t* data;
    uint16_t length;
    uint32_t crc;
    uint32_t sequence;
    uint16_t data_id;
} E2EProtectedData;

// E2E API
bool E2E_Init(E2EState* state, const E2EConfig* config);
E2EStatusType E2E_Protect(const E2EConfig* config, E2EState* state, E2EProtectedData* data);
E2EStatusType E2E_Check(const E2EConfig* config, E2EState* state, const E2EProtectedData* data);
E2EStateType E2E_GetState(const E2EState* state);
void E2E_Reset(E2EState* state);

#endif // CANT_E2E_PROTECTION_H 