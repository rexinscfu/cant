#ifndef CANT_TIMING_MONITOR_H
#define CANT_TIMING_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TIMING_SERVICE_HANDLER = 0,
    TIMING_SECURITY_CHECK,
    TIMING_DATA_TRANSFER,
    TIMING_ROUTINE_EXECUTION,
    TIMING_COUNT
} TimingMetric;

typedef struct {
    uint32_t min_time_us;
    uint32_t max_time_us;
    uint32_t avg_time_us;
    uint32_t last_time_us;
    uint32_t total_samples;
    uint32_t violations;
} TimingStats;

typedef struct {
    uint32_t warning_threshold_us;
    uint32_t error_threshold_us;
    bool enable_monitoring;
    bool log_violations;
    uint32_t max_samples;
} TimingConfig;

bool Timing_Init(const TimingConfig* config);
void Timing_Deinit(void);

void Timing_StartMeasurement(TimingMetric metric);
void Timing_StopMeasurement(TimingMetric metric);
void Timing_GetStats(TimingMetric metric, TimingStats* stats);
void Timing_ResetStats(TimingMetric metric);

#endif // CANT_TIMING_MONITOR_H 