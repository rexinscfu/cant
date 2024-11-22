#ifndef CANT_WATCHDOG_H
#define CANT_WATCHDOG_H

typedef struct Watchdog Watchdog;

Watchdog* watchdog_create(unsigned int timeout_ms);
void watchdog_start(Watchdog* watchdog);
void watchdog_stop(Watchdog* watchdog);
void watchdog_pat(Watchdog* watchdog);

#endif // CANT_WATCHDOG_H 