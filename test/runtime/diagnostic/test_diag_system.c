 #include "unity.h"
#include "diagnostic/diag_system.h"
#include "diagnostic/error/error_handler.h"
#include <string.h>

static DiagSystemConfig test_config;
static void error_callback(const ErrorInfo* error);
static uint32_t callback_count = 0;

void setUp(void) {
    // Initialize test configuration
    memset(&test_config, 0, sizeof(DiagSystemConfig));
    
    // Logger config
    test_config.logger.enable_console = true;
    test_config.logger.enable_file = false;
    test_config.logger.min_level = LOG_LEVEL_DEBUG;
    
    // Session config
    test_config.session.max_sessions = 10;
    test_config.session.session_timeout_ms = 5000;
    
    // Security config
    test_config.security.delay_time_ms = 1000;
    test_config.security.max_attempts = 3;
    
    // Resource config
    test_config.resource.enable_monitoring = true;
    test_config.resource.check_interval_ms = 100;
    
    // Initialize diagnostic system
    TEST_ASSERT_TRUE(DiagSystem_Init(&test_config));
}

void tearDown(void) {
    DiagSystem_Deinit();
}

void test_DiagSystem_Initialization(void) {
    DiagSystemStatus status;
    DiagSystem_GetStatus(&status);
    
    TEST_ASSERT_EQUAL_UINT32(0, status.active_sessions);
    TEST_ASSERT_EQUAL_UINT32(0, status.security_violations);
    TEST_ASSERT_EQUAL_UINT32(0, status.error_count);
    TEST_ASSERT_TRUE(DiagSystem_IsHealthy());
}

void test_ErrorHandler(void) {
    ErrorHandlerConfig error_config = {
        .enable_callbacks = true,
        .log_errors = true,
        .auto_reset = false,
        .max_stored_errors = 50,
        .global_callback = error_callback
    };

    TEST_ASSERT_TRUE(Error_Init(&error_config));
    
    // Test error reporting
    Error_Report(0x1234, ERROR_SEVERITY_WARNING, "TEST", "Test warning");
    TEST_ASSERT_EQUAL_UINT32(1, Error_GetCount(ERROR_SEVERITY_WARNING));
    
    Error_Report(0x5678, ERROR_SEVERITY_ERROR, "TEST", "Test error");
    TEST_ASSERT_EQUAL_UINT32(1, Error_GetCount(ERROR_SEVERITY_ERROR));
    
    // Test callback
    TEST_ASSERT_EQUAL_UINT32(2, callback_count);
    
    // Test last error
    ErrorInfo last_error;
    TEST_ASSERT_TRUE(Error_GetLastError(&last_error));
    TEST_ASSERT_EQUAL_UINT32(0x5678, last_error.error_code);
    TEST_ASSERT_EQUAL(ERROR_SEVERITY_ERROR, last_error.severity);
    
    Error_ClearAll();
    TEST_ASSERT_EQUAL_UINT32(0, Error_GetCount(ERROR_SEVERITY_WARNING));
    TEST_ASSERT_EQUAL_UINT32(0, Error_GetCount(ERROR_SEVERITY_ERROR));
}

static void error_callback(const ErrorInfo* error) {
    callback_count++;
}
