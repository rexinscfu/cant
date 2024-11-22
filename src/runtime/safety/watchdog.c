#include "watchdog.h"
#include "../utils/timer.h"
#include "../os/critical.h"

// Internal state
static struct {
    WatchdogConfig config;
    struct {
        Timer window_timer;
        Timer timeout_timer;
        bool enabled;
        bool window_valid;
    } state;
    CriticalSection critical;
} watchdog;

// Hardware-specific watchdog functions (to be implemented per platform)
static void hw_watchdog_init(uint32_t timeout_ms) {
    // Initialize hardware watchdog timer
    // Example for STM32:
    // IWDG->KR = 0x5555;  // Enable write access
    // IWDG->PR = 0x06;    // Set prescaler
    // IWDG->RLR = (timeout_ms * 32) / 256;  // Set reload value
    // IWDG->KR = 0xCCCC;  // Start watchdog
}

static void hw_watchdog_refresh(void) {
    // Refresh hardware watchdog
    // Example for STM32:
    // IWDG->KR = 0xAAAA;
}

static void hw_watchdog_disable(void) {
    // Disable hardware watchdog (if possible)
    // Note: Many watchdogs cannot be disabled once enabled
}

bool Watchdog_Init(const WatchdogConfig* config) {
    if (!config || config->timeout_ms == 0) {
        return false;
    }
    
    enter_critical(&watchdog.critical);
    
    memcpy(&watchdog.config, config, sizeof(WatchdogConfig));
    
    // Initialize timers
    timer_init();
    timer_start(&watchdog.state.timeout_timer, config->timeout_ms);
    
    if (config->type == WATCHDOG_WINDOW) {
        timer_start(&watchdog.state.window_timer, config->window_start_ms);
        watchdog.state.window_valid = true;
    }
    
    // Initialize hardware watchdog
    hw_watchdog_init(config->timeout_ms);
    
    watchdog.state.enabled = true;
    
    exit_critical(&watchdog.critical);
    return true;
}

void Watchdog_DeInit(void) {
    enter_critical(&watchdog.critical);
    
    if (watchdog.state.enabled) {
        hw_watchdog_disable();
        watchdog.state.enabled = false;
        watchdog.state.window_valid = false;
    }
    
    exit_critical(&watchdog.critical);
}

void Watchdog_Refresh(void) {
    if (!watchdog.state.enabled) return;
    
    enter_critical(&watchdog.critical);
    
    if (watchdog.config.type == WATCHDOG_WINDOW) {
        // Check if we're in the valid window
        uint32_t elapsed = timer_remaining(&watchdog.state.window_timer);
        
        if (elapsed < watchdog.config.window_start_ms || 
            elapsed > watchdog.config.window_end_ms) {
            // Invalid refresh window
            if (watchdog.config.timeout_callback) {
                watchdog.config.timeout_callback();
            }
            
            if (watchdog.config.reset_on_timeout) {
                Watchdog_ForceReset();
            }
            
            exit_critical(&watchdog.critical);
            return;
        }
    }
    
    // Refresh hardware watchdog
    hw_watchdog_refresh();
    
    // Reset software timers
    timer_start(&watchdog.state.timeout_timer, watchdog.config.timeout_ms);
    
    if (watchdog.config.type == WATCHDOG_WINDOW) {
        timer_start(&watchdog.state.window_timer, watchdog.config.window_start_ms);
    }
    
    exit_critical(&watchdog.critical);
}

bool Watchdog_IsEnabled(void) {
    return watchdog.state.enabled;
}

uint32_t Watchdog_GetRemainingTime(void) {
    if (!watchdog.state.enabled) return 0;
    
    uint32_t remaining;
    
    enter_critical(&watchdog.critical);
    remaining = timer_remaining(&watchdog.state.timeout_timer);
    exit_critical(&watchdog.critical);
    
    return remaining;
}

void Watchdog_ForceReset(void) {
    // Force an immediate system reset
    // Example for ARM Cortex-M:
    __disable_irq();
    NVIC_SystemReset();
}

bool Watchdog_IsWindowOpen(void) {
    if (!watchdog.state.enabled || 
        watchdog.config.type != WATCHDOG_WINDOW) {
        return false;
    }
    
    bool window_open;
    
    enter_critical(&watchdog.critical);
    uint32_t elapsed = timer_remaining(&watchdog.state.window_timer);
    window_open = (elapsed >= watchdog.config.window_start_ms && 
                  elapsed <= watchdog.config.window_end_ms);
    exit_critical(&watchdog.critical);
    
    return window_open;
}

void Watchdog_SetCallback(void (*callback)(void)) {
    if (!watchdog.state.enabled) return;
    
    enter_critical(&watchdog.critical);
    watchdog.config.timeout_callback = callback;
    exit_critical(&watchdog.critical);
} 