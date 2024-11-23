#ifndef CANT_DIAG_TIMER_H
#define DIAG_TIMER_H

#include <stdint.h>
#include <stdbool.h>

// Timer types
typedef enum {
    TIMER_TYPE_REQUEST,
    TIMER_TYPE_SESSION,
    TIMER_TYPE_SECURITY,
    TIMER_TYPE_TESTER_PRESENT
} DiagTimerType;

// Timer callback
typedef void (*DiagTimerCallback)(uint32_t timer_id, void* context);

// Timer functions
bool DiagTimer_Init(void);
void DiagTimer_Deinit(void);

uint32_t DiagTimer_Start(DiagTimerType type, uint32_t timeout_ms, DiagTimerCallback callback, void* context);
void DiagTimer_Stop(uint32_t timer_id);
void DiagTimer_Reset(uint32_t timer_id);

bool DiagTimer_IsActive(uint32_t timer_id);
uint32_t DiagTimer_GetRemaining(uint32_t timer_id);

void DiagTimer_StartRequest(uint32_t msg_id, uint32_t timeout_ms);
void DiagTimer_StartSession(uint32_t timeout_ms);
void DiagTimer_StartSecurity(uint32_t timeout_ms);
void DiagTimer_StartTesterPresent(uint32_t interval_ms);

void DiagTimer_Process(void);

#endif // CANT_DIAG_TIMER_H 