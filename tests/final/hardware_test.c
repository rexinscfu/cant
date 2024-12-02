#include "unity.h"
#include "../../src/hardware/timer_hw.h"
#include "../../src/hardware/dma_config.h"
#include "../../src/runtime/core/cache_opt.h"
#include <string.h>

void setUp(void) {
    TIMER_InitFast();
    DMA_Init();
    enable_cache();
}

void test_timer_precision(void) {
    uint32_t start, end;
    uint32_t max_drift = 0;
    
    for(int i = 0; i < 1000; i++) {
        start = TIMER_GetUsFast();
        TIMER_DelayUs(100);
        end = TIMER_GetUsFast();
        
        uint32_t elapsed = end - start;
        uint32_t drift = (elapsed > 100) ? (elapsed - 100) : (100 - elapsed);
        if(drift > max_drift) max_drift = drift;
    }
    
    printf("Max Timer Drift: %dus\n", max_drift);
    TEST_ASSERT_LESS_THAN(5, max_drift);
}

void test_dma_transfer(void) {
    uint8_t src_buffer[1024];
    uint8_t dst_buffer[1024];
    
    for(int i = 0; i < sizeof(src_buffer); i++) {
        src_buffer[i] = i & 0xFF;
    }
    
    // Start DMA transfer
    DMA_StartReceive();
    
    // Wait for completion
    uint32_t timeout = 1000;
    while(DMA_GetPosition() < sizeof(src_buffer) && timeout--) {
        TIMER_DelayUs(10);
    }
    
    TEST_ASSERT_NOT_EQUAL(0, timeout);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(src_buffer, dst_buffer, sizeof(src_buffer));
}

void test_cache_performance(void) {
    uint32_t start, end;
    uint8_t test_data[4096];
    
    // Test with cache
    start = TIMER_GetUsFast();
    for(int i = 0; i < 1000; i++) {
        memcpy(test_data, test_data + 2048, 2048);
    }
    end = TIMER_GetUsFast();
    uint32_t cache_time = end - start;
    
    // Test without cache
    invalidate_cache();
    start = TIMER_GetUsFast();
    for(int i = 0; i < 1000; i++) {
        memcpy(test_data, test_data + 2048, 2048);
    }
    end = TIMER_GetUsFast();
    uint32_t nocache_time = end - start;
    
    printf("Cache Performance Gain: %d%%\n", 
           (nocache_time - cache_time) * 100 / nocache_time);
    TEST_ASSERT_GREATER_THAN(cache_time, nocache_time);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_timer_precision);
    RUN_TEST(test_dma_transfer);
    RUN_TEST(test_cache_performance);
    return UNITY_END();
} 