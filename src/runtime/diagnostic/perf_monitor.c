#include "perf_monitor.h"
#include "../hardware/timer_hw.h"

static PerfStats stats;
static uint32_t start_time;
static uint32_t total_time;
static uint32_t measurement_count;

void PerfMonitor_Init(void) {
    memset(&stats, 0, sizeof(PerfStats));
    total_time = 0;
    measurement_count = 0;
}

void PerfMonitor_StartMeasurement(void) {
    start_time = TIMER_GetUs();
}

void PerfMonitor_StopMeasurement(void) {
    uint32_t elapsed = TIMER_GetUs() - start_time;
    total_time += elapsed;
    measurement_count++;
    
    if(elapsed > stats.max_process_time) {
        stats.max_process_time = elapsed;
    }
    
    stats.avg_process_time = total_time / measurement_count;
}

void PerfMonitor_GetStats(PerfStats* out_stats) {
    if(out_stats) {
        memcpy(out_stats, &stats, sizeof(PerfStats));
    }
}

void PerfMonitor_Reset(void) {
    memset(&stats, 0, sizeof(PerfStats));
    total_time = 0;
    measurement_count = 0;
}

static uint32_t last_reset = 0; 