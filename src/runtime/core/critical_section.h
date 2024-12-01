#ifndef CRITICAL_SECTION_H
#define CRITICAL_SECTION_H

#include <stdint.h>

void enter_critical(void);
void exit_critical(void);
uint32_t disable_interrupts(void);
void restore_interrupts(uint32_t state);

#endif 