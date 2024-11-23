#include "unity.h"
#include "../../src/runtime/diagnostic/diag_security.h"
#include "../../src/runtime/diagnostic/diag_timer.h"
#include "../../src/runtime/diagnostic/diag_error.h"
#include <string.h>

// Test context
typedef struct {
    bool callback_called;
    DiagSecurityLevel last_level;
    DiagSecurityResult last_result;
    uint32_t callback_count;
    void* last_context;
} TestContext;

static TestContext test_ctx;

// Mock time functions (similar to timer tests)
static uint32_t mock_time = 0;

uint32_t mock_get_timestamp(void) {
    return mock_time;
}

void mock_advance_time(uint32_t ms) {
    mock_time += ms;
}

// Test callback
static void test_security_callback(DiagSecurityLevel level, 
                                 DiagSecurityResult result, 
                                 void* context) 
{
    test_ctx.callback_called = true;
    test_ctx.last_level = level;
    test_ctx.last_result = result;
    test_ctx.last_context = context;
    test_ctx.callback_count++;
}

void setUp(void) {
    memset(&test_ctx, 0, sizeof(TestContext));
    mock_time = 0;
    
    DiagTimer_SetTimestampFunction(mock_get_timestamp);
    DiagTimer_Init();
    DiagSecurity_Init();
}

void tearDown(void) {
    DiagSecurity_Deinit();
    DiagTimer_Deinit();
}

// Basic security tests
void test_DiagSecurity_BasicAccess(void) {
    // Register callback
    TEST_ASSERT_TRUE(DiagSecurity_RegisterCallback(test_security_callback, NULL));
    
    // Request level 1 access
    uint32_t seed = DiagSecurity_RequestAccess(DIAG_SEC_LEVEL_1);
    TEST_ASSERT_NOT_EQUAL(0, seed);
    
    // Calculate key (simplified for test)
    uint32_t key = ~seed;  // In real implementation, use proper algorithm
    
    // Verify key
    DiagSecurityResult result = DiagSecurity_VerifyKey(DIAG_SEC_LEVEL_1, key);
    TEST_ASSERT_EQUAL(SECURITY_RESULT_OK, result);
    TEST_ASSERT_TRUE(test_ctx.callback_called);
    TEST_ASSERT_EQUAL(DIAG_SEC_LEVEL_1, test_ctx.last_level);
    TEST_ASSERT_EQUAL(SECURITY_RESULT_OK, test_ctx.last_result);
}

void test_DiagSecurity_InvalidKey(void) {
    uint32_t seed = DiagSecurity_RequestAccess(DIAG_SEC_LEVEL_1);
    TEST_ASSERT_NOT_EQUAL(0, seed);
    
    // Try invalid key
    DiagSecurityResult result = DiagSecurity_VerifyKey(DIAG_SEC_LEVEL_1, 0x12345678);
    TEST_ASSERT_EQUAL(SECURITY_RESULT_INVALID_KEY, result);
    
    // Verify security level hasn't changed
    TEST_ASSERT_EQUAL(DIAG_SEC_LOCKED, DiagSecurity_GetCurrentLevel());
}

void test_DiagSecurity_LevelProgression(void) {
    // Try accessing higher level without lower level
    uint32_t seed = DiagSecurity_RequestAccess(DIAG_SEC_LEVEL_2);
    TEST_ASSERT_EQUAL(0, seed);
    
    // Get level 1 access first
    seed = DiagSecurity_RequestAccess(DIAG_SEC_LEVEL_1);
    TEST_ASSERT_NOT_EQUAL(0, seed);
    
    uint32_t key = ~seed;  // Simplified key calculation
    DiagSecurityResult result = DiagSecurity_VerifyKey(DIAG_SEC_LEVEL_1, key);
    TEST_ASSERT_EQUAL(SECURITY_RESULT_OK, result);
    
    // Now try level 2
    seed = DiagSecurity_RequestAccess(DIAG_SEC_LEVEL_2);
    TEST_ASSERT_NOT_EQUAL(0, seed);
}

// Timeout and attempt limit tests
void test_DiagSecurity_AccessTimeout(void) {
    uint32_t seed = DiagSecurity_RequestAccess(DIAG_SEC_LEVEL_1);
    TEST_ASSERT_NOT_EQUAL(0, seed);
    
    // Wait longer than timeout
    mock_advance_time(SECURITY_ACCESS_TIMEOUT + 100);
    DiagTimer_Process();
    
    // Try to verify key after timeout
    uint32_t key = ~seed;
    DiagSecurityResult result = DiagSecurity_VerifyKey(DIAG_SEC_LEVEL_1, key);
    TEST_ASSERT_EQUAL(SECURITY_RESULT_TIMEOUT, result);
}

