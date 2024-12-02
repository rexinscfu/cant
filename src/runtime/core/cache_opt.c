#include "stm32f4xx_hal.h"
#include <stdint.h>

// Align buffers to cache line size
#define CACHE_LINE_SIZE 32

// Optimize frequently accessed data
__attribute__((aligned(CACHE_LINE_SIZE)))
static uint8_t fast_buffer[1024];

void enable_cache(void) {
    SCB_EnableICache();
    SCB_EnableDCache();
    
    // Configure MPU for frequently accessed regions
    MPU_Region_InitTypeDef MPU_InitStruct;
    
    HAL_MPU_Disable();
    
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress = (uint32_t)fast_buffer;
    MPU_InitStruct.Size = MPU_REGION_SIZE_1KB;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
    
    HAL_MPU_ConfigRegion(&MPU_InitStruct);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

void flush_cache(void) {
    SCB_CleanDCache();
}

void invalidate_cache(void) {
    SCB_InvalidateDCache();
}

static uint32_t cache_hits = 0;
static uint32_t cache_misses = 0; 