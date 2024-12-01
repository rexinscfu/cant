#ifndef PERF_MONITOR_H
#define PERF_MONITOR_H

#include <stdint.h>

typedef struct {
    uint32_t msg_count;
    uint32_t avg_process_time;
    uint32_t max_process_time;
    uint32_t buffer_usage;
    uint32_t route_hits;
    uint32_t route_misses;
} PerfStats;

void PerfMonitor_Init(void);
void PerfMonitor_StartMeasurement(void);
void PerfMonitor_StopMeasurement(void);
void PerfMonitor_GetStats(PerfStats* stats);
void PerfMonitor_Reset(void);

#endif 