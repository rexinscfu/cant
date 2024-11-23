#include "unity.h"
#include "../../src/runtime/diagnostic/diag_session.h"
#include "../../src/runtime/diagnostic/diag_timer.h"
#include "../../src/runtime/diagnostic/diag_error.h"
#include <string.h>

// Test context
typedef struct {
    bool callback_called;
    DiagSession last_old_session;
    DiagSession last_new_session;
    DiagSessionResult last_result;
    uint32_t callback_count;
    void* last_context;
} TestContext;

static TestContext test_ctx;

// Mock time functions
static uint32_t mock_time = 0;

uint32_t mock_get_timestamp(void) {
    return mock_time;
}

void mock_advance_time(uint32_t ms) {
    mock_time += ms;
}

// Test callback
static void test_session_callback(DiagSession old_session, 
                                DiagSession new_session,
                                DiagSessionResult result,
                                void* context) 
{
    test_ctx.callback_called = true;
    test_ctx.last_old_session = old_session;
    test_ctx.last_new_session = new_session;
    test_ctx.last_result = result;
    test_ctx.last_context = context;
    test_ctx.callback_count++;
}

void setUp(void) {
    memset(&test_ctx, 0, sizeof(TestContext));
    mock_time = 0;
    
    DiagTimer_SetTimestampFunction(mock_get_timestamp);
    DiagTimer_Init();
    DiagSession_Init();
}

void tearDown(void) {
    DiagSession_Deinit();
    DiagTimer_Deinit();
}

// Basic session tests
void test_DiagSession_BasicTransition(void) {
    // Register callback
    TEST_ASSERT_TRUE(DiagSession_RegisterCallback(test_session_callback, NULL));
    
    // Start programming session
    DiagSessionResult result = DiagSession_Start(DIAG_SESSION_PROGRAMMING);
    TEST_ASSERT_EQUAL(SESSION_RESULT_OK, result);
    TEST_ASSERT_TRUE(test_ctx.callback_called);
    TEST_ASSERT_EQUAL(DIAG_SESSION_DEFAULT, test_ctx.last_old_session);
    TEST_ASSERT_EQUAL(DIAG_SESSION_PROGRAMMING, test_ctx.last_new_session);
    
    // Verify current session
    TEST_ASSERT_EQUAL(DIAG_SESSION_PROGRAMMING, DiagSession_GetCurrent());
}

void test_DiagSession_InvalidTransition(void) {
    // Try to start extended session from default
    DiagSessionResult result = DiagSession_Start(DIAG_SESSION_EXTENDED);
    TEST_ASSERT_EQUAL(SESSION_RESULT_INVALID_TRANSITION, result);
    
    // Verify still in default session
    TEST_ASSERT_EQUAL(DIAG_SESSION_DEFAULT, DiagSession_GetCurrent());
}

void test_DiagSession_SessionProgression(void) {
    // Default -> Programming -> Extended -> Default
    DiagSessionResult result;
    
    result = DiagSession_Start(DIAG_SESSION_PROGRAMMING);
    TEST_ASSERT_EQUAL(SESSION_RESULT_OK, result);
    
    result = DiagSession_Start(DIAG_SESSION_EXTENDED);
    TEST_ASSERT_EQUAL(SESSION_RESULT_OK, result);
    
    result = DiagSession_Start(DIAG_SESSION_DEFAULT);
    TEST_ASSERT_EQUAL(SESSION_RESULT_OK, result);
}

// Session timeout tests
void test_DiagSession_Timeout(void) {
    // Register callback
    DiagSession_RegisterCallback(test_session_callback, NULL);
    
    // Start programming session
    DiagSessionResult result = DiagSession_Start(DIAG_SESSION_PROGRAMMING);
    TEST_ASSERT_EQUAL(SESSION_RESULT_OK, result);
    
    // Reset callback flag
    test_ctx.callback_called = false;
    
    // Wait longer than P3 timeout
    mock_advance_time(SESSION_P3_TIMEOUT + 100);
    DiagTimer_Process();
    
    // Verify session timeout occurred
    TEST_ASSERT_TRUE(test_ctx.callback_called);
    TEST_ASSERT_EQUAL(DIAG_SESSION_PROGRAMMING, test_ctx.last_old_session);
    TEST_ASSERT_EQUAL(DIAG_SESSION_DEFAULT, test_ctx.last_new_session);
    TEST_ASSERT_EQUAL(SESSION_RESULT_TIMEOUT, test_ctx.last_result);
}

