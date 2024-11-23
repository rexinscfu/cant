#include "unity.h"
#include "network/net_buffer.h"
#include <string.h>

static NetBuffer test_buffer;
static const uint32_t BUFFER_SIZE = 1024;

void setUp(void) {
    TEST_ASSERT_TRUE(NetBuffer_Init(&test_buffer, BUFFER_SIZE));
}

void tearDown(void) {
    NetBuffer_Deinit(&test_buffer);
}

void test_NetBuffer_WriteRead(void) {
    const uint8_t test_data[] = "Hello, Network!";
    const uint32_t test_length = strlen((char*)test_data) + 1;
    
    // Test write
    TEST_ASSERT_TRUE(NetBuffer_Write(&test_buffer, test_data, test_length));
    TEST_ASSERT_EQUAL_UINT32(test_length, NetBuffer_GetAvailable(&test_buffer));
    
    // Test read
    uint8_t read_data[32];
    TEST_ASSERT_TRUE(NetBuffer_Read(&test_buffer, read_data, test_length));
    TEST_ASSERT_EQUAL_MEMORY(test_data, read_data, test_length);
    TEST_ASSERT_TRUE(NetBuffer_IsEmpty(&test_buffer));
}

void test_NetBuffer_Overflow(void) {
    uint8_t large_data[BUFFER_SIZE + 100];
    memset(large_data, 0xAA, sizeof(large_data));
    
    TEST_ASSERT_FALSE(NetBuffer_Write(&test_buffer, large_data, sizeof(large_data)));
    TEST_ASSERT_TRUE(NetBuffer_HasOverflowed(&test_buffer));
}

void test_NetBuffer_Wrap(void) {
    uint8_t data1[BUFFER_SIZE/2];
    uint8_t data2[BUFFER_SIZE/4];
    memset(data1, 0xBB, sizeof(data1));
    memset(data2, 0xCC, sizeof(data2));
    
    TEST_ASSERT_TRUE(NetBuffer_Write(&test_buffer, data1, sizeof(data1)));
    TEST_ASSERT_TRUE(NetBuffer_Read(&test_buffer, data1, sizeof(data1)));
    TEST_ASSERT_TRUE(NetBuffer_Write(&test_buffer, data2, sizeof(data2)));
    
    uint8_t read_data[BUFFER_SIZE/4];
    TEST_ASSERT_TRUE(NetBuffer_Read(&test_buffer, read_data, sizeof(read_data)));
    TEST_ASSERT_EQUAL_MEMORY(data2, read_data, sizeof(data2));
} 