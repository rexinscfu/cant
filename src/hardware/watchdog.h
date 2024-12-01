#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>

bool WATCHDOG_Init(uint32_t timeout_ms);
void WATCHDOG_Refresh(void);
void WATCHDOG_Force_Reset(void);
uint32_t WATCHDOG_GetResetCount(void);

#endif 