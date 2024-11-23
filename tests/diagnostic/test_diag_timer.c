#include "unity.h"
#include "../../src/runtime/diagnostic/diag_timer.h"
#include "../../src/runtime/diagnostic/diag_error.h"
#include <string.h>

// Test context
typedef struct {
    uint32_t callback_count;
    uint32_t last_timer_id;
    void* last_context;
    uint32_t elapsed_time;
    bool callback_called;
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
static void test_timer_callback(uint32_t timer_id, void* context) {
    test_ctx.callback_count++;
    test_ctx.last_timer_id = timer_id;
    test_ctx.last_context = context;
    test_ctx.callback_called = true;
}

void setUp(void) {
    memset(&test_ctx, 0, sizeof(TestContext));
    mock_time = 0;
    
    // Override timestamp function with mock
    DiagTimer_SetTimestampFunction(mock_get_timestamp);
    
    // Initialize timer system
    DiagTimer_Init();
}

void tearDown(void) {
    DiagTimer_Deinit();
}

// Basic timer tests
void test_DiagTimer_BasicOperation(void) {
    uint32_t timer_id = DiagTimer_Start(TIMER_TYPE_REQUEST, 100, 
                                      test_timer_callback, NULL);
    
    TEST_ASSERT_NOT_EQUAL(0, timer_id);
    TEST_ASSERT_FALSE(test_ctx.callback_called);
    
    // Advance time just before expiration
    mock_advance_time(99);
    DiagTimer_Process();
    TEST_ASSERT_FALSE(test_ctx.callback_called);
    
    // Advance to expiration
    mock_advance_time(1);
    DiagTimer_Process();
    TEST_ASSERT_TRUE(test_ctx.callback_called);
    TEST_ASSERT_EQUAL(timer_id, test_ctx.last_timer_id);
}

void test_DiagTimer_MultipleTimers(void) {
    uint32_t timer1 = DiagTimer_Start(TIMER_TYPE_REQUEST, 100, 
                                    test_timer_callback, (void*)1);
    uint32_t timer2 = DiagTimer_Start(TIMER_TYPE_REQUEST, 200, 
                                    test_timer_callback, (void*)2);
    uint32_t timer3 = DiagTimer_Start(TIMER_TYPE_REQUEST, 300, 
                                    test_timer_callback, (void*)3);
    
    TEST_ASSERT_NOT_EQUAL(0, timer1);
    TEST_ASSERT_NOT_EQUAL(0, timer2);
    TEST_ASSERT_NOT_EQUAL(0, timer3);
    
    // First timer expires
    mock_advance_time(100);
    DiagTimer_Process();
    TEST_ASSERT_EQUAL(1, test_ctx.callback_count);
    TEST_ASSERT_EQUAL(1, test_ctx.last_context);
    
    // Second timer expires
    mock_advance_time(100);
    DiagTimer_Process();
    TEST_ASSERT_EQUAL(2, test_ctx.callback_count);
    TEST_ASSERT_EQUAL(2, test_ctx.last_context);
    
    // Third timer expires
    mock_advance_time(100);
    DiagTimer_Process();
    TEST_ASSERT_EQUAL(3, test_ctx.callback_count);
    TEST_ASSERT_EQUAL(3, test_ctx.last_context);
}

void test_DiagTimer_StopTimer(void) {
    uint32_t timer_id = DiagTimer_Start(TIMER_TYPE_REQUEST, 100, 
                                      test_timer_callback, NULL);
    
    TEST_ASSERT_TRUE(DiagTimer_Stop(timer_id));
    
    // Advance past expiration
    mock_advance_time(200);
    DiagTimer_Process();
    TEST_ASSERT_FALSE(test_ctx.callback_called);
}

void test_DiagTimer_RestartTimer(void) {
    uint32_t timer_id = DiagTimer_Start(TIMER_TYPE_REQUEST, 100, 
                                      test_timer_callback, NULL);
    
    // Advance halfway
    mock_advance_time(50);
    DiagTimer_Process();
    TEST_ASSERT_FALSE(test_ctx.callback_called);
    
    // Restart timer
    TEST_ASSERT_TRUE(DiagTimer_Restart(timer_id, 100));
    
    // Advance past original expiration
    mock_advance_time(50);
    DiagTimer_Process();
    TEST_ASSERT_FALSE(test_ctx.callback_called);
    
    // Advance to new expiration
    mock_advance_time(50);
    DiagTimer_Process();
    TEST_ASSERT_TRUE(test_ctx.callback_called);
}

// Edge cases and stress tests
void test_DiagTimer_MaximumTimers(void) {
    uint32_t timers[MAX_TIMERS + 1];
    int successful_timers = 0;
    
    // Try to create more than maximum allowed timers
    for (int i = 0; i <= MAX_TIMERS; i++) {
        timers[i] = DiagTimer_Start(TIMER_TYPE_REQUEST, 100 + i, 
                                  test_timer_callback, (void*)(uintptr_t)i);
        if (timers[i] != 0) successful_timers++;
    }
    
    TEST_ASSERT_EQUAL(MAX_TIMERS, successful_timers);
}

void test_DiagTimer_TimerOverflow(void) {
    // Set mock time near uint32_t max
    mock_time = UINT32_MAX - 1000;
    
    uint32_t timer_id = DiagTimer_Start(TIMER_TYPE_REQUEST, 2000, 
                                      test_timer_callback, NULL);
    
    TEST_ASSERT_NOT_EQUAL(0, timer_id);
    
    // Advance time past overflow
    mock_advance_time(1500);
    DiagTimer_Process();
    TEST_ASSERT_FALSE(test_ctx.callback_called);
    
    mock_advance_time(500);
    DiagTimer_Process();
    TEST_ASSERT_TRUE(test_ctx.callback_called);
}

void test_DiagTimer_PerformanceMultipleTimers(void) {
    const int NUM_TIMERS = 100;
    uint32_t timers[NUM_TIMERS];
    uint32_t start_time;
    
    // Start multiple timers
    start_time = mock_get_timestamp();
    for (int i = 0; i < NUM_TIMERS; i++) {
        timers[i] = DiagTimer_Start(TIMER_TYPE_REQUEST, 100 + i, 
                                  test_timer_callback, (void*)(uintptr_t)i);
        TEST_ASSERT_NOT_EQUAL(0, timers[i]);
    }
    test_ctx.elapsed_time = mock_get_timestamp() - start_time;
    
    // Verify reasonable performance for timer creation
    TEST_ASSERT_TRUE(test_ctx.elapsed_time < 50);  // Less than 50ms for 100 timers
    
    // Process timers
    start_time = mock_get_timestamp();
    for (int i = 0; i < 10; i++) {  // Process multiple times
        mock_advance_time(10);
        DiagTimer_Process();
    }
    test_ctx.elapsed_time = mock_get_timestamp() - start_time;
    
    // Verify reasonable performance for timer processing
    TEST_ASSERT_TRUE(test_ctx.elapsed_time < 20);  // Less than 20ms for processing
}

void test_DiagTimer_ConcurrentModification(void) {
    // Callback that starts a new timer
    void nested_timer_callback(uint32_t timer_id, void* context) {
        test_ctx.callback_count++;
        // Try to start a new timer from callback
        uint32_t new_timer = DiagTimer_Start(TIMER_TYPE_REQUEST, 50, 
                                           test_timer_callback, NULL);
        TEST_ASSERT_NOT_EQUAL(0, new_timer);
    }
    
    uint32_t timer_id = DiagTimer_Start(TIMER_TYPE_REQUEST, 100, 
                                      nested_timer_callback, NULL);
    
    TEST_ASSERT_NOT_EQUAL(0, timer_id);
    
    // Advance to trigger first timer
    mock_advance_time(100);
    DiagTimer_Process();
    TEST_ASSERT_EQUAL(1, test_ctx.callback_count);
    
    // Advance to trigger nested timer
    mock_advance_time(50);
    DiagTimer_Process();
    TEST_ASSERT_EQUAL(2, test_ctx.callback_count);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_DiagTimer_BasicOperation);
    RUN_TEST(test_DiagTimer_MultipleTimers);
    RUN_TEST(test_DiagTimer_StopTimer);
    RUN_TEST(test_DiagTimer_RestartTimer);
    RUN_TEST(test_DiagTimer_MaximumTimers);
    RUN_TEST(test_DiagTimer_TimerOverflow);
    RUN_TEST(test_DiagTimer_PerformanceMultipleTimers);
    RUN_TEST(test_DiagTimer_ConcurrentModification);
    
    return UNITY_END();
} 