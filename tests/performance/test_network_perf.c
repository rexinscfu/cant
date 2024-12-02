#include "unity.h"
#include "../../src/runtime/network/message_handler.h"
#include "../../src/runtime/memory/mem_pool.h"
#include "../../src/runtime/diagnostic/perf_monitor.h"
#include <string.h>

#define TEST_ITERATIONS 1000
#define MSG_SIZE 64

static uint8_t test_data[MSG_SIZE];
static PerfStats perf_stats;
static uint32_t start_time;

void setUp(void) {
    MessageHandler_Init();
    MemPool_Init();
    PerfMonitor_Init();
    
    for(int i = 0; i < MSG_SIZE; i++) {
        test_data[i] = i & 0xFF;
    }
}

void test_message_throughput(void) {
    start_time = TIMER_GetMs();
    
    for(int i = 0; i < TEST_ITERATIONS; i++) {
        PerfMonitor_StartMeasurement();
        TEST_ASSERT_TRUE(MessageHandler_Send(test_data, MSG_SIZE));
        MessageHandler_Process();
        PerfMonitor_StopMeasurement();
    }
    
    PerfMonitor_GetStats(&perf_stats);
    
    uint32_t elapsed = TIMER_GetMs() - start_time;
    uint32_t msgs_per_sec = (TEST_ITERATIONS * 1000) / elapsed;
    
    printf("Throughput: %d msg/s\n", msgs_per_sec);
    printf("Avg process time: %d us\n", perf_stats.avg_process_time);
    printf("Max process time: %d us\n", perf_stats.max_process_time);
}

void test_memory_stress(void) {
    uint8_t* ptrs[32];
    uint32_t alloc_fails = 0;
    
    for(int i = 0; i < 1000; i++) {
        for(int j = 0; j < 32; j++) {
            ptrs[j] = MemPool_Alloc();
            if(!ptrs[j]) alloc_fails++;
        }
        
        MessageHandler_Process();
        
        for(int j = 0; j < 32; j++) {
            if(ptrs[j]) MemPool_Free(ptrs[j]);
        }
    }
    
    printf("Alloc fails: %d\n", alloc_fails);
    TEST_ASSERT_LESS_THAN(50, alloc_fails);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_message_throughput);
    RUN_TEST(test_memory_stress);
    return UNITY_END();
} 