#include <gtest/gtest.h>
#include "../memory_manager.h"
#include "mocks/memory_mock.h"

class DiagMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        Memory_Mock_Init();
    }

    void TearDown() override {
        Memory_Mock_Deinit();
    }
};

TEST_F(DiagMemoryTest, ReadMemoryBoundaryConditions) {
    uint8_t test_data[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t read_buffer[4];
    UdsMessage response;

    // Test reading at start of memory
    Memory_Mock_SetMemoryContent(0x0, test_data, sizeof(test_data));
    ASSERT_TRUE(handle_read_memory_by_address(0x0, sizeof(test_data), read_buffer));
    ASSERT_EQ(memcmp(read_buffer, test_data, sizeof(test_data)), 0);

    // Test reading at end of memory
    uint32_t end_address = MOCK_MEMORY_SIZE - sizeof(test_data);
    Memory_Mock_SetMemoryContent(end_address, test_data, sizeof(test_data));
    ASSERT_TRUE(handle_read_memory_by_address(end_address, sizeof(test_data), read_buffer));
    ASSERT_EQ(memcmp(read_buffer, test_data, sizeof(test_data)), 0);

    // Test reading beyond memory bounds
    ASSERT_FALSE(handle_read_memory_by_address(MOCK_MEMORY_SIZE - 2, sizeof(test_data), read_buffer));
}

TEST_F(DiagMemoryTest, ProtectedMemoryAccess) {
    uint8_t test_data[] = {0x11, 0x22, 0x33, 0x44};
    uint8_t read_buffer[4];

    // Set up protected memory range
    Memory_Mock_SetProtectedRange(0x1000, 0x2000);
    Memory_Mock_SetMemoryContent(0x1500, test_data, sizeof(test_data));

    // Test reading protected memory without security access
    ASSERT_FALSE(handle_read_memory_by_address(0x1500, sizeof(test_data), read_buffer));

    // Test reading protected memory with security access
    Security_Mock_SetSecurityLevel(SECURITY_LEVEL_UNLOCKED);
    ASSERT_TRUE(handle_read_memory_by_address(0x1500, sizeof(test_data), read_buffer));
    ASSERT_EQ(memcmp(read_buffer, test_data, sizeof(test_data)), 0);
}

TEST_F(DiagMemoryTest, MemoryCheckRoutine) {
    // Set up test memory pattern
    uint8_t pattern[256];
    for (int i = 0; i < 256; i++) {
        pattern[i] = i;
    }
    Memory_Mock_SetMemoryContent(0x1000, pattern, sizeof(pattern));

    // Start memory check routine
    uint32_t start_address = 0x1000;
    uint32_t size = sizeof(pattern);
    ASSERT_TRUE(memory_check_start(start_address, size));

    // Set expected checksum
    uint32_t expected_checksum = 0;
    for (int i = 0; i < 256; i++) {
        expected_checksum += pattern[i];
    }
    Memory_Mock_SetChecksum(expected_checksum);
    Memory_Mock_SetCheckComplete(true);

    // Verify results
    RoutineResult result;
    ASSERT_TRUE(memory_check_get_result(&result));
    ASSERT_EQ(result.result_code, 0);
    
    uint32_t calculated_checksum;
    memcpy(&calculated_checksum, &result.data[2], sizeof(uint32_t));
    ASSERT_EQ(calculated_checksum, expected_checksum);
} 