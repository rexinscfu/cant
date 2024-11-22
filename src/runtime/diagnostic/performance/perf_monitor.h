#ifndef CANT_PERF_MONITOR_H
#define CANT_PERF_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    PERF_METRIC_CPU_USAGE,
    PERF_METRIC_MEMORY_USAGE,
    PERF_METRIC_RESPONSE_TIME,
    PERF_METRIC_QUEUE_LENGTH,
    PERF_METRIC_ERROR_RATE,
    PERF_METRIC_COUNT
} PerfMetric;

typedef struct {
    uint32_t current_value;
    uint32_t min_value;
    uint32_t max_value;
    uint32_t avg_value;
    uint32_t total_samples;
    uint32_t threshold_violations;
} PerfStats;

typedef struct {
    uint32_t sampling_interval_ms;
    uint32_t warning_thresholds[PERF_METRIC_COUNT];
    uint32_t error_thresholds[PERF_METRIC_COUNT];
    bool enable_logging;
    uint32_t max_history_size;
} PerfConfig;

bool Perf_Init(const PerfConfig* config);
void Perf_Deinit(void);

void Perf_UpdateMetric(PerfMetric metric, uint32_t value);
void Perf_GetStats(PerfMetric metric, PerfStats* stats);
void Perf_ResetStats(PerfMetric metric);
void Perf_ProcessMetrics(void);

#endif // CANT_PERF_MONITOR_H 