void test_DiagSecurity_AttemptLimit(void) {
    uint32_t seed = DiagSecurity_RequestAccess(DIAG_SEC_LEVEL_1);
    TEST_ASSERT_NOT_EQUAL(0, seed);
    
    // Try invalid keys up to limit
    for (int i = 0; i < SECURITY_MAX_ATTEMPTS; i++) {
        DiagSecurityResult result = DiagSecurity_VerifyKey(DIAG_SEC_LEVEL_1, 0x12345678);
        TEST_ASSERT_EQUAL(SECURITY_RESULT_INVALID_KEY, result);
    }
    
    // Next attempt should be locked
    DiagSecurityResult result = DiagSecurity_VerifyKey(DIAG_SEC_LEVEL_1, ~seed);
    TEST_ASSERT_EQUAL(SECURITY_RESULT_LOCKED, result);
    
    // Wait for lockout timer
    mock_advance_time(SECURITY_LOCKOUT_TIME + 100);
    DiagTimer_Process();
    
    // Should be able to try again
    seed = DiagSecurity_RequestAccess(DIAG_SEC_LEVEL_1);
    TEST_ASSERT_NOT_EQUAL(0, seed);
}

// Session management tests
void test_DiagSecurity_SessionChange(void) {
    // Get security access
    uint32_t seed = DiagSecurity_RequestAccess(DIAG_SEC_LEVEL_1);
    uint32_t key = ~seed;
    DiagSecurityResult result = DiagSecurity_VerifyKey(DIAG_SEC_LEVEL_1, key);
    TEST_ASSERT_EQUAL(SECURITY_RESULT_OK, result);
    
    // Simulate session change
    DiagSecurity_HandleSessionChange(DIAG_SESSION_DEFAULT);
    
    // Verify security level reset
    TEST_ASSERT_EQUAL(DIAG_SEC_LOCKED, DiagSecurity_GetCurrentLevel());
}

// Stress tests
void test_DiagSecurity_RapidAccessRequests(void) {
    const int NUM_REQUESTS = 1000;
    uint32_t seeds[NUM_REQUESTS];
    uint32_t start_time = mock_get_timestamp();
    
    // Make rapid access requests
    for (int i = 0; i < NUM_REQUESTS; i++) {
        seeds[i] = DiagSecurity_RequestAccess(DIAG_SEC_LEVEL_1);
        TEST_ASSERT_NOT_EQUAL(0, seeds[i]);
        
        // Verify keys immediately
        uint32_t key = ~seeds[i];
        DiagSecurityResult result = DiagSecurity_VerifyKey(DIAG_SEC_LEVEL_1, key);
        
        if (i % 100 == 0) {  // Advance time periodically
            mock_advance_time(1);
            DiagTimer_Process();
        }
        
        TEST_ASSERT_EQUAL(SECURITY_RESULT_OK, result);
    }
    
    uint32_t elapsed = mock_get_timestamp() - start_time;
    TEST_ASSERT_TRUE(elapsed < 1000);  // Should complete within 1 second
}

// Custom security level tests
void test_DiagSecurity_CustomLevel(void) {
    // Define custom security level
    DiagSecurityLevel custom_level = DIAG_SEC_CUSTOM_START;
    
    // Custom key calculation function
    uint32_t custom_calculate_key(uint32_t seed) {
        return seed ^ 0xDEADBEEF;
    }
    
    // Register custom security level
    DiagSecurityConfig custom_config = {
        .level = custom_level,
        .prerequisite_level = DIAG_SEC_LOCKED,
        .calculate_key = custom_calculate_key,
        .timeout_ms = 5000
    };
    
    TEST_ASSERT_TRUE(DiagSecurity_RegisterCustomLevel(&custom_config));
    
    // Test custom level access
    uint32_t seed = DiagSecurity_RequestAccess(custom_level);
    TEST_ASSERT_NOT_EQUAL(0, seed);
    
    uint32_t key = custom_calculate_key(seed);
    DiagSecurityResult result = DiagSecurity_VerifyKey(custom_level, key);
    TEST_ASSERT_EQUAL(SECURITY_RESULT_OK, result);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_DiagSecurity_BasicAccess);
    RUN_TEST(test_DiagSecurity_InvalidKey);
    RUN_TEST(test_DiagSecurity_LevelProgression);
    RUN_TEST(test_DiagSecurity_AccessTimeout);
    RUN_TEST(test_DiagSecurity_AttemptLimit);
    RUN_TEST(test_DiagSecurity_SessionChange);
    RUN_TEST(test_DiagSecurity_RapidAccessRequests);
    RUN_TEST(test_DiagSecurity_CustomLevel);
    
    return UNITY_END();
} 