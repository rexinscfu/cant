#include "timer.h"
#include "../os/critical.h"

// Internal timer state
static struct {
    uint32_t system_ticks;
    uint32_t tick_frequency_hz;
    CriticalSection critical;
    bool initialized;
} timer_state = {
    .system_ticks = 0,
    .tick_frequency_hz = 1000, // Default 1kHz
    .initialized = false
};

// SysTick handler - should be called from system timer interrupt
void SysTick_Handler(void) {
    enter_critical(&timer_state.critical);
    timer_state.system_ticks++;
    exit_critical(&timer_state.critical);
}

void timer_init(void) {
    if (timer_state.initialized) return;
    
    init_critical(&timer_state.critical);
    
    // Configure system timer (SysTick) for 1ms intervals
    uint32_t cpu_freq = SystemCoreClock;  // Get from system configuration
    SysTick_Config(cpu_freq / timer_state.tick_frequency_hz);
    
    timer_state.initialized = true;
}

uint32_t get_system_time_ms(void) {
    uint32_t ticks;
    
    enter_critical(&timer_state.critical);
    ticks = timer_state.system_ticks;
    exit_critical(&timer_state.critical);
    
    return ticks;
}

void timer_start(Timer* timer, uint32_t timeout_ms) {
    if (!timer) return;
    
    enter_critical(&timer_state.critical);
    timer->start_time = timer_state.system_ticks;
    timer->timeout = timeout_ms;
    timer->running = true;
    exit_critical(&timer_state.critical);
}

void timer_stop(Timer* timer) {
    if (!timer) return;
    timer->running = false;
}

bool timer_expired(const Timer* timer) {
    if (!timer || !timer->running) return false;
    
    uint32_t current_time;
    
    enter_critical(&timer_state.critical);
    current_time = timer_state.system_ticks;
    exit_critical(&timer_state.critical);
    
    return (current_time - timer->start_time) >= timer->timeout;
}

uint32_t timer_remaining(const Timer* timer) {
    if (!timer || !timer->running) return 0;
    
    uint32_t current_time;
    
    enter_critical(&timer_state.critical);
    current_time = timer_state.system_ticks;
    exit_critical(&timer_state.critical);
    
    uint32_t elapsed = current_time - timer->start_time;
    return (elapsed >= timer->timeout) ? 0 : (timer->timeout - elapsed);
}

void timer_delay_ms(uint32_t delay_ms) {
    uint32_t start = get_system_time_ms();
    while ((get_system_time_ms() - start) < delay_ms) {
        // Optional: Add yield or sleep for power efficiency
        __WFE();  // Wait for event (power saving)
    }
} 