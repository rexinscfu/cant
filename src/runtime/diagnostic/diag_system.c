#include "diag_system.h"
#include <string.h>
#include "../os/critical.h"

typedef struct {
    DiagSystemConfig config;
    bool initialized;
    uint32_t last_error;
    CriticalSection critical;
} DiagSystem;

static DiagSystem diag_system;

// Integration callback functions
static void transport_received_callback(const uint8_t* data, uint16_t length) {
    if (diag_system.initialized) {
        Diag_System_HandleRequest(data, length);
    }
}

static void session_change_callback(UdsSessionType old_session, UdsSessionType new_session) {
    // Update security and communication states based on session change
    if (new_session == UDS_SESSION_DEFAULT) {
        Security_Manager_LockLevel(0xFF); // Lock all security levels
        Comm_Manager_ControlCommunication(0, COMM_CONTROL_ENABLE_RX_TX);
    }
}

static void security_change_callback(uint8_t level, bool access_granted) {
    // Update session state based on security change
    if (!access_granted) {
        Session_Manager_ChangeSession(UDS_SESSION_DEFAULT);
    }
}

bool Diag_System_Init(const DiagSystemConfig* config) {
    if (!config) {
        return false;
    }

    enter_critical(&diag_system.critical);

    // Store configuration
    memcpy(&diag_system.config, config, sizeof(DiagSystemConfig));

    // Initialize all components
    bool success = true;

    // Add transport received callback
    DiagTransportConfig transport_config = config->transport_config;
    transport_config.receive_callback = transport_received_callback;
    success &= Diag_Transport_Init(&transport_config);

    // Add session change callback
    SessionManagerConfig session_config = config->session_config;
    session_config.session_change_callback = session_change_callback;
    success &= Session_Manager_Init(&session_config);

    // Add security change callback
    SecurityManagerConfig security_config = config->security_config;
    security_config.security_callback = security_change_callback;
    success &= Security_Manager_Init(&security_config);

    // Initialize other components
    success &= Memory_Manager_Init(&config->memory_config);
    success &= Service_Router_Init(&config->router_config);
    success &= Data_Manager_Init(&config->data_config);
    success &= Routine_Manager_Init(&config->routine_config);
    success &= Comm_Manager_Init(&config->comm_config);

    if (success) {
        diag_system.initialized = true;
        diag_system.last_error = 0;
    }

    exit_critical(&diag_system.critical);
    return success;
}

void Diag_System_DeInit(void) {
    enter_critical(&diag_system.critical);

    // Deinitialize all components in reverse order
    Comm_Manager_DeInit();
    Routine_Manager_DeInit();
    Data_Manager_DeInit();
    Service_Router_DeInit();
    Memory_Manager_DeInit();
    Security_Manager_DeInit();
    Session_Manager_DeInit();
    Diag_Transport_DeInit();

    memset(&diag_system, 0, sizeof(DiagSystem));

    exit_critical(&diag_system.critical);
}

void Diag_System_Process(void) {
    if (!diag_system.initialized) {
        return;
    }

    // Process timeouts and periodic tasks for all components
    Diag_Transport_Process();
    Session_Manager_ProcessTimeout();
    Security_Manager_ProcessTimeout();
    Routine_Manager_ProcessTimeout();
    Comm_Manager_ProcessTimeout();
}

bool Diag_System_HandleRequest(const uint8_t* data, uint16_t length) {
    if (!diag_system.initialized || !data || length == 0) {
        return false;
    }

    enter_critical(&diag_system.critical);

    // Parse UDS message
    UdsMessage request = {0};
    UdsMessage response = {0};
    
    if (!parse_uds_message(data, length, &request)) {
        exit_critical(&diag_system.critical);
        return false;
    }

    // Update session activity
    Session_Manager_UpdateActivity();

    // Process request through service router
    UdsResponseCode result = Service_Router_ProcessRequest(&request, &response);

    // Send response if not suppressed
    if (!Session_Manager_IsSuppressResponse()) {
        uint8_t response_buffer[4096];
        uint16_t response_length = 0;

        if (build_uds_response(&response, result, response_buffer, &response_length)) {
            Diag_Transport_Transmit(response_buffer, response_length);
        }
    }

    exit_critical(&diag_system.critical);
    return true;
}

bool Diag_System_IsReady(void) {
    return diag_system.initialized;
}

uint32_t Diag_System_GetLastError(void) {
    return diag_system.last_error;
} 