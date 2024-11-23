#include "unity.h"
#include "../../src/runtime/diagnostic/diag_state.h"
#include "../../src/runtime/diagnostic/diag_timer.h"
#include "../../src/runtime/diagnostic/diag_error.h"
#include <string.h>

// Test context structure
typedef struct {
    DiagState last_from_state;
    DiagState last_to_state;
    DiagStateEvent last_event;
    uint32_t callback_count;
    bool callback_called;
} TestContext;

static TestContext test_ctx;

// Test callback function
static void test_state_callback(const DiagStateTransition* transition, void* context) {
    TestContext* ctx = (TestContext*)context;
    ctx->last_from_state = transition->from_state;
    ctx->last_to_state = transition->to_state;
    ctx->last_event = transition->event;
    ctx->callback_count++;
    ctx->callback_called = true;
}

void setUp(void) {
    // Initialize test context
    memset(&test_ctx, 0, sizeof(TestContext));
    
    // Initialize required subsystems
    DiagTimer_Init();
    DiagError_Init();
    DiagState_Init();
}

void tearDown(void) {
    DiagState_Deinit();
    DiagError_Deinit();
    DiagTimer_Deinit();
}

// Basic initialization tests
void test_DiagState_InitializationSuccess(void) {
    TEST_ASSERT_EQUAL(STATE_UNINIT, DiagState_GetCurrent());
    TEST_ASSERT_TRUE(DiagState_HandleEvent(STATE_EVENT_INIT, NULL) == STATE_RESULT_OK);
    TEST_ASSERT_EQUAL(STATE_IDLE, DiagState_GetCurrent());
}

void test_DiagState_DoubleInitializationFails(void) {
    TEST_ASSERT_FALSE(DiagState_Init());
}

// State transition tests
void test_DiagState_ValidTransitions(void) {
    // Register callback
    TEST_ASSERT_TRUE(DiagState_RegisterCallback(test_state_callback, &test_ctx));
    
    // Test UNINIT -> IDLE transition
    TEST_ASSERT_TRUE(DiagState_HandleEvent(STATE_EVENT_INIT, NULL) == STATE_RESULT_OK);
    TEST_ASSERT_EQUAL(STATE_IDLE, DiagState_GetCurrent());
    TEST_ASSERT_TRUE(test_ctx.callback_called);
    TEST_ASSERT_EQUAL(STATE_UNINIT, test_ctx.last_from_state);
    TEST_ASSERT_EQUAL(STATE_IDLE, test_ctx.last_to_state);
    
    // Reset callback flag
    test_ctx.callback_called = false;
    
    // Test IDLE -> SESSION_STARTING transition
    TEST_ASSERT_TRUE(DiagState_HandleEvent(STATE_EVENT_SESSION_START, NULL) == STATE_RESULT_OK);
    TEST_ASSERT_EQUAL(STATE_SESSION_STARTING, DiagState_GetCurrent());
    TEST_ASSERT_TRUE(test_ctx.callback_called);
    TEST_ASSERT_EQUAL(STATE_IDLE, test_ctx.last_from_state);
    TEST_ASSERT_EQUAL(STATE_SESSION_STARTING, test_ctx.last_to_state);
}

void test_DiagState_InvalidTransitions(void) {
    // Try invalid transition from UNINIT to SESSION_ACTIVE
    TEST_ASSERT_TRUE(DiagState_HandleEvent(STATE_EVENT_MESSAGE_RECEIVED, NULL) == STATE_RESULT_INVALID_STATE);
    TEST_ASSERT_EQUAL(STATE_UNINIT, DiagState_GetCurrent());
    
    // Initialize to IDLE state
    DiagState_HandleEvent(STATE_EVENT_INIT, NULL);
    
    // Try invalid transition from IDLE to SECURITY_ACTIVE
    TEST_ASSERT_TRUE(DiagState_HandleEvent(STATE_EVENT_MESSAGE_RECEIVED, NULL) == STATE_RESULT_INVALID_STATE);
    TEST_ASSERT_EQUAL(STATE_IDLE, DiagState_GetCurrent());
}

// Custom state tests
static DiagStateResult custom_state_handler(DiagStateEvent event, void* data) {
    if (event == STATE_EVENT_CUSTOM_START) {
        return STATE_IDLE;
    }
    return STATE_RESULT_INVALID_EVENT;
}

