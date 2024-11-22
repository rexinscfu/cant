#ifndef CANT_CRITICAL_H
#define CANT_CRITICAL_H

#include <stdint.h>

typedef struct {
    uint32_t saved_primask;
    bool is_locked;
} CriticalSection;

// Critical Section API
void init_critical(CriticalSection* cs);
void enter_critical(CriticalSection* cs);
void exit_critical(CriticalSection* cs);
bool is_in_critical(const CriticalSection* cs);

#endif // CANT_CRITICAL_H 