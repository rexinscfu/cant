#ifndef TIMER_HW_H
#define TIMER_HW_H

#include <stdint.h>
#include <stdbool.h>

void TIMER_Init(void);
uint32_t TIMER_GetMs(void);
uint32_t TIMER_GetUs(void);
void TIMER_DelayMs(uint32_t ms);
void TIMER_DelayUs(uint32_t us);

#endif 