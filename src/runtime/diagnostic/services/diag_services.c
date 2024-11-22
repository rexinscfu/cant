#include "diag_services.h"
#include "../session_manager.h"
#include "../security_manager.h"
#include "../memory_manager.h"
#include "../data_manager.h"
#include "../routine_manager.h"
#include "../../os/critical.h"
#include "../../utils/timer.h"
#include <string.h>

UdsResponseCode handle_diagnostic_session_control(const UdsMessage* request, UdsMessage* response) {
    if (request->length < 2) {
        return UDS_RESPONSE_INVALID_FORMAT;
    }

    uint8_t session_type = request->data[1];
    bool success = false;

    switch (session_type) {
        case UDS_SESSION_DEFAULT:
            success = Session_Manager_ChangeSession(UDS_SESSION_DEFAULT);
            break;
        case UDS_SESSION_PROGRAMMING:
            if (Security_Manager_IsLevelUnlocked(SECURITY_LEVEL_PROGRAMMING)) {
                success = Session_Manager_ChangeSession(UDS_SESSION_PROGRAMMING);
            }
            break;
        case UDS_SESSION_EXTENDED:
            if (Security_Manager_IsLevelUnlocked(SECURITY_LEVEL_EXTENDED)) {
                success = Session_Manager_ChangeSession(UDS_SESSION_EXTENDED);
            }
            break;
        case UDS_SESSION_SAFETY:
            if (Security_Manager_IsLevelUnlocked(SECURITY_LEVEL_SAFETY)) {
                success = Session_Manager_ChangeSession(UDS_SESSION_SAFETY);
            }
            break;
        default:
            return UDS_RESPONSE_SUB_FUNCTION_NOT_SUPPORTED;
    }

    if (!success) {
        return UDS_RESPONSE_CONDITIONS_NOT_CORRECT;
    }

    response->data[0] = session_type;
    response->data[1] = (uint8_t)(Session_Manager_GetP2Timeout() >> 8);
    response->data[2] = (uint8_t)(Session_Manager_GetP2Timeout() & 0xFF);
    response->data[3] = (uint8_t)(Session_Manager_GetP2StarTimeout() >> 8);
    response->data[4] = (uint8_t)(Session_Manager_GetP2StarTimeout() & 0xFF);
    response->length = 5;

    return UDS_RESPONSE_OK;
}

UdsResponseCode handle_ecu_reset(const UdsMessage* request, UdsMessage* response) {
    if (request->length < 2) {
        return UDS_RESPONSE_INVALID_FORMAT;
    }

    uint8_t reset_type = request->data[1];
    bool success = false;

    switch (reset_type) {
        case 0x01: // Hard Reset
            success = System_PerformHardReset();
            break;
        case 0x02: // Key Off/On Reset
            success = System_PerformKeyReset();
            break;
        case 0x03: // Soft Reset
            success = System_PerformSoftReset();
            break;
        case 0x04: // Enable Rapid Power Shutdown
            success = System_EnableRapidPowerdown();
            break;
        case 0x05: // Disable Rapid Power Shutdown
            success = System_DisableRapidPowerdown();
            break;
        default:
            return UDS_RESPONSE_SUB_FUNCTION_NOT_SUPPORTED;
    }

    if (!success) {
        return UDS_RESPONSE_CONDITIONS_NOT_CORRECT;
    }

    response->data[0] = reset_type;
    response->length = 1;

    return UDS_RESPONSE_OK;
}

UdsResponseCode handle_security_access(const UdsMessage* request, UdsMessage* response) {
    if (request->length < 2) {
        return UDS_RESPONSE_INVALID_FORMAT;
    }

    uint8_t sub_function = request->data[1];
    uint8_t security_level = sub_function & 0xFE;
    bool is_request_seed = (sub_function & 0x01) == 0x01;

    if (is_request_seed) {
        uint8_t seed[32];
        uint16_t seed_length = 0;

        if (!Security_Manager_RequestSeed(security_level, seed, &seed_length)) {
            return UDS_RESPONSE_CONDITIONS_NOT_CORRECT;
        }

        response->data[0] = sub_function;
        memcpy(&response->data[1], seed, seed_length);
        response->length = seed_length + 1;
    } else {
        if (request->length < 3) {
            return UDS_RESPONSE_INVALID_FORMAT;
        }

        uint16_t key_length = request->length - 2;
        if (!Security_Manager_ValidateKey(security_level, &request->data[2], key_length)) {
            return UDS_RESPONSE_INVALID_KEY;
        }

        response->data[0] = sub_function;
        response->length = 1;
    }

    return UDS_RESPONSE_OK;
}

