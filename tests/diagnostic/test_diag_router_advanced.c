#include "unity.h"
#include "../../src/runtime/diagnostic/diag_router.h"
#include "../../src/runtime/diagnostic/diag_filter.h"
#include "../../src/runtime/network/message_handler.h"
#include <string.h>

static uint8_t test_buffer[1024];
static uint32_t callback_count = 0;
static uint8_t last_msg[256];
static uint32_t last_len = 0;

void setUp(void) {
    memset(test_buffer, 0, sizeof(test_buffer));
    callback_count = 0;
    memset(last_msg, 0, sizeof(last_msg));
    last_len = 0;
    
    DiagRouter_Init();
    DiagFilter_Init();
    MessageHandler_Init();
}

void tearDown(void) {
    DiagRouter_Deinit();
}

void test_basic_routing(void) {
    uint8_t msg[] = {0x01, 0xF1, 0x10, 0x03};
    
    TEST_ASSERT_TRUE(DiagRouter_AddRoute(0x01, 0xF1, 0xFFFF));
    TEST_ASSERT_EQUAL(ROUTE_OK, DiagRouter_HandleMessage(msg, sizeof(msg)));
}

void test_filter_chain(void) {
    uint8_t msg[] = {0x01, 0xF1, 0x27, 0x01};  // security access
    
    DiagRouter_AddRoute(0x01, 0xF1, 0xFFFF);
    _set_filter_flags(0x01);  // enable security
    
    TEST_ASSERT_EQUAL(ROUTE_OK, DiagRouter_HandleMessage(msg, sizeof(msg)));
}

void test_message_overflow(void) {
    uint8_t big_msg[2048];
    memset(big_msg, 0xAA, sizeof(big_msg));
    big_msg[0] = 0x01;
    big_msg[1] = 0xF1;
    
    TEST_ASSERT_EQUAL(ROUTE_ERROR, DiagRouter_HandleMessage(big_msg, sizeof(big_msg)));
}

void test_multiple_routes(void) {
    DiagRouter_AddRoute(0x01, 0xF1, 0x10);
    DiagRouter_AddRoute(0x01, 0xF2, 0x10);
    DiagRouter_AddRoute(0x01, 0xF3, 0x10);
    
    uint8_t msg[] = {0x01, 0xF2, 0x10, 0x01};
    TEST_ASSERT_EQUAL(ROUTE_OK, DiagRouter_HandleMessage(msg, sizeof(msg)));
}

void test_route_removal(void) {
    DiagRouter_AddRoute(0x01, 0xF1, 0x10);
    TEST_ASSERT_EQUAL(ROUTE_OK, DiagRouter_RemoveRoute(0x01, 0xF1));
    
    uint8_t msg[] = {0x01, 0xF1, 0x10, 0x01};
    TEST_ASSERT_EQUAL(ROUTE_ERROR, DiagRouter_HandleMessage(msg, sizeof(msg)));
}

void test_stress_routing(void) {
    for(int i = 0; i < 100; i++) {
        uint8_t src = (i % 3) + 1;
        uint8_t tgt = 0xF0 + (i % 4);
        DiagRouter_AddRoute(src, tgt, 0xFFFF);
        
        uint8_t msg[] = {src, tgt, 0x10, 0x01};
        DiagRouter_HandleMessage(msg, sizeof(msg));
    }
    
    clear_routes();
    TEST_ASSERT_EQUAL(0, get_route_count());
}

void test_invalid_routes(void) {
    TEST_ASSERT_FALSE(DiagRouter_AddRoute(0x00, 0xF1, 0x10));
    TEST_ASSERT_FALSE(DiagRouter_AddRoute(0x01, 0x00, 0x10));
}

void test_filter_rejection(void) {
    DiagRouter_AddRoute(0x01, 0xF1, 0x27);
    _set_filter_flags(0x00);  // disable security
    
    uint8_t msg[] = {0x01, 0xF1, 0x27, 0x01};
    TEST_ASSERT_EQUAL(ROUTE_ERROR, DiagRouter_HandleMessage(msg, sizeof(msg)));
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_basic_routing);
    RUN_TEST(test_filter_chain);
    RUN_TEST(test_message_overflow);
    RUN_TEST(test_multiple_routes);
    RUN_TEST(test_route_removal);
    RUN_TEST(test_stress_routing);
    RUN_TEST(test_invalid_routes);
    RUN_TEST(test_filter_rejection);
    
    return UNITY_END();
} 