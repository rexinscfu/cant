#ifndef CANT_WATCHDOG_H
#define CANT_WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>

// Watchdog Types
typedef enum {
    WATCHDOG_WINDOW,
    WATCHDOG_INDEPENDENT,
    WATCHDOG_EXTERNAL
} WatchdogType;

// Watchdog Configuration
typedef struct {
    WatchdogType type;
    uint32_t timeout_ms;
    uint32_t window_start_ms;  // For windowed watchdog
    uint32_t window_end_ms;    // For windowed watchdog
    bool reset_on_timeout;
    void (*timeout_callback)(void);
} WatchdogConfig;

// Watchdog API
bool Watchdog_Init(const WatchdogConfig* config);
void Watchdog_DeInit(void);
void Watchdog_Refresh(void);
bool Watchdog_IsEnabled(void);
uint32_t Watchdog_GetRemainingTime(void);
void Watchdog_ForceReset(void);
bool Watchdog_IsWindowOpen(void);
void Watchdog_SetCallback(void (*callback)(void));

#endif // CANT_WATCHDOG_H 