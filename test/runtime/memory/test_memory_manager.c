#include "unity.h"
#include "memory/memory_manager.h"
#include <string.h>

static MemConfig test_config;
static const uint32_t TEST_HEAP_SIZE = 1024 * 1024; // 1MB
static const uint32_t POOL_SIZES[] = {32, 64, 128, 256};
static const uint32_t POOL_COUNTS[] = {32, 16, 8, 4};

void setUp(void) {
    memset(&test_config, 0, sizeof(MemConfig));
    test_config.heap_size = TEST_HEAP_SIZE;
    test_config.pool_sizes = POOL_SIZES;
    test_config.pool_counts = POOL_COUNTS;
    test_config.pool_count = sizeof(POOL_SIZES) / sizeof(POOL_SIZES[0]);
    test_config.enable_guards = true;
    test_config.enable_tracking = true;
    test_config.enable_stats = true;
    
    TEST_ASSERT_TRUE(Memory_Init(&test_config));
}

void tearDown(void) {
    Memory_Deinit();
}

void test_Memory_BasicAllocation(void) {
    // Test basic allocation
    void* ptr1 = MEMORY_ALLOC(100);
    TEST_ASSERT_NOT_NULL(ptr1);
    
    // Test zero allocation
    void* ptr2 = MEMORY_ALLOC(0);
    TEST_ASSERT_NULL(ptr2);
    
    // Test large allocation
    void* ptr3 = MEMORY_ALLOC(TEST_HEAP_SIZE * 2);
    TEST_ASSERT_NULL(ptr3);
    
    MEMORY_FREE(ptr1);
}

void test_Memory_MultipleAllocations(void) {
    void* ptrs[10];
    
    // Allocate multiple blocks
    for (int i = 0; i < 10; i++) {
        ptrs[i] = MEMORY_ALLOC(100);
        TEST_ASSERT_NOT_NULL(ptrs[i]);
        memset(ptrs[i], i, 100);
    }
    
    // Verify data
    for (int i = 0; i < 10; i++) {
        uint8_t* data = (uint8_t*)ptrs[i];
        for (int j = 0; j < 100; j++) {
            TEST_ASSERT_EQUAL_UINT8(i, data[j]);
        }
    }
    
    // Free blocks
    for (int i = 0; i < 10; i++) {
        MEMORY_FREE(ptrs[i]);
    }
}

void test_Memory_PoolAllocation(void) {
    void* small_ptrs[32];
    
    // Test pool allocations
    for (int i = 0; i < 32; i++) {
        small_ptrs[i] = MEMORY_ALLOC(24); // Should use 32-byte pool
        TEST_ASSERT_NOT_NULL(small_ptrs[i]);
    }
    
    // Free pool allocations
    for (int i = 0; i < 32; i++) {
        MEMORY_FREE(small_ptrs[i]);
    }
}

void test_Memory_Realloc(void) {
    // Initial allocation
    void* ptr = MEMORY_ALLOC(50);
    TEST_ASSERT_NOT_NULL(ptr);
    memset(ptr, 0xAA, 50);
    
    // Grow buffer
    ptr = MEMORY_REALLOC(ptr, 100);
    TEST_ASSERT_NOT_NULL(ptr);
    
    // Verify original data
    uint8_t* data = (uint8_t*)ptr;
    for (int i = 0; i < 50; i++) {
        TEST_ASSERT_EQUAL_UINT8(0xAA, data[i]);
    }
    
    // Shrink buffer
    ptr = MEMORY_REALLOC(ptr, 25);
    TEST_ASSERT_NOT_NULL(ptr);
    
    // Verify data again
    data = (uint8_t*)ptr;
    for (int i = 0; i < 25; i++) {
        TEST_ASSERT_EQUAL_UINT8(0xAA, data[i]);
    }
    
    MEMORY_FREE(ptr);
}

void test_Memory_AlignedAllocation(void) {
    void* ptr = MEMORY_ALLOC_ALIGNED(100, 16);
    TEST_ASSERT_NOT_NULL(ptr);
    TEST_ASSERT_EQUAL_UINT8(0, ((uintptr_t)ptr & 0xF));
    MEMORY_FREE(ptr);
}

void test_Memory_Stats(void) {
    MemStats stats;
    void* ptr = MEMORY_ALLOC(100);
    
    Memory_GetStats(&stats);
    TEST_ASSERT_EQUAL_UINT32(100, stats.current_usage);
    TEST_ASSERT_EQUAL_UINT32(1, stats.allocation_count);
    
    MEMORY_FREE(ptr);
    Memory_GetStats(&stats);
    TEST_ASSERT_EQUAL_UINT32(0, stats.current_usage);
    TEST_ASSERT_EQUAL_UINT32(1, stats.free_count);
}

void test_Memory_Integrity(void) {
    void* ptr = MEMORY_ALLOC(100);
    TEST_ASSERT_TRUE(Memory_CheckIntegrity());
    
    // Corrupt memory
    uint8_t* guard = (uint8_t*)ptr + 100;
    *guard = 0xFF;
    
    TEST_ASSERT_FALSE(Memory_CheckIntegrity());
    MEMORY_FREE(ptr);
} 