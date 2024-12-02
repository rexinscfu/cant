#include "stm32f4xx_hal.h"
#include "timer_hw.h"

static TIM_HandleTypeDef htim3;
static volatile uint32_t us_ticks = 0;

void TIMER_InitFast(void) {
    __HAL_RCC_TIM3_CLK_ENABLE();
    
    // Configure for 1MHz counting
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = (SystemCoreClock/1000000) - 1;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 0xFFFF;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    
    HAL_TIM_Base_Init(&htim3);
    HAL_TIM_Base_Start_IT(&htim3);
    
    // Higher priority for precise timing
    HAL_NVIC_SetPriority(TIM3_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM3_IRQn);
}

uint32_t TIMER_GetUsFast(void) {
    return (us_ticks << 16) | __HAL_TIM_GET_COUNTER(&htim3);
}

void TIM3_IRQHandler(void) {
    if(__HAL_TIM_GET_FLAG(&htim3, TIM_FLAG_UPDATE)) {
        __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);
        us_ticks++;
    }
}

// Fast inline delay for short durations
__attribute__((always_inline)) static inline void delay_cycles(uint32_t cycles) {
    cycles /= 4;  // Approximate cycles per loop
    while(cycles--) {
        __asm volatile ("nop");
    }
}

static uint32_t last_tick = 0;
static bool high_res_enabled = false; 