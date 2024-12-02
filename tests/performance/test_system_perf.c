#include "unity.h"
#include "../../src/runtime/core/sys_monitor.h"
#include "../../src/runtime/memory/mem_pool.h"
#include "../../src/runtime/network/buffer_manager.h"
#include "../../src/hardware/timer_hw.h"
#include <string.h>

#define STRESS_ITERATIONS 5000
#define ALLOC_SIZE 48
#define MAX_ALLOCS 24

static void* ptrs[MAX_ALLOCS];
static uint8_t test_data[256];
static SystemStats sys_stats;

void setUp(void) {
    SysMonitor_Init();
    MemPool_Init();
    BufferManager_Init();
    memset(ptrs, 0, sizeof(ptrs));
    
    for(int i = 0; i < sizeof(test_data); i++) {
        test_data[i] = (i * 7 + 13) & 0xFF;
    }
}

void test_memory_fragmentation(void) {
    uint32_t fail_count = 0;
    uint32_t start_time = TIMER_GetMs();
    
    for(int iter = 0; iter < STRESS_ITERATIONS; iter++) {
        // Random allocation pattern
        for(int i = 0; i < MAX_ALLOCS; i++) {
            if(!(iter & 3)) {  // 25% chance to free
                if(ptrs[i]) {
                    MemPool_Free(ptrs[i]);
                    ptrs[i] = NULL;
                }
            } else {  // 75% chance to alloc
                if(!ptrs[i]) {
                    ptrs[i] = MemPool_Alloc();
                    if(!ptrs[i]) fail_count++;
                }
            }
        }
        
        if(iter % 100 == 0) {
            MemPool_GarbageCollect();
        }
    }
    
    uint32_t elapsed = TIMER_GetMs() - start_time;
    printf("Memory test: %d ms, fails: %d\n", elapsed, fail_count);
    TEST_ASSERT_LESS_THAN(STRESS_ITERATIONS/10, fail_count);
}

void test_buffer_throughput(void) {
    uint32_t success = 0;
    uint32_t start_time = TIMER_GetMs();
    
    for(int i = 0; i < 1000; i++) {
        uint8_t* buf = BufferManager_Alloc();
        if(buf) {
            memcpy(buf, test_data, 256);
            BufferManager_Free(buf);
            success++;
        }
        BufferManager_Process();
    }
    
    uint32_t elapsed = TIMER_GetMs() - start_time;
    uint32_t rate = (success * 1000) / elapsed;
    printf("Buffer rate: %d/sec\n", rate);
    TEST_ASSERT_GREATER_THAN(500, rate);
}

void test_system_monitoring(void) {
    // Simulate load
    for(int i = 0; i < 100; i++) {
        void* p = MemPool_Alloc();
        if(p) MemPool_Free(p);
        BufferManager_Process();
        SysMonitor_Update();
        TIMER_DelayMs(10);
    }
    
    SysMonitor_GetStats(&sys_stats);
    TEST_ASSERT_NOT_EQUAL(0, sys_stats.msg_rate);
    TEST_ASSERT_NOT_EQUAL(0, sys_stats.avg_mem_usage);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_memory_fragmentation);
    RUN_TEST(test_buffer_throughput);
    RUN_TEST(test_system_monitoring);
    return UNITY_END();
} 