#include "unity.h"
#include "../../src/runtime/diagnostic/diag_monitor.h"
#include "../../src/runtime/diagnostic/diag_logger.h"
#include <string.h>

// Test context
typedef struct {
    char last_output[1024];
    uint32_t output_count;
    bool write_called;
} TestContext;

static TestContext test_ctx;

// Custom write function for testing
static void test_write(const char* text, void* context) {
    TestContext* ctx = (TestContext*)context;
    strncpy(ctx->last_output, text, sizeof(ctx->last_output) - 1);
    ctx->output_count++;
    ctx->write_called = true;
}

void setUp(void) {
    memset(&test_ctx, 0, sizeof(TestContext));
    DiagLogger_Init();
}

void tearDown(void) {
    DiagMonitor_Deinit();
    DiagLogger_Deinit();
}

// Monitor configuration tests
void test_DiagMonitor_FileConfig(void) {
    DiagMonitorConfig config = {
        .type = MONITOR_TYPE_FILE,
        .config.file = {
            .filename = "test_log.txt",
            .append = false
        }
    };
    
    TEST_ASSERT_TRUE(DiagMonitor_Init(&config));
    TEST_ASSERT_TRUE(DiagMonitor_IsEnabled());
    
    // Log something
    DiagLogger_Log(LOG_LEVEL_INFO, LOG_CAT_CORE, "Test file logging");
    
    // Verify file exists and contains data
    FILE* f = fopen("test_log.txt", "r");
    TEST_ASSERT_NOT_NULL(f);
    
    char buffer[256];
    TEST_ASSERT_TRUE(fgets(buffer, sizeof(buffer), f) != NULL);
    TEST_ASSERT_TRUE(strstr(buffer, "Test file logging") != NULL);
    
    fclose(f);
    remove("test_log.txt");
}

void test_DiagMonitor_ConsoleConfig(void) {
    DiagMonitorConfig config = {
        .type = MONITOR_TYPE_CONSOLE,
        .config.console = {
            .color_output = true,
            .timestamp = true
        }
    };
    
    TEST_ASSERT_TRUE(DiagMonitor_Init(&config));
    TEST_ASSERT_TRUE(DiagMonitor_IsEnabled());
    
    // Note: Console output is hard to test, mainly verify initialization
}

void test_DiagMonitor_CustomConfig(void) {
    DiagMonitorConfig config = {
        .type = MONITOR_TYPE_CUSTOM,
        .config.custom = {
            .context = &test_ctx,
            .write = test_write
        }
    };
    
    TEST_ASSERT_TRUE(DiagMonitor_Init(&config));
    TEST_ASSERT_TRUE(DiagMonitor_IsEnabled());
    
    DiagLogger_Log(LOG_LEVEL_INFO, LOG_CAT_CORE, "Test custom output");
    
    TEST_ASSERT_TRUE(test_ctx.write_called);
    TEST_ASSERT_TRUE(strstr(test_ctx.last_output, "Test custom output") != NULL);
}

// Monitor control tests
void test_DiagMonitor_EnableDisable(void) {
    DiagMonitorConfig config = {
        .type = MONITOR_TYPE_CUSTOM,
        .config.custom = {
            .context = &test_ctx,
            .write = test_write
        }
    };
    
    DiagMonitor_Init(&config);
    
    // Test disable
    DiagMonitor_Disable();
    TEST_ASSERT_FALSE(DiagMonitor_IsEnabled());
    
    test_ctx.write_called = false;
    DiagLogger_Log(LOG_LEVEL_INFO, LOG_CAT_CORE, "Should not appear");
    TEST_ASSERT_FALSE(test_ctx.write_called);
    
    // Test enable
    DiagMonitor_Enable();
    TEST_ASSERT_TRUE(DiagMonitor_IsEnabled());
    
    DiagLogger_Log(LOG_LEVEL_INFO, LOG_CAT_CORE, "Should appear");
    TEST_ASSERT_TRUE(test_ctx.write_called);
}

// Output format tests
void test_DiagMonitor_OutputFormat(void) {
    DiagMonitorConfig config = {
        .type = MONITOR_TYPE_CUSTOM,
        .config.custom = {
            .context = &test_ctx,
            .write = test_write
        }
    };
    
    DiagMonitor_Init(&config);
    
    // Test different log levels
    DiagLogger_Log(LOG_LEVEL_ERROR, LOG_CAT_CORE, "Error message");
    TEST_ASSERT_TRUE(strstr(test_ctx.last_output, "[ERROR]") != NULL);
    
    DiagLogger_Log(LOG_LEVEL_WARNING, LOG_CAT_CORE, "Warning message");
    TEST_ASSERT_TRUE(strstr(test_ctx.last_output, "[WARNING]") != NULL);
    
    // Test categories
    DiagLogger_Log(LOG_LEVEL_INFO, LOG_CAT_SECURITY, "Security message");
    TEST_ASSERT_TRUE(strstr(test_ctx.last_output, "[SECURITY]") != NULL);
}

// Performance tests
void test_DiagMonitor_Performance(void) {
    DiagMonitorConfig config = {
        .type = MONITOR_TYPE_CUSTOM,
        .config.custom = {
            .context = &test_ctx,
            .write = test_write
        }
    };
    
    DiagMonitor_Init(&config);
    
    uint32_t start_time = DiagTimer_GetTimestamp();
    
    // Log 1000 messages
    for (int i = 0; i < 1000; i++) {
        DiagLogger_Log(LOG_LEVEL_INFO, LOG_CAT_CORE, "Performance test %d", i);
    }
    
    uint32_t elapsed = DiagTimer_GetTimestamp() - start_time;
    
    // Should process 1000 messages in under 100ms
    TEST_ASSERT_TRUE(elapsed < 100);
    TEST_ASSERT_EQUAL(1000, test_ctx.output_count);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_DiagMonitor_FileConfig);
    RUN_TEST(test_DiagMonitor_ConsoleConfig);
    RUN_TEST(test_DiagMonitor_CustomConfig);
    RUN_TEST(test_DiagMonitor_EnableDisable);
    RUN_TEST(test_DiagMonitor_OutputFormat);
    RUN_TEST(test_DiagMonitor_Performance);
    
    return UNITY_END();
} 