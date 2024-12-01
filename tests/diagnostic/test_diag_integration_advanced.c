#include "unity.h"
#include "../../src/runtime/diagnostic/diag_core.h"
#include "../../src/runtime/diagnostic/diag_logger.h"
#include "../../src/runtime/diagnostic/diag_monitor.h"
#include "../../src/runtime/diagnostic/diag_recorder.h"
#include <string.h>

// Test context
typedef struct {
    uint32_t log_count;
    uint32_t monitor_count;
    uint32_t record_count;
    char last_message[256];
} TestContext;

static TestContext test_ctx;

// Mock callbacks
static void test_log_callback(const DiagLogEntry* entry, void* context) {
    TestContext* ctx = (TestContext*)context;
    ctx->log_count++;
    strncpy(ctx->last_message, entry->message, sizeof(ctx->last_message) - 1);
}

static void test_monitor_write(const char* text, void* context) {
    TestContext* ctx = (TestContext*)context;
    ctx->monitor_count++;
    strncpy(ctx->last_message, text, sizeof(ctx->last_message) - 1);
}

void setUp(void) {
    memset(&test_ctx, 0, sizeof(TestContext));
    
    // Initialize all components
    DiagLogger_Init();
    
    DiagMonitorConfig monitor_config = {
        .type = MONITOR_TYPE_CUSTOM,
        .config.custom = {
            .context = &test_ctx,
            .write = test_monitor_write
        }
    };
    DiagMonitor_Init(&monitor_config);
    
    DiagRecorderConfig recorder_config = {
        .max_entries = 1000,
        .circular_buffer = true,
        .auto_start = true,
        .export_path = "integration_test.txt"
    };
    DiagRecorder_Init(&recorder_config);
    
    // Register callbacks
    DiagLogger_RegisterCallback(test_log_callback, &test_ctx);
}

void tearDown(void) {
    DiagRecorder_Deinit();
    DiagMonitor_Deinit();
    DiagLogger_Deinit();
}

// Complete diagnostic flow with logging, monitoring, and recording
void test_DiagIntegration_CompleteFlow(void) {
    // 1. Start diagnostic session with logging
    uint8_t start_session[] = {
        0x02,  // Format version
        0x10,  // Service ID (DiagnosticSessionControl)
        0x03,  // Programming session
        0x00   // Padding
    };
    
    LOG_INFO(LOG_CAT_CORE, "Starting diagnostic session");
    DiagCore_HandleMessage(start_session, sizeof(start_session));
    
    TEST_ASSERT_TRUE(test_ctx.log_count > 0);
    TEST_ASSERT_TRUE(test_ctx.monitor_count > 0);
    TEST_ASSERT_EQUAL(1, DiagRecorder_GetEntryCount());
    
    // 2. Security access with all components active
    uint8_t security_request[] = {
        0x02,  // Format version
        0x27,  // Service ID (SecurityAccess)
        0x01,  // RequestSeed
        0x00   // Padding
    };
    
    LOG_INFO(LOG_CAT_SECURITY, "Requesting security access");
    DiagCore_HandleMessage(security_request, sizeof(security_request));
    
    const DiagRecordEntry* security_entry = DiagRecorder_GetEntry(1);
    TEST_ASSERT_NOT_NULL(security_entry);
    TEST_ASSERT_EQUAL(RECORD_TYPE_SECURITY, security_entry->type);
    
    // 3. Test error handling across components
    uint8_t invalid_message[] = {
        0xFF,  // Invalid format
        0x00,
        0x00,
        0x00
    };
    
    LOG_WARNING(LOG_CAT_CORE, "Testing error handling");
    DiagCore_HandleMessage(invalid_message, sizeof(invalid_message));
    
    // Verify error was logged, monitored, and recorded
    TEST_ASSERT_TRUE(strstr(test_ctx.last_message, "ERROR") != NULL);
    
    const DiagRecordEntry* error_entry = DiagRecorder_GetEntry(2);
    TEST_ASSERT_NOT_NULL(error_entry);
    TEST_ASSERT_EQUAL(RECORD_TYPE_ERROR, error_entry->type);
    
    // 4. Performance test with all components
    LOG_INFO(LOG_CAT_CORE, "Starting performance test");
    
    uint32_t start_time = DiagTimer_GetTimestamp();
    
    for (int i = 0; i < 100; i++) {
        uint8_t test_message[] = {
            0x02,
            0x3E,  // TesterPresent
            0x00,
            0x00
        };
        DiagCore_HandleMessage(test_message, sizeof(test_message));
    }
    
    uint32_t elapsed = DiagTimer_GetTimestamp() - start_time;
    TEST_ASSERT_TRUE(elapsed < 1000);  // Should complete within 1 second
    
    // 5. Verify statistics
    DiagRecorderStats stats;
    DiagRecorder_GetStats(&stats);
    
    TEST_ASSERT_TRUE(stats.message_count > 100);
    TEST_ASSERT_TRUE(stats.error_count > 0);
    TEST_ASSERT_TRUE(stats.security_events > 0);
    
    // 6. Export and verify recording
    TEST_ASSERT_TRUE(DiagRecorder_ExportToFile("integration_test.txt"));
    
    FILE* f = fopen("integration_test.txt", "r");
    TEST_ASSERT_NOT_NULL(f);
    fclose(f);
    remove("integration_test.txt");
}

