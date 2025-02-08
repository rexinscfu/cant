// Example diagnostic system configuration
#include "diagnostic/diag_system.h"
#include "diagnostic/service_router.h"
#include "diagnostic/routine_manager.h"
#include "diagnostic/data_manager.h"

// Example service handlers
static UdsResponseCode handle_ecu_reset(const UdsMessage* request, UdsMessage* response) {
    // Implementation of ECU reset service
    switch (request->data[0]) {
        case 0x01: // Hard reset
            // Perform hard reset
            return UDS_RESPONSE_OK;
        case 0x02: // Key off/on reset
            // Perform key cycle reset
            return UDS_RESPONSE_OK;
        case 0x03: // Soft reset
            // Perform soft reset
            return UDS_RESPONSE_OK;
        default:
            return UDS_RESPONSE_SUB_FUNCTION_NOT_SUPPORTED;
    }
}

// Example routine control handlers
static bool start_self_test_routine(const uint8_t* data, uint16_t length) {
    // Implementation of self test routine
    return true;
}

static bool get_self_test_result(RoutineResult* result) {
    static uint8_t test_results[] = {0x01, 0x00, 0x00}; // Example result data
    result->result_code = 0;
    result->data = test_results;
    result->length = sizeof(test_results);
    return true;
}

// Example data identifier handlers
static bool read_vin(uint16_t did, uint8_t* data, uint16_t* length) {
    static const uint8_t vin[] = "EXAMPLEVIN123456";
    memcpy(data, vin, sizeof(vin));
    *length = sizeof(vin);
    return true;
}

// Service routes configuration
static const ServiceRoute service_routes[] = {
    {UDS_SID_DIAGNOSTIC_SESSION_CONTROL, handle_session_control, false, 0},
    {UDS_SID_ECU_RESET, handle_ecu_reset, true, 1},
    {UDS_SID_SECURITY_ACCESS, handle_security_access, false, 0},
    {UDS_SID_READ_DATA_BY_IDENTIFIER, handle_read_data_by_id, false, 0},
    {UDS_SID_ROUTINE_CONTROL, handle_routine_control, true, 1}
};

// Routine definitions
static const RoutineDefinition routines[] = {
    {
        .routine_id = 0x0100,
        .security_level = 1,
        .timeout_ms = 5000,
        .start_routine = start_self_test_routine,
        .stop_routine = NULL,
        .get_result = get_self_test_result
    },
    // Add more routines as needed
};

// Data identifier definitions
static const DataIdentifier data_identifiers[] = {
    {
        .did = 0xF190, // VIN
        .type = DATA_TYPE_STRING,
        .length = 17,
        .access_rights = DATA_ACCESS_READ,
        .security_level = 0,
        .scaling = SCALING_NONE,
        .read_handler = read_vin,
        .write_handler = NULL
    },
    // Add more DIDs as needed
};

// Complete diagnostic system configuration
const DiagSystemConfig example_config = {
    .transport_config = {
        .protocol = DIAG_PROTOCOL_UDS,
        .max_message_length = 4096,
        .p2_timeout_ms = 50,
        .p2_star_timeout_ms = 5000,
        .transmit_callback = NULL, // Set in initialization
        .receive_callback = NULL   // Set in initialization
    },
    .session_config = {
        .default_p2_timeout_ms = 50,
        .extended_p2_timeout_ms = 5000,
        .s3_timeout_ms = 5000,
        .enable_session_lock = true,
        .session_change_callback = NULL, // Set in initialization
        .security_change_callback = NULL // Set in initialization
    },
    .router_config = {
        .routes = service_routes,
        .route_count = sizeof(service_routes) / sizeof(service_routes[0]),
        .pre_process_callback = NULL,
        .post_process_callback = NULL
    },
    .routine_config = {
        .routines = routines,
        .routine_count = sizeof(routines) / sizeof(routines[0]),
        .status_callback = NULL,
        .error_callback = NULL
    },
    .data_config = {
        .identifiers = data_identifiers,
        .identifier_count = sizeof(data_identifiers) / sizeof(data_identifiers[0]),
        .access_callback = NULL
    },
    .security_config = {
        .levels = NULL, // Configure security levels separately
        .level_count = 0,
        .default_delay_time_ms = 10000,
        .default_max_attempts = 3,
        .security_callback = NULL,
        .violation_callback = NULL
    },
    .memory_config = {
        .block_size = 512,
        .max_block_count = 256,
        .verify_callback = NULL,
        .error_callback = NULL
    },
    .comm_config = {
        .channels = NULL, // Configure communication channels separately
        .channel_count = 0,
        .error_callback = NULL,
        .state_change_callback = NULL
    }
}; 