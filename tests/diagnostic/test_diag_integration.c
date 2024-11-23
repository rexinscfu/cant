#include "unity.h"
#include "../../src/runtime/diagnostic/diag_core.h"
#include "../../src/runtime/diagnostic/diag_session.h"
#include "../../src/runtime/diagnostic/diag_security.h"
#include "../../src/runtime/diagnostic/diag_parser.h"
#include "../../src/runtime/diagnostic/diag_state.h"
#include "../../src/runtime/diagnostic/diag_timer.h"
#include "../../src/runtime/diagnostic/diag_error.h"
#include <string.h>

// Test context for integration tests
typedef struct {
    bool message_received;
    bool response_sent;
    bool session_changed;
    bool security_changed;
    bool error_occurred;
    DiagMessage last_message;
    DiagResponse last_response;
    DiagSession last_session;
    DiagSecurityLevel last_security_level;
    DiagError last_error;
} TestContext;

static TestContext test_ctx;
static uint8_t test_buffer[4096];

// Mock time control
static uint32_t mock_time = 0;
uint32_t mock_get_timestamp(void) {
    return mock_time;
}
void mock_advance_time(uint32_t ms) {
    mock_time += ms;
}

// Callback functions for different subsystems
static void message_callback(const DiagMessage* message, void* context) {
    test_ctx.message_received = true;
    memcpy(&test_ctx.last_message, message, sizeof(DiagMessage));
}

static void response_callback(const DiagResponse* response, void* context) {
    test_ctx.response_sent = true;
    memcpy(&test_ctx.last_response, response, sizeof(DiagResponse));
}

static void session_callback(DiagSession old_session, 
                           DiagSession new_session,
                           DiagSessionResult result,
                           void* context) 
{
    test_ctx.session_changed = true;
    test_ctx.last_session = new_session;
}

static void security_callback(DiagSecurityLevel level,
                            DiagSecurityResult result,
                            void* context) 
{
    test_ctx.security_changed = true;
    test_ctx.last_security_level = level;
}

static void error_callback(const DiagError* error, void* context) {
    test_ctx.error_occurred = true;
    memcpy(&test_ctx.last_error, error, sizeof(DiagError));
}

void setUp(void) {
    // Initialize test context
    memset(&test_ctx, 0, sizeof(TestContext));
    memset(test_buffer, 0, sizeof(test_buffer));
    mock_time = 0;
    
    // Initialize all subsystems
    DiagTimer_SetTimestampFunction(mock_get_timestamp);
    DiagTimer_Init();
    DiagError_Init();
    DiagState_Init();
    DiagSession_Init();
    DiagSecurity_Init();
    
    // Register callbacks
    DiagCore_RegisterMessageCallback(message_callback, NULL);
    DiagCore_RegisterResponseCallback(response_callback, NULL);
    DiagSession_RegisterCallback(session_callback, NULL);
    DiagSecurity_RegisterCallback(security_callback, NULL);
    DiagError_RegisterHandler(error_callback, NULL);
}

void tearDown(void) {
    DiagSecurity_Deinit();
    DiagSession_Deinit();
    DiagState_Deinit();
    DiagError_Deinit();
    DiagTimer_Deinit();
}

