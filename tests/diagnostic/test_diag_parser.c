#include "unity.h"
#include "../../src/runtime/diagnostic/diag_parser.h"
#include "../../src/runtime/diagnostic/diag_error.h"
#include "../../src/runtime/memory/memory_manager.h"
#include <string.h>

// Test message buffers
static uint8_t test_buffer[4096];
static uint8_t response_buffer[4096];

// Test context
typedef struct {
    bool callback_called;
    DiagMessage last_message;
    DiagResponse last_response;
    uint32_t callback_count;
} TestContext;

static TestContext test_ctx;

void setUp(void) {
    memset(&test_ctx, 0, sizeof(TestContext));
    memset(test_buffer, 0, sizeof(test_buffer));
    memset(response_buffer, 0, sizeof(response_buffer));
    
    // Initialize required subsystems
    MemoryManager_Init();
    DiagError_Init();
}

void tearDown(void) {
    if (test_ctx.last_message.data) {
        MEMORY_FREE(test_ctx.last_message.data);
    }
    if (test_ctx.last_response.data) {
        MEMORY_FREE(test_ctx.last_response.data);
    }
    
    DiagError_Deinit();
    MemoryManager_Deinit();
}

// Helper function to create test messages
static void create_test_message(uint8_t service_id, uint8_t sub_function,
                              const uint8_t* data, uint32_t data_length) 
{
    test_buffer[0] = FORMAT_VERSION;
    test_buffer[1] = data_length + 2;  // +2 for service ID and sub-function
    test_buffer[2] = service_id;
    test_buffer[3] = sub_function;
    
    if (data && data_length > 0) {
        memcpy(&test_buffer[4], data, data_length);
    }
    
    // Calculate checksum
    uint8_t checksum = 0;
    for (uint32_t i = 0; i < data_length + 4; i++) {
        checksum += test_buffer[i];
    }
    test_buffer[data_length + 4] = ~checksum + 1;
}

// Basic parsing tests
void test_DiagParser_BasicMessageParsing(void) {
    const uint8_t test_data[] = {0x11, 0x22, 0x33, 0x44};
    create_test_message(DIAG_SID_READ_DATA_BY_ID, 0x00, test_data, sizeof(test_data));
    
    DiagMessage message;
    DiagParserResult result = DiagParser_ParseRequest(test_buffer, 
                                                    sizeof(test_data) + 5,
                                                    &message);
    
    TEST_ASSERT_EQUAL(PARSER_RESULT_OK, result);
    TEST_ASSERT_EQUAL(DIAG_SID_READ_DATA_BY_ID, message.service_id);
    TEST_ASSERT_EQUAL(sizeof(test_data), message.length);
    TEST_ASSERT_EQUAL_MEMORY(test_data, message.data, sizeof(test_data));
    
    if (message.data) MEMORY_FREE(message.data);
}

void test_DiagParser_InvalidFormat(void) {
    // Test with invalid version
    test_buffer[0] = 0xFF;  // Invalid version
    
    DiagMessage message;
    DiagParserResult result = DiagParser_ParseRequest(test_buffer, 5, &message);
    
    TEST_ASSERT_EQUAL(PARSER_RESULT_INVALID_FORMAT, result);
}

void test_DiagParser_InvalidLength(void) {
    create_test_message(DIAG_SID_READ_DATA_BY_ID, 0x00, NULL, 0);
    test_buffer[1] = 0xFF;  // Invalid length
    
    DiagMessage message;
    DiagParserResult result = DiagParser_ParseRequest(test_buffer, 5, &message);
    
    TEST_ASSERT_EQUAL(PARSER_RESULT_INVALID_LENGTH, result);
}

void test_DiagParser_InvalidChecksum(void) {
    const uint8_t test_data[] = {0x11, 0x22};
    create_test_message(DIAG_SID_READ_DATA_BY_ID, 0x00, test_data, sizeof(test_data));
    test_buffer[sizeof(test_data) + 4] ^= 0xFF;  // Corrupt checksum
    
    DiagMessage message;
    DiagParserResult result = DiagParser_ParseRequest(test_buffer, 
                                                    sizeof(test_data) + 5,
                                                    &message);
    
    TEST_ASSERT_EQUAL(PARSER_RESULT_INVALID_FORMAT, result);
}

