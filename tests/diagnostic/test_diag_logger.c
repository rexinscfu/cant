#include "unity.h"
#include "../../src/runtime/diagnostic/diag_logger.h"
#include "../../src/runtime/diagnostic/diag_timer.h"
#include <string.h>

// Test context
typedef struct {
    bool callback_called;
    DiagLogEntry last_entry;
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
static void test_log_callback(const DiagLogEntry* entry, void* context) {
    test_ctx.callback_called = true;
    memcpy(&test_ctx.last_entry, entry, sizeof(DiagLogEntry));
    test_ctx.last_context = context;
    test_ctx.callback_count++;
}

void setUp(void) {
    memset(&test_ctx, 0, sizeof(TestContext));
    mock_time = 0;
    
    DiagTimer_SetTimestampFunction(mock_get_timestamp);
    DiagTimer_Init();
    DiagLogger_Init();
}

void tearDown(void) {
    DiagLogger_Deinit();
    DiagTimer_Deinit();
}

// Basic logging tests
void test_DiagLogger_BasicLogging(void) {
    DiagLogger_RegisterCallback(test_log_callback, (void*)0x1234);
    
    DiagLogger_Log(LOG_LEVEL_INFO, LOG_CAT_CORE, "Test message %d", 42);
    
    TEST_ASSERT_TRUE(test_ctx.callback_called);
    TEST_ASSERT_EQUAL(LOG_LEVEL_INFO, test_ctx.last_entry.level);
    TEST_ASSERT_EQUAL(LOG_CAT_CORE, test_ctx.last_entry.category);
    TEST_ASSERT_EQUAL_STRING("Test message 42", test_ctx.last_entry.message);
    TEST_ASSERT_EQUAL(0, test_ctx.last_entry.timestamp);
    TEST_ASSERT_EQUAL(0x1234, (uintptr_t)test_ctx.last_context);
}

void test_DiagLogger_LogLevels(void) {
    DiagLogger_RegisterCallback(test_log_callback, NULL);
    
    // Set log level to WARNING
    DiagLogger_SetLevel(LOG_LEVEL_WARNING);
    
    // INFO message should be filtered
    test_ctx.callback_called = false;
    DiagLogger_Log(LOG_LEVEL_INFO, LOG_CAT_CORE, "Info message");
    TEST_ASSERT_FALSE(test_ctx.callback_called);
    
    // WARNING message should pass
    DiagLogger_Log(LOG_LEVEL_WARNING, LOG_CAT_CORE, "Warning message");
    TEST_ASSERT_TRUE(test_ctx.callback_called);
    TEST_ASSERT_EQUAL(LOG_LEVEL_WARNING, test_ctx.last_entry.level);
    
    // ERROR message should pass
    test_ctx.callback_called = false;
    DiagLogger_Log(LOG_LEVEL_ERROR, LOG_CAT_CORE, "Error message");
    TEST_ASSERT_TRUE(test_ctx.callback_called);
    TEST_ASSERT_EQUAL(LOG_LEVEL_ERROR, test_ctx.last_entry.level);
}

void test_DiagLogger_HexLogging(void) {
    DiagLogger_RegisterCallback(test_log_callback, NULL);
    
    uint8_t test_data[] = {0x12, 0x34, 0x56, 0x78};
    DiagLogger_LogHex(LOG_LEVEL_DEBUG, LOG_CAT_PARSER, 
                     "Test hex data", test_data, sizeof(test_data));
    
    TEST_ASSERT_TRUE(test_ctx.callback_called);
    TEST_ASSERT_EQUAL(LOG_LEVEL_DEBUG, test_ctx.last_entry.level);
    TEST_ASSERT_EQUAL(LOG_CAT_PARSER, test_ctx.last_entry.category);
    TEST_ASSERT_EQUAL(sizeof(test_data), test_ctx.last_entry.data_length);
    TEST_ASSERT_EQUAL_MEMORY(test_data, test_ctx.last_entry.data, sizeof(test_data));
}

void test_DiagLogger_MultipleCallbacks(void) {
    // Test context for second callback
    TestContext test_ctx2;
    memset(&test_ctx2, 0, sizeof(TestContext));
    
    void second_callback(const DiagLogEntry* entry, void* context) {
        TestContext* ctx = (TestContext*)context;
        ctx->callback_called = true;
        memcpy(&ctx->last_entry, entry, sizeof(DiagLogEntry));
        ctx->callback_count++;
    }
    
    DiagLogger_RegisterCallback(test_log_callback, &test_ctx);
    DiagLogger_RegisterCallback(second_callback, &test_ctx2);
    
    DiagLogger_Log(LOG_LEVEL_INFO, LOG_CAT_CORE, "Test multiple callbacks");
    
    TEST_ASSERT_TRUE(test_ctx.callback_called);
    TEST_ASSERT_TRUE(test_ctx2.callback_called);
    TEST_ASSERT_EQUAL_STRING("Test multiple callbacks", 
                           test_ctx.last_entry.message);
    TEST_ASSERT_EQUAL_STRING("Test multiple callbacks", 
                           test_ctx2.last_entry.message);
}

void test_DiagLogger_BufferBehavior(void) {
    DiagLogger_RegisterCallback(test_log_callback, NULL);
    
    // Fill buffer without triggering flush
    for (int i = 0; i < 31; i++) {
        DiagLogger_Log(LOG_LEVEL_INFO, LOG_CAT_CORE, "Message %d", i);
        TEST_ASSERT_EQUAL(0, test_ctx.callback_count);
    }
    
    // This should trigger buffer flush
    DiagLogger_Log(LOG_LEVEL_INFO, LOG_CAT_CORE, "Final message");
    TEST_ASSERT_EQUAL(32, test_ctx.callback_count);
}

void test_DiagLogger_ErrorAutoFlush(void) {
    DiagLogger_RegisterCallback(test_log_callback, NULL);
    
    // Log some regular messages
    for (int i = 0; i < 5; i++) {
        DiagLogger_Log(LOG_LEVEL_INFO, LOG_CAT_CORE, "Info %d", i);
    }
    TEST_ASSERT_EQUAL(0, test_ctx.callback_count);
    
    // Error message should trigger immediate flush
    DiagLogger_Log(LOG_LEVEL_ERROR, LOG_CAT_CORE, "Error message");
    TEST_ASSERT_EQUAL(6, test_ctx.callback_count);
}

void test_DiagLogger_Timestamps(void) {
    DiagLogger_RegisterCallback(test_log_callback, NULL);
    
    mock_advance_time(1000);
    DiagLogger_Log(LOG_LEVEL_INFO, LOG_CAT_CORE, "Message 1");
    
    mock_advance_time(500);
    DiagLogger_Log(LOG_LEVEL_ERROR, LOG_CAT_CORE, "Message 2");
    
    TEST_ASSERT_EQUAL(1500, test_ctx.last_entry.timestamp);
}

void test_DiagLogger_SequenceNumbers(void) {
    DiagLogger_RegisterCallback(test_log_callback, NULL);
    
    // Log multiple messages and verify sequence
    for (int i = 0; i < 5; i++) {
        DiagLogger_Log(LOG_LEVEL_INFO, LOG_CAT_CORE, "Message %d", i);
    }
    
    // Force flush
    DiagLogger_Log(LOG_LEVEL_ERROR, LOG_CAT_CORE, "Final");
    
    TEST_ASSERT_EQUAL(5, test_ctx.last_entry.sequence);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_DiagLogger_BasicLogging);
    RUN_TEST(test_DiagLogger_LogLevels);
    RUN_TEST(test_DiagLogger_HexLogging);
    RUN_TEST(test_DiagLogger_MultipleCallbacks);
    RUN_TEST(test_DiagLogger_BufferBehavior);
    RUN_TEST(test_DiagLogger_ErrorAutoFlush);
    RUN_TEST(test_DiagLogger_Timestamps);
    RUN_TEST(test_DiagLogger_SequenceNumbers);
    
    return UNITY_END();
} 