// Complete diagnostic flow test
void test_DiagIntegration_CompleteFlow(void) {
    // 1. Start diagnostic session
    uint8_t start_session_data[] = {
        FORMAT_VERSION,
        0x02,  // Length
        DIAG_SID_START_DIAGNOSTIC_SESSION,
        DIAG_SESSION_PROGRAMMING,
        0x00   // Checksum (to be calculated)
    };
    start_session_data[4] = calculate_checksum(start_session_data, 4);
    
    TEST_ASSERT_TRUE(DiagCore_HandleMessage(start_session_data, 5));
    TEST_ASSERT_TRUE(test_ctx.message_received);
    TEST_ASSERT_TRUE(test_ctx.session_changed);
    TEST_ASSERT_EQUAL(DIAG_SESSION_PROGRAMMING, test_ctx.last_session);
    
    // 2. Request security access
    uint8_t security_request[] = {
        FORMAT_VERSION,
        0x02,
        DIAG_SID_SECURITY_ACCESS,
        0x01,  // Request seed
        0x00   // Checksum
    };
    security_request[4] = calculate_checksum(security_request, 4);
    
    test_ctx.message_received = false;
    TEST_ASSERT_TRUE(DiagCore_HandleMessage(security_request, 5));
    TEST_ASSERT_TRUE(test_ctx.message_received);
    
    // 3. Send security key
    uint32_t seed = test_ctx.last_response.data[0] | 
                   (test_ctx.last_response.data[1] << 8) |
                   (test_ctx.last_response.data[2] << 16) |
                   (test_ctx.last_response.data[3] << 24);
    uint32_t key = ~seed;  // Simplified key calculation
    
    uint8_t security_key[] = {
        FORMAT_VERSION,
        0x06,
        DIAG_SID_SECURITY_ACCESS,
        0x02,  // Send key
        key & 0xFF,
        (key >> 8) & 0xFF,
        (key >> 16) & 0xFF,
        (key >> 24) & 0xFF,
        0x00   // Checksum
    };
    security_key[8] = calculate_checksum(security_key, 8);
    
    test_ctx.message_received = false;
    TEST_ASSERT_TRUE(DiagCore_HandleMessage(security_key, 9));
    TEST_ASSERT_TRUE(test_ctx.security_changed);
    TEST_ASSERT_EQUAL(DIAG_SEC_LEVEL_1, test_ctx.last_security_level);
    
    // 4. Read data by identifier
    uint8_t read_data[] = {
        FORMAT_VERSION,
        0x03,
        DIAG_SID_READ_DATA_BY_ID,
        0x12,  // Example identifier
        0x34,
        0x00   // Checksum
    };
    read_data[5] = calculate_checksum(read_data, 5);
    
    test_ctx.message_received = false;
    TEST_ASSERT_TRUE(DiagCore_HandleMessage(read_data, 6));
    TEST_ASSERT_TRUE(test_ctx.message_received);
    
    // 5. End session
    uint8_t end_session[] = {
        FORMAT_VERSION,
        0x02,
        DIAG_SID_START_DIAGNOSTIC_SESSION,
        DIAG_SESSION_DEFAULT,
        0x00   // Checksum
    };
    end_session[4] = calculate_checksum(end_session, 4);
    
    test_ctx.message_received = false;
    TEST_ASSERT_TRUE(DiagCore_HandleMessage(end_session, 5));
    TEST_ASSERT_TRUE(test_ctx.session_changed);
    TEST_ASSERT_EQUAL(DIAG_SESSION_DEFAULT, test_ctx.last_session);
}

// Test error handling across components
void test_DiagIntegration_ErrorHandling(void) {
    // 1. Invalid message format
    uint8_t invalid_message[] = {
        0xFF,  // Invalid version
        0x02,
        DIAG_SID_READ_DATA_BY_ID,
        0x00,
        0x00
    };
    
    TEST_ASSERT_FALSE(DiagCore_HandleMessage(invalid_message, 5));
    TEST_ASSERT_TRUE(test_ctx.error_occurred);
    TEST_ASSERT_EQUAL(ERROR_PROTOCOL_INVALID_FORMAT, test_ctx.last_error.code);
    
    // 2. Security access without proper session
    uint8_t security_request[] = {
        FORMAT_VERSION,
        0x02,
        DIAG_SID_SECURITY_ACCESS,
        0x01,
        0x00
    };
    security_request[4] = calculate_checksum(security_request, 4);
    
    test_ctx.error_occurred = false;
    TEST_ASSERT_FALSE(DiagCore_HandleMessage(security_request, 5));
    TEST_ASSERT_TRUE(test_ctx.error_occurred);
    TEST_ASSERT_EQUAL(ERROR_SESSION_INVALID_STATE, test_ctx.last_error.code);
}

// Test timeout handling across components
void test_DiagIntegration_TimeoutHandling(void) {
    // 1. Start programming session
    uint8_t start_session[] = {
        FORMAT_VERSION,
        0x02,
        DIAG_SID_START_DIAGNOSTIC_SESSION,
        DIAG_SESSION_PROGRAMMING,
        0x00
    };
    start_session[4] = calculate_checksum(start_session, 4);
    
    TEST_ASSERT_TRUE(DiagCore_HandleMessage(start_session, 5));
    TEST_ASSERT_EQUAL(DIAG_SESSION_PROGRAMMING, test_ctx.last_session);
    
    // 2. Wait for P3 timeout
    mock_advance_time(SESSION_P3_TIMEOUT + 100);
    DiagTimer_Process();
    
    // Verify session reverted to default
    TEST_ASSERT_EQUAL(DIAG_SESSION_DEFAULT, DiagSession_GetCurrent());
    TEST_ASSERT_TRUE(test_ctx.error_occurred);
    TEST_ASSERT_EQUAL(ERROR_TIMING_TIMEOUT, test_ctx.last_error.code);
}

