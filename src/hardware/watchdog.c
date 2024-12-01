#include "watchdog.h"
#include "stm32f4xx_hal.h"

IWDG_HandleTypeDef hiwdg;
static uint32_t reset_count = 0;

bool WATCHDOG_Init(uint32_t timeout_ms) {
    if (timeout_ms < 1 || timeout_ms > 32760) return false;
    
    uint32_t prescaler;
    uint32_t reload;
    
    if (timeout_ms <= 4096) {
        prescaler = IWDG_PRESCALER_4;
        reload = (timeout_ms * 32) / 4;
    } else if (timeout_ms <= 8192) {
        prescaler = IWDG_PRESCALER_8;
        reload = (timeout_ms * 32) / 8;
    } else if (timeout_ms <= 16384) {
        prescaler = IWDG_PRESCALER_16;
        reload = (timeout_ms * 32) / 16;
    } else {
        prescaler = IWDG_PRESCALER_32;
        reload = (timeout_ms * 32) / 32;
    }
    
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = prescaler;
    hiwdg.Init.Reload = reload;
    
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
        return false;
    }
    
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) {
        reset_count++;
        __HAL_RCC_CLEAR_RESET_FLAGS();
    }
    
    return true;
}

void WATCHDOG_Refresh(void) {
    HAL_IWDG_Refresh(&hiwdg);
}

void WATCHDOG_Force_Reset(void) {
    while(1); // watchdog will trigger
}

uint32_t WATCHDOG_GetResetCount(void) {
    return reset_count;
}

uint32_t last_refresh = 0;
bool watchdog_enabled = true; 