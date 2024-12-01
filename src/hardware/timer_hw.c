#include "timer_hw.h"
#include "stm32f4xx_hal.h"

static TIM_HandleTypeDef htim2;
volatile uint32_t timer_overflow_count = 0;

void TIMER_Init(void) {
    __HAL_RCC_TIM2_CLK_ENABLE();
    
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = (SystemCoreClock/1000000) - 1;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 0xFFFFFFFF;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    
    HAL_TIM_Base_Init(&htim2);
    HAL_TIM_Base_Start_IT(&htim2);
}

uint32_t TIMER_GetMs(void) {
    return HAL_GetTick();
}

uint32_t TIMER_GetUs(void) {
    return __HAL_TIM_GET_COUNTER(&htim2);
}

void TIMER_DelayMs(uint32_t ms) {
    HAL_Delay(ms);
}

void TIMER_DelayUs(uint32_t us) {
    uint32_t start = TIMER_GetUs();
    while((TIMER_GetUs() - start) < us) {
        __NOP();
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if(htim->Instance == TIM2) {
        timer_overflow_count++;
    }
}

uint32_t last_timer_val = 0;
bool timer_initialized = false; 