// Response parsing tests
void test_DiagParser_ResponseParsing(void) {
    // Create positive response
    response_buffer[0] = FORMAT_VERSION;
    response_buffer[1] = 0x03;  // Length
    response_buffer[2] = DIAG_SID_READ_DATA_BY_ID + 0x40;  // Response SID
    response_buffer[3] = DIAG_RESP_POSITIVE;
    response_buffer[4] = 0x00;  // Data
    response_buffer[5] = calculate_checksum(response_buffer, 5);
    
    DiagResponse response;
    DiagParserResult result = DiagParser_ParseResponse(response_buffer, 6, &response);
    
    TEST_ASSERT_EQUAL(PARSER_RESULT_OK, result);
    TEST_ASSERT_EQUAL(DIAG_SID_READ_DATA_BY_ID + 0x40, response.service_id);
    TEST_ASSERT_TRUE(response.success);
    
    if (response.data) MEMORY_FREE(response.data);
}

void test_DiagParser_NegativeResponse(void) {
    // Create negative response
    response_buffer[0] = FORMAT_VERSION;
    response_buffer[1] = 0x03;
    response_buffer[2] = DIAG_SID_READ_DATA_BY_ID + 0x40;
    response_buffer[3] = DIAG_RESP_GENERAL_REJECT;
    response_buffer[4] = 0x00;
    response_buffer[5] = calculate_checksum(response_buffer, 5);
    
    DiagResponse response;
    DiagParserResult result = DiagParser_ParseResponse(response_buffer, 6, &response);
    
    TEST_ASSERT_EQUAL(PARSER_RESULT_OK, result);
    TEST_ASSERT_FALSE(response.success);
    TEST_ASSERT_EQUAL(DIAG_RESP_GENERAL_REJECT, response.response_code);
    
    if (response.data) MEMORY_FREE(response.data);
}

// Message formatting tests
void test_DiagParser_FormatRequest(void) {
    DiagMessage message = {
        .service_id = DIAG_SID_READ_DATA_BY_ID,
        .sub_function = 0x00,
        .data = (uint8_t*)"\x11\x22\x33",
        .length = 3
    };
    
    uint32_t length;
    TEST_ASSERT_TRUE(DiagParser_FormatRequest(&message, test_buffer, &length));
    
    TEST_ASSERT_EQUAL(8, length);  // Version + Length + SID + SubFunc + Data(3) + Checksum
    TEST_ASSERT_EQUAL(FORMAT_VERSION, test_buffer[0]);
    TEST_ASSERT_EQUAL(5, test_buffer[1]);  // Length field
    TEST_ASSERT_EQUAL(DIAG_SID_READ_DATA_BY_ID, test_buffer[2]);
}

// Performance tests
void test_DiagParser_PerformanceParsing(void) {
    const int NUM_MESSAGES = 1000;
    const uint8_t test_data[] = {0x11, 0x22, 0x33, 0x44};
    create_test_message(DIAG_SID_READ_DATA_BY_ID, 0x00, test_data, sizeof(test_data));
    
    uint32_t start_time = DiagTimer_GetTimestamp();
    DiagMessage message;
    
    for (int i = 0; i < NUM_MESSAGES; i++) {
        DiagParserResult result = DiagParser_ParseRequest(test_buffer, 
                                                        sizeof(test_data) + 5,
                                                        &message);
        TEST_ASSERT_EQUAL(PARSER_RESULT_OK, result);
        if (message.data) MEMORY_FREE(message.data);
    }
    
    uint32_t elapsed = DiagTimer_GetTimestamp() - start_time;
    
    // Ensure reasonable performance (adjust threshold as needed)
    TEST_ASSERT_TRUE(elapsed < 500);  // Less than 0.5ms per parse
}

// Edge cases and stress tests
void test_DiagParser_MaximumMessageSize(void) {
    uint8_t large_data[MAX_MESSAGE_LENGTH];
    memset(large_data, 0xAA, sizeof(large_data));
    
    create_test_message(DIAG_SID_WRITE_DATA_BY_ID, 0x00, 
                       large_data, sizeof(large_data));
    
    DiagMessage message;
    DiagParserResult result = DiagParser_ParseRequest(test_buffer,
                                                    sizeof(large_data) + 5,
                                                    &message);
    
    TEST_ASSERT_EQUAL(PARSER_RESULT_OK, result);
    TEST_ASSERT_EQUAL(sizeof(large_data), message.length);
    
    if (message.data) MEMORY_FREE(message.data);
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_DiagParser_BasicMessageParsing);
    RUN_TEST(test_DiagParser_InvalidFormat);
    RUN_TEST(test_DiagParser_InvalidLength);
    RUN_TEST(test_DiagParser_InvalidChecksum);
    RUN_TEST(test_DiagParser_ResponseParsing);
    RUN_TEST(test_DiagParser_NegativeResponse);
    RUN_TEST(test_DiagParser_FormatRequest);
    RUN_TEST(test_DiagParser_PerformanceParsing);
    RUN_TEST(test_DiagParser_MaximumMessageSize);
    
    return UNITY_END();
} 