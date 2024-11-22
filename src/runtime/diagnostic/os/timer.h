#ifndef CANT_OS_TIMER_H
#define CANT_OS_TIMER_H

#include <stdint.h>
#include <stdbool.h>

// Timer initialization
bool Timer_Init(void);
void Timer_Deinit(void);

// Basic timer functions
uint32_t Timer_GetMilliseconds(void);
uint32_t Timer_GetMicroseconds(void);
void Timer_DelayMilliseconds(uint32_t ms);
void Timer_DelayMicroseconds(uint32_t us);

// High precision timer functions
uint64_t Timer_GetHighResCounter(void);
uint64_t Timer_GetHighResFrequency(void);

// Timer callback management
typedef void (*TimerCallback)(void* context);

typedef struct {
    uint32_t period_ms;
    bool repeat;
    TimerCallback callback;
    void* context;
} TimerConfig;

uint32_t Timer_CreateTimer(const TimerConfig* config);
bool Timer_StartTimer(uint32_t timer_id);
bool Timer_StopTimer(uint32_t timer_id);
bool Timer_DeleteTimer(uint32_t timer_id);

// Timer processing (called from main loop)
void Timer_Process(void);

#endif // CANT_OS_TIMER_H 