// Test concurrent operations
void test_DiagIntegration_ConcurrentOperations(void) {
    // 1. Start multiple timers
    uint32_t timer1 = DiagTimer_Start(TIMER_TYPE_REQUEST, 100, NULL, NULL);
    uint32_t timer2 = DiagTimer_Start(TIMER_TYPE_SESSION, 200, NULL, NULL);
    
    // 2. Start session change
    uint8_t start_session[] = {
        FORMAT_VERSION,
        0x02,
        DIAG_SID_START_DIAGNOSTIC_SESSION,
        DIAG_SESSION_PROGRAMMING,
        0x00
    };
    start_session[4] = calculate_checksum(start_session, 4);
    
    TEST_ASSERT_TRUE(DiagCore_HandleMessage(start_session, 5));
    
    // 3. Request security access immediately
    uint8_t security_request[] = {
        FORMAT_VERSION,
        0x02,
        DIAG_SID_SECURITY_ACCESS,
        0x01,
        0x00
    };
    security_request[4] = calculate_checksum(security_request, 4);
    
    TEST_ASSERT_TRUE(DiagCore_HandleMessage(security_request, 5));
    
    // 4. Process timers and verify state
    mock_advance_time(150);
    DiagTimer_Process();
    
    TEST_ASSERT_EQUAL(DIAG_SESSION_PROGRAMMING, DiagSession_GetCurrent());
    TEST_ASSERT_TRUE(test_ctx.security_changed);
}

// Test state transitions
void test_DiagIntegration_StateTransitions(void) {
    // Track state changes through complete diagnostic flow
    DiagState initial_state = DiagState_GetCurrent();
    
    // 1. Start session
    uint8_t start_session[] = {
        FORMAT_VERSION,
        0x02,
        DIAG_SID_START_DIAGNOSTIC_SESSION,
        DIAG_SESSION_PROGRAMMING,
        0x00
    };
    start_session[4] = calculate_checksum(start_session, 4);
    
    TEST_ASSERT_TRUE(DiagCore_HandleMessage(start_session, 5));
    TEST_ASSERT_NOT_EQUAL(initial_state, DiagState_GetCurrent());
    
    // 2. Security access
    uint8_t security_request[] = {
        FORMAT_VERSION,
        0x02,
        DIAG_SID_SECURITY_ACCESS,
        0x01,
        0x00
    };
    security_request[4] = calculate_checksum(security_request, 4);
    
    DiagState pre_security_state = DiagState_GetCurrent();
    TEST_ASSERT_TRUE(DiagCore_HandleMessage(security_request, 5));
    TEST_ASSERT_NOT_EQUAL(pre_security_state, DiagState_GetCurrent());
}

// Test resource cleanup
void test_DiagIntegration_ResourceCleanup(void) {
    // 1. Allocate various resources
    uint32_t timers[5];
    for (int i = 0; i < 5; i++) {
        timers[i] = DiagTimer_Start(TIMER_TYPE_REQUEST, 100 * (i + 1), NULL, NULL);
        TEST_ASSERT_NOT_EQUAL(0, timers[i]);
    }
    
    // 2. Start session and security
    uint8_t start_session[] = {
        FORMAT_VERSION,
        0x02,
        DIAG_SID_START_DIAGNOSTIC_SESSION,
        DIAG_SESSION_PROGRAMMING,
        0x00
    };
    start_session[4] = calculate_checksum(start_session, 4);
    TEST_ASSERT_TRUE(DiagCore_HandleMessage(start_session, 5));
    
    // 3. Force cleanup
    DiagCore_Reset();
    
    // 4. Verify cleanup
    TEST_ASSERT_EQUAL(DIAG_SESSION_DEFAULT, DiagSession_GetCurrent());
    TEST_ASSERT_EQUAL(DIAG_SEC_LOCKED, DiagSecurity_GetCurrentLevel());
    
    // Verify timers are cleaned up
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_FALSE(DiagTimer_IsActive(timers[i]));
    }
}

// Performance stress test
void test_DiagIntegration_PerformanceStress(void) {
    const int NUM_MESSAGES = 1000;
    uint32_t start_time = mock_get_timestamp();
    
    uint8_t test_message[] = {
        FORMAT_VERSION,
        0x03,
        DIAG_SID_READ_DATA_BY_ID,
        0x12,
        0x34,
        0x00
    };
    test_message[5] = calculate_checksum(test_message, 5);
    
    for (int i = 0; i < NUM_MESSAGES; i++) {
        TEST_ASSERT_TRUE(DiagCore_HandleMessage(test_message, 6));
        
        if (i % 100 == 0) {
            mock_advance_time(1);
            DiagTimer_Process();
        }
    }
    
    uint32_t elapsed = mock_get_timestamp() - start_time;
    TEST_ASSERT_TRUE(elapsed < 1000);  // Should process 1000 messages within 1 second
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_DiagIntegration_CompleteFlow);
    RUN_TEST(test_DiagIntegration_ErrorHandling);
    RUN_TEST(test_DiagIntegration_TimeoutHandling);
    RUN_TEST(test_DiagIntegration_ConcurrentOperations);
    RUN_TEST(test_DiagIntegration_StateTransitions);
    RUN_TEST(test_DiagIntegration_ResourceCleanup);
    RUN_TEST(test_DiagIntegration_PerformanceStress);
    
    return UNITY_END();
}

