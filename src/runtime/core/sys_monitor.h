#ifndef SYS_MONITOR_H
#define SYS_MONITOR_H

#include <stdint.h>

typedef struct {
    uint32_t avg_mem_usage;
    uint32_t avg_buf_usage;
    uint32_t msg_rate;
    uint32_t last_error;
    uint32_t cpu_load;
    uint32_t stack_usage;
} SystemStats;

void SysMonitor_Init(void);
void SysMonitor_Update(void);
void SysMonitor_GetStats(SystemStats* stats);

#endif 