#ifndef CANT_TIMER_H
#define CANT_TIMER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t start_time;
    uint32_t timeout;
    bool running;
} Timer;

// Timer API
void timer_init(void);
uint32_t get_system_time_ms(void);
void timer_start(Timer* timer, uint32_t timeout_ms);
void timer_stop(Timer* timer);
bool timer_expired(const Timer* timer);
uint32_t timer_remaining(const Timer* timer);
void timer_delay_ms(uint32_t delay_ms);

#endif // CANT_TIMER_H 