static bool custom_state_enter(void* data) {
    test_ctx.callback_called = true;
    return true;
}

static bool custom_state_exit(void* data) {
    test_ctx.callback_count++;
    return true;
}

void test_DiagState_CustomStates(void) {
    DiagCustomStateHandler custom_handler = {
        .state = STATE_CUSTOM_START,
        .enter = custom_state_enter,
        .exit = custom_state_exit,
        .handle_event = custom_state_handler
    };
    
    // Register custom state
    TEST_ASSERT_TRUE(DiagState_RegisterCustomState(&custom_handler));
    
    // Force transition to custom state
    TEST_ASSERT_TRUE(DiagState_ForceState(STATE_CUSTOM_START, NULL) == STATE_RESULT_OK);
    TEST_ASSERT_EQUAL(STATE_CUSTOM_START, DiagState_GetCurrent());
    TEST_ASSERT_TRUE(test_ctx.callback_called);
    
    // Test custom state handler
    TEST_ASSERT_TRUE(DiagState_HandleEvent(STATE_EVENT_CUSTOM_START, NULL) == STATE_RESULT_OK);
    TEST_ASSERT_EQUAL(STATE_IDLE, DiagState_GetCurrent());
    TEST_ASSERT_EQUAL(1, test_ctx.callback_count);
}

// Timeout tests
void test_DiagState_TransitionTimeout(void) {
    // Register callback
    DiagState_RegisterCallback(test_state_callback, &test_ctx);
    
    // Initialize to IDLE state
    DiagState_HandleEvent(STATE_EVENT_INIT, NULL);
    
    // Start session with artificially long delay
    test_ctx.callback_called = false;
    
    // Simulate timeout by advancing time
    // Note: This requires timer mock implementation
    DiagTimer_AdvanceTime(STATE_TRANSITION_TIMEOUT + 100);
    
    TEST_ASSERT_TRUE(test_ctx.callback_called);
    TEST_ASSERT_EQUAL(STATE_ERROR, DiagState_GetCurrent());
}

// Error handling tests
void test_DiagState_ErrorHandling(void) {
    // Initialize to IDLE state
    DiagState_HandleEvent(STATE_EVENT_INIT, NULL);
    
    // Trigger error event
    TEST_ASSERT_TRUE(DiagState_HandleEvent(STATE_EVENT_ERROR, NULL) == STATE_RESULT_OK);
    TEST_ASSERT_EQUAL(STATE_ERROR, DiagState_GetCurrent());
    
    // Verify error can be reset
    TEST_ASSERT_TRUE(DiagState_HandleEvent(STATE_EVENT_RESET, NULL) == STATE_RESULT_OK);
    TEST_ASSERT_EQUAL(STATE_IDLE, DiagState_GetCurrent());
}

// Performance tests
void test_DiagState_PerformanceTransitions(void) {
    const int NUM_TRANSITIONS = 1000;
    uint32_t start_time = DiagTimer_GetTimestamp();
    
    for (int i = 0; i < NUM_TRANSITIONS; i++) {
        DiagState_HandleEvent(STATE_EVENT_INIT, NULL);
        DiagState_HandleEvent(STATE_EVENT_SESSION_START, NULL);
        DiagState_HandleEvent(STATE_EVENT_MESSAGE_RECEIVED, NULL);
        DiagState_HandleEvent(STATE_EVENT_SESSION_END, NULL);
    }
    
    uint32_t elapsed = DiagTimer_GetTimestamp() - start_time;
    
    // Ensure reasonable performance (adjust threshold as needed)
    TEST_ASSERT_TRUE(elapsed < 1000);  // Less than 1ms per transition cycle
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_DiagState_InitializationSuccess);
    RUN_TEST(test_DiagState_DoubleInitializationFails);
    RUN_TEST(test_DiagState_ValidTransitions);
    RUN_TEST(test_DiagState_InvalidTransitions);
    RUN_TEST(test_DiagState_CustomStates);
    RUN_TEST(test_DiagState_TransitionTimeout);
    RUN_TEST(test_DiagState_ErrorHandling);
    RUN_TEST(test_DiagState_PerformanceTransitions);
    
    return UNITY_END();
} 