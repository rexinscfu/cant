#include "unity.h"
#include "../../src/runtime/core/sys_monitor.h"
#include "../../src/runtime/memory/mem_pool.h"
#include "../../src/runtime/network/message_handler.h"
#include "../../src/runtime/diagnostic/perf_monitor.h"
#include "../../src/hardware/timer_hw.h"
#include <string.h>

static uint8_t test_buffer[2048];
static SystemStats sys_stats;
static PerfStats perf_stats;
static uint32_t start_time;

void setUp(void) {
    SysMonitor_Init();
    MemPool_Init();
    MessageHandler_Init();
    PerfMonitor_Init();
    memset(test_buffer, 0, sizeof(test_buffer));
    start_time = TIMER_GetMs();
}

void test_full_message_chain(void) {
    const int NUM_MESSAGES = 500;
    uint32_t success_count = 0;
    
    for(int i = 0; i < NUM_MESSAGES; i++) {
        // Prepare test message
        test_buffer[0] = 0x55;  // start marker
        test_buffer[1] = 64;    // length
        for(int j = 0; j < 64; j++) {
            test_buffer[j+2] = (i + j) & 0xFF;
        }
        
        PerfMonitor_StartMeasurement();
        if(MessageHandler_Send(test_buffer, 68)) {
            success_count++;
        }
        MessageHandler_Process();
        PerfMonitor_StopMeasurement();
        
        if(i % 50 == 0) {
            SysMonitor_Update();
        }
    }
    
    PerfMonitor_GetStats(&perf_stats);
    SysMonitor_GetStats(&sys_stats);
    
    printf("Message Success Rate: %d%%\n", (success_count * 100) / NUM_MESSAGES);
    printf("Avg Process Time: %dus\n", perf_stats.avg_process_time);
    printf("Memory Usage: %d%%\n", (sys_stats.avg_mem_usage * 100) / POOL_NUM_BLOCKS);
    
    TEST_ASSERT_GREATER_THAN(NUM_MESSAGES * 0.95, success_count);
    TEST_ASSERT_LESS_THAN(1000, perf_stats.avg_process_time);
}

void test_stress_conditions(void) {
    const int STRESS_TIME = 5000; // 5 seconds
    uint32_t msg_sent = 0;
    uint32_t msg_failed = 0;
    
    while(TIMER_GetMs() - start_time < STRESS_TIME) {
        // Rapid message sending
        for(int i = 0; i < 10; i++) {
            test_buffer[0] = 0x55;
            test_buffer[1] = 32;
            if(MessageHandler_Send(test_buffer, 34)) {
                msg_sent++;
            } else {
                msg_failed++;
            }
        }
        
        MessageHandler_Process();
        SysMonitor_Update();
        
        // Simulate real-world delays
        TIMER_DelayMs(1);
    }
    
    SysMonitor_GetStats(&sys_stats);
    printf("Messages Sent: %d\n", msg_sent);
    printf("Messages Failed: %d\n", msg_failed);
    printf("Buffer Usage: %d%%\n", sys_stats.avg_buf_usage);
    
    TEST_ASSERT_LESS_THAN(msg_sent * 0.1, msg_failed);
}

void test_memory_stability(void) {
    void* ptrs[50];
    uint32_t alloc_fails = 0;
    uint32_t initial_free = MemPool_GetFreeBlocks();
    
    for(int iter = 0; iter < 100; iter++) {
        // Allocate memory blocks
        for(int i = 0; i < 50; i++) {
            ptrs[i] = MemPool_Alloc();
            if(!ptrs[i]) alloc_fails++;
        }
        
        // Use the memory
        for(int i = 0; i < 50; i++) {
            if(ptrs[i]) {
                memset(ptrs[i], 0xAA, 64);
            }
        }
        
        // Free memory blocks
        for(int i = 0; i < 50; i++) {
            if(ptrs[i]) {
                MemPool_Free(ptrs[i]);
                ptrs[i] = NULL;
            }
        }
        
        MessageHandler_Process();
        SysMonitor_Update();
    }
    
    uint32_t final_free = MemPool_GetFreeBlocks();
    printf("Memory Alloc Fails: %d\n", alloc_fails);
    printf("Memory Leak: %d blocks\n", initial_free - final_free);
    
    TEST_ASSERT_EQUAL(initial_free, final_free);
}

void test_error_recovery(void) {
    uint32_t recovery_count = 0;
    
    // Simulate error conditions
    for(int i = 0; i < 100; i++) {
        // Overflow test
        test_buffer[0] = 0x55;
        test_buffer[1] = 255;  // invalid length
        MessageHandler_Send(test_buffer, 257);
        
        // Invalid message format
        test_buffer[0] = 0x00;  // invalid marker
        MessageHandler_Send(test_buffer, 32);
        
        MessageHandler_Process();
        SysMonitor_Update();
        
        // Check if system recovered
        if(MessageHandler_Send(test_buffer, 32)) {
            recovery_count++;
        }
    }
    
    SysMonitor_GetStats(&sys_stats);
    printf("Recovery Rate: %d%%\n", recovery_count);
    TEST_ASSERT_GREATER_THAN(90, recovery_count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_full_message_chain);
    RUN_TEST(test_stress_conditions);
    RUN_TEST(test_memory_stability);
    RUN_TEST(test_error_recovery);
    return UNITY_END();
} 