UdsResponseCode handle_communication_control(const UdsMessage* request, UdsMessage* response) {
    if (request->length < 3) {
        return UDS_RESPONSE_INVALID_FORMAT;
    }

    uint8_t control_type = request->data[1];
    uint8_t comm_type = request->data[2];

    switch (control_type) {
        case 0x00: // Enable RX and TX
            Comm_Manager_ControlCommunication(comm_type, COMM_CONTROL_ENABLE_RX_TX);
            break;
        case 0x01: // Enable RX, Disable TX
            Comm_Manager_ControlCommunication(comm_type, COMM_CONTROL_ENABLE_RX_DISABLE_TX);
            break;
        case 0x02: // Disable RX, Enable TX
            Comm_Manager_ControlCommunication(comm_type, COMM_CONTROL_DISABLE_RX_ENABLE_TX);
            break;
        case 0x03: // Disable RX and TX
            Comm_Manager_ControlCommunication(comm_type, COMM_CONTROL_DISABLE_RX_TX);
            break;
        default:
            return UDS_RESPONSE_SUB_FUNCTION_NOT_SUPPORTED;
    }

    response->data[0] = control_type;
    response->length = 1;

    return UDS_RESPONSE_OK;
}

UdsResponseCode handle_read_data_by_id(const UdsMessage* request, UdsMessage* response) {
    if (request->length < 3) {
        return UDS_RESPONSE_INVALID_FORMAT;
    }

    uint16_t did = (request->data[1] << 8) | request->data[2];
    uint8_t data[MAX_DID_SIZE];
    uint16_t length = 0;

    if (!validate_data_identifier(did)) {
        return UDS_RESPONSE_REQUEST_OUT_OF_RANGE;
    }

    if (!Data_Manager_ReadDataById(did, data, &length)) {
        return UDS_RESPONSE_CONDITIONS_NOT_CORRECT;
    }

    response->data[0] = request->data[1];
    response->data[1] = request->data[2];
    memcpy(&response->data[2], data, length);
    response->length = length + 2;

    return UDS_RESPONSE_OK;
}

UdsResponseCode handle_write_data_by_id(const UdsMessage* request, UdsMessage* response) {
    if (request->length < 4) {
        return UDS_RESPONSE_INVALID_FORMAT;
    }

    uint16_t did = (request->data[1] << 8) | request->data[2];
    uint16_t data_length = request->length - 3;

    if (!validate_data_identifier(did)) {
        return UDS_RESPONSE_REQUEST_OUT_OF_RANGE;
    }

    if (!Data_Manager_WriteDataById(did, &request->data[3], data_length)) {
        return UDS_RESPONSE_CONDITIONS_NOT_CORRECT;
    }

    response->data[0] = request->data[1];
    response->data[1] = request->data[2];
    response->length = 2;

    return UDS_RESPONSE_OK;
}

UdsResponseCode handle_routine_control(const UdsMessage* request, UdsMessage* response) {
    if (request->length < 4) {
        return UDS_RESPONSE_INVALID_FORMAT;
    }

    uint8_t sub_function = request->data[1];
    uint16_t routine_id = (request->data[2] << 8) | request->data[3];
    uint16_t data_length = request->length - 4;

    if (!validate_routine_id(routine_id)) {
        return UDS_RESPONSE_REQUEST_OUT_OF_RANGE;
    }

    bool success = false;
    RoutineResult result = {0};

    switch (sub_function) {
        case 0x01: // Start Routine
            success = Routine_Manager_StartRoutine(routine_id, &request->data[4], data_length);
            break;
        case 0x02: // Stop Routine
            success = Routine_Manager_StopRoutine(routine_id);
            break;
        case 0x03: // Request Results
            success = Routine_Manager_GetResult(routine_id, &result);
            break;
        default:
            return UDS_RESPONSE_SUB_FUNCTION_NOT_SUPPORTED;
    }

    if (!success) {
        return UDS_RESPONSE_CONDITIONS_NOT_CORRECT;
    }

    response->data[0] = sub_function;
    response->data[1] = request->data[2];
    response->data[2] = request->data[3];

    if (sub_function == 0x03 && result.length > 0) {
        memcpy(&response->data[3], result.data, result.length);
        response->length = result.length + 3;
    } else {
        response->length = 3;
    }

    return UDS_RESPONSE_OK;
}

UdsResponseCode handle_request_download(const UdsMessage* request, UdsMessage* response) {
    if (request->length < 4) {
        return UDS_RESPONSE_INVALID_FORMAT;
    }

    uint8_t address_size = (request->data[1] >> 4) & 0x0F;
    uint8_t length_size = request->data[1] & 0x0F;
    uint8_t format = request->data[2];

    if (request->length < (3 + address_size + length_size)) {
        return UDS_RESPONSE_INVALID_FORMAT;
    }

    uint32_t address = 0;
    uint32_t size = 0;

    for (uint8_t i = 0; i < address_size; i++) {
        address = (address << 8) | request->data[3 + i];
    }

    for (uint8_t i = 0; i < length_size; i++) {
        size = (size << 8) | request->data[3 + address_size + i];
    }

    if (!validate_memory_range(address, size)) {
        return UDS_RESPONSE_REQUEST_OUT_OF_RANGE;
    }

    uint32_t max_block_length = Memory_Manager_GetMaxBlockSize();
    if (!Memory_Manager_StartDownload(address, size, format)) {
        return UDS_RESPONSE_CONDITIONS_NOT_CORRECT;
    }

    response->data[0] = (uint8_t)(max_block_length >> 8);
    response->data[1] = (uint8_t)(max_block_length & 0xFF);
    response->length = 2;

    return UDS_RESPONSE_OK;
}

