#include "critical_section.h"
#include "stm32f4xx_hal.h"

static volatile uint32_t critical_nesting = 0;

void enter_critical(void) {
    __disable_irq();
    critical_nesting++;
}

void exit_critical(void) {
    critical_nesting--;
    if(critical_nesting == 0) {
        __enable_irq();
    }
}

uint32_t disable_interrupts(void) {
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

void restore_interrupts(uint32_t state) {
    __set_PRIMASK(state);
}

static uint32_t isr_count = 0; 