void test_DiagSession_TesterPresent(void) {
    // Start programming session
    DiagSessionResult result = DiagSession_Start(DIAG_SESSION_PROGRAMMING);
    TEST_ASSERT_EQUAL(SESSION_RESULT_OK, result);
    
    // Advance time but send tester present
    for (int i = 0; i < 5; i++) {
        mock_advance_time(SESSION_P3_TIMEOUT / 2);
        DiagTimer_Process();
        DiagSession_HandleTesterPresent();
    }
    
    // Verify still in programming session
    TEST_ASSERT_EQUAL(DIAG_SESSION_PROGRAMMING, DiagSession_GetCurrent());
}

// Custom session tests
void test_DiagSession_CustomSession(void) {
    // Define custom session handlers
    bool custom_session_start(void* data) {
        return true;
    }
    
    bool custom_session_end(void* data) {
        return true;
    }
    
    // Register custom session
    DiagSessionConfig custom_config = {
        .session = DIAG_SESSION_CUSTOM_START,
        .start_handler = custom_session_start,
        .end_handler = custom_session_end,
        .p2_timeout_ms = 1000,
        .p3_timeout_ms = 5000
    };
    
    TEST_ASSERT_TRUE(DiagSession_RegisterCustomSession(&custom_config));
    
    // Test custom session transition
    DiagSessionResult result = DiagSession_Start(DIAG_SESSION_CUSTOM_START);
    TEST_ASSERT_EQUAL(SESSION_RESULT_OK, result);
}

// Stress tests
void test_DiagSession_RapidTransitions(void) {
    const int NUM_TRANSITIONS = 1000;
    DiagSession sessions[] = {
        DIAG_SESSION_DEFAULT,
        DIAG_SESSION_PROGRAMMING,
        DIAG_SESSION_EXTENDED
    };
    
    uint32_t start_time = mock_get_timestamp();
    
    for (int i = 0; i < NUM_TRANSITIONS; i++) {
        DiagSession next_session = sessions[i % 3];
        DiagSessionResult result = DiagSession_Start(next_session);
        
        if (i % 100 == 0) {  // Advance time periodically
            mock_advance_time(1);
            DiagTimer_Process();
        }
        
        TEST_ASSERT_EQUAL(SESSION_RESULT_OK, result);
    }
    
    uint32_t elapsed = mock_get_timestamp() - start_time;
    TEST_ASSERT_TRUE(elapsed < 1000);  // Should complete within 1 second
}

// Security interaction tests
void test_DiagSession_SecurityInteraction(void) {
    // Start session that requires security
    DiagSessionResult result = DiagSession_Start(DIAG_SESSION_PROGRAMMING);
    TEST_ASSERT_EQUAL(SESSION_RESULT_OK, result);
    
    // Verify security level requirement
    TEST_ASSERT_TRUE(DiagSession_RequiresSecurity(DIAG_SESSION_PROGRAMMING));
    TEST_ASSERT_FALSE(DiagSession_RequiresSecurity(DIAG_SESSION_DEFAULT));
    
    // Verify security access allowed in current session
    TEST_ASSERT_TRUE(DiagSession_IsSecurityAllowed());
}

// Resource management tests
void test_DiagSession_ResourceManagement(void) {
    // Register maximum number of callbacks
    void* contexts[MAX_SESSION_CALLBACKS];
    for (int i = 0; i < MAX_SESSION_CALLBACKS; i++) {
        contexts[i] = (void*)(uintptr_t)i;
        TEST_ASSERT_TRUE(DiagSession_RegisterCallback(test_session_callback, 
                                                    contexts[i]));
    }
    
    // Try to register one more callback
    TEST_ASSERT_FALSE(DiagSession_RegisterCallback(test_session_callback, NULL));
    
    // Verify all callbacks are called
    DiagSessionResult result = DiagSession_Start(DIAG_SESSION_PROGRAMMING);
    TEST_ASSERT_EQUAL(SESSION_RESULT_OK, result);
    TEST_ASSERT_EQUAL(MAX_SESSION_CALLBACKS, test_ctx.callback_count);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_DiagSession_BasicTransition);
    RUN_TEST(test_DiagSession_InvalidTransition);
    RUN_TEST(test_DiagSession_SessionProgression);
    RUN_TEST(test_DiagSession_Timeout);
    RUN_TEST(test_DiagSession_TesterPresent);
    RUN_TEST(test_DiagSession_CustomSession);
    RUN_TEST(test_DiagSession_RapidTransitions);
    RUN_TEST(test_DiagSession_SecurityInteraction);
    RUN_TEST(test_DiagSession_ResourceManagement);
    
    return UNITY_END();
} 