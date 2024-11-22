#include "critical.h"

void init_critical(CriticalSection* cs) {
    if (!cs) return;
    cs->saved_primask = 0;
    cs->is_locked = false;
}

void enter_critical(CriticalSection* cs) {
    if (!cs) return;
    
    // Save current interrupt state and disable interrupts
    cs->saved_primask = __get_PRIMASK();
    __disable_irq();
    cs->is_locked = true;
}

void exit_critical(CriticalSection* cs) {
    if (!cs || !cs->is_locked) return;
    
    // Restore previous interrupt state
    if (!cs->saved_primask) {
        __enable_irq();
    }
    cs->is_locked = false;
}

bool is_in_critical(const CriticalSection* cs) {
    return cs ? cs->is_locked : false;
} 