// Test resource cleanup and error recovery
void test_DiagIntegration_ResourceManagement(void) {
    // 1. Stress test component initialization/deinitialization
    for (int i = 0; i < 10; i++) {
        DiagLogger_Deinit();
        DiagMonitor_Deinit();
        DiagRecorder_Deinit();
        
        setUp();  // Reinitialize everything
        
        // Verify components are working
        LOG_INFO(LOG_CAT_CORE, "Test iteration %d", i);
        TEST_ASSERT_TRUE(test_ctx.log_count > 0);
    }
    
    // 2. Test memory cleanup
    DiagRecorderConfig config = {
        .max_entries = 1000000,  // Intentionally large
        .circular_buffer = true,
        .auto_start = true
    };
    
    // Should fail gracefully
    TEST_ASSERT_FALSE(DiagRecorder_Init(&config));
    
    // 3. Verify no memory leaks (manual verification needed)
    setUp();  // Reinitialize with normal configuration
    
    // Fill recorder with data
    for (int i = 0; i < 1000; i++) {
        uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
        DiagRecorder_AddCustomRecord(i, data, sizeof(data));
    }
    
    // Clear everything
    DiagRecorder_Clear();
    TEST_ASSERT_EQUAL(0, DiagRecorder_GetEntryCount());
}

// Test concurrent operations
void test_DiagIntegration_ConcurrentOperations(void) {
    // 1. Simulate rapid logging while recording
    for (int i = 0; i < 100; i++) {
        LOG_INFO(LOG_CAT_CORE, "Rapid log %d", i);
        
        uint8_t data[] = {i & 0xFF};
        DiagRecorder_AddCustomRecord(i, data, sizeof(data));
        
        if (i % 10 == 0) {
            DiagTimer_Process();  // Process any pending timers
        }
    }
    
    TEST_ASSERT_EQUAL(100, DiagRecorder_GetEntryCount());
    TEST_ASSERT_TRUE(test_ctx.log_count >= 100);
    
    // 2. Test monitor disable/enable during operations
    DiagMonitor_Disable();
    
    uint32_t previous_monitor_count = test_ctx.monitor_count;
    LOG_INFO(LOG_CAT_CORE, "This should not be monitored");
    TEST_ASSERT_EQUAL(previous_monitor_count, test_ctx.monitor_count);
    
    DiagMonitor_Enable();
    LOG_INFO(LOG_CAT_CORE, "This should be monitored");
    TEST_ASSERT_EQUAL(previous_monitor_count + 1, test_ctx.monitor_count);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_DiagIntegration_CompleteFlow);
    RUN_TEST(test_DiagIntegration_ResourceManagement);
    RUN_TEST(test_DiagIntegration_ConcurrentOperations);
    
    return UNITY_END();
} 