UdsResponseCode handle_transfer_data(const UdsMessage* request, UdsMessage* response) {
    if (request->length < 2) {
        return UDS_RESPONSE_INVALID_FORMAT;
    }

    uint8_t block_sequence = request->data[1];
    uint16_t data_length = request->length - 2;

    if (!Memory_Manager_TransferBlock(block_sequence, &request->data[2], data_length)) {
        return UDS_RESPONSE_REQUEST_OUT_OF_RANGE;
    }

    response->data[0] = block_sequence;
    response->length = 1;

    return UDS_RESPONSE_OK;
}

UdsResponseCode handle_request_transfer_exit(const UdsMessage* request, UdsMessage* response) {
    if (!Memory_Manager_CompleteTransfer()) {
        return UDS_RESPONSE_CONDITIONS_NOT_CORRECT;
    }

    response->length = 0;
    return UDS_RESPONSE_OK;
}

UdsResponseCode handle_tester_present(const UdsMessage* request, UdsMessage* response) {
    if (request->length < 2) {
        return UDS_RESPONSE_INVALID_FORMAT;
    }

    uint8_t sub_function = request->data[1];
    
    if (sub_function != 0x00) {
        return UDS_RESPONSE_SUB_FUNCTION_NOT_SUPPORTED;
    }

    Session_Manager_UpdateActivity();
    response->data[0] = sub_function;
    response->length = 1;

    return UDS_RESPONSE_OK;
}

UdsResponseCode handle_read_memory_by_address(const UdsMessage* request, UdsMessage* response) {
    if (request->length < 4) {
        return UDS_RESPONSE_INVALID_FORMAT;
    }

    uint8_t address_size = (request->data[1] >> 4) & 0x0F;
    uint8_t length_size = request->data[1] & 0x0F;

    if (request->length != (3 + address_size + length_size)) {
        return UDS_RESPONSE_INVALID_FORMAT;
    }

    uint32_t address = 0;
    uint32_t size = 0;

    for (uint8_t i = 0; i < address_size; i++) {
        address = (address << 8) | request->data[2 + i];
    }

    for (uint8_t i = 0; i < length_size; i++) {
        size = (size << 8) | request->data[2 + address_size + i];
    }

    if (!validate_memory_range(address, size)) {
        return UDS_RESPONSE_REQUEST_OUT_OF_RANGE;
    }

    if (!Memory_Manager_ReadMemory(address, &response->data[1], size)) {
        return UDS_RESPONSE_CONDITIONS_NOT_CORRECT;
    }

    response->length = size + 1;
    return UDS_RESPONSE_OK;
}

// System control function implementations
bool System_PerformHardReset(void) {
    // Save critical data
    Data_Manager_SavePermanentData();
    
    // Disable all communications
    Comm_Manager_ControlCommunication(0xFF, COMM_CONTROL_DISABLE_RX_TX);
    
    // Reset security and session
    Security_Manager_LockLevel(0xFF);
    Session_Manager_ChangeSession(UDS_SESSION_DEFAULT);
    
    // Perform hardware reset
    NVIC_SystemReset();
    return true;
}

bool System_PerformKeyReset(void) {
    // Similar to hard reset but simulates key cycle
    Data_Manager_SavePermanentData();
    
    // Simulate power cycle sequence
    System_PowerDown();
    Timer_DelayMs(100);
    System_PowerUp();
    
    return true;
}

bool System_PerformSoftReset(void) {
    // Software reset without power cycle
    Session_Manager_ChangeSession(UDS_SESSION_DEFAULT);
    Security_Manager_LockLevel(0xFF);
    
    // Reset all managers to default state
    Routine_Manager_AbortAll();
    Memory_Manager_Reset();
    Data_Manager_Reset();
    
    return true;
}

// Validation helper functions
bool validate_memory_range(uint32_t address, uint32_t size) {
    // Check if address and size are within valid ranges
    if (address < MEMORY_START_ADDRESS || 
        (address + size) > MEMORY_END_ADDRESS ||
        size > MAX_MEMORY_BLOCK_SIZE) {
        return false;
    }

    // Check if memory range is protected
    for (uint32_t i = 0; i < PROTECTED_RANGES_COUNT; i++) {
        if (address >= protected_ranges[i].start && 
            (address + size) <= protected_ranges[i].end) {
            return false;
        }
    }

    return true;
}

bool validate_data_identifier(uint16_t did) {
    return Data_Manager_GetDataIdentifier(did) != NULL;
}

bool validate_routine_id(uint16_t rid) {
    return Routine_Manager_GetRoutine(rid) != NULL;
}

bool validate_io_parameters(uint8_t control_type, const uint8_t* params, uint16_t length) {
    // Validate I/O control parameters based on control type
    switch (control_type) {
        case 0x00: // Return Control to ECU
            return length == 0;
        case 0x01: // Reset to Default
            return length == 0;
        case 0x02: // Freeze Current State
            return length == 0;
        case 0x03: // Short Term Adjustment
            return length >= 1;
        default:
            return false;
    }
}