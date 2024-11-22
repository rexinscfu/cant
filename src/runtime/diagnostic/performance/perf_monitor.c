#include "perf_monitor.h"
#include "../logging/diag_logger.h"
#include "../os/critical.h"
#include "../os/timer.h"
#include <string.h>

#define MAX_HISTORY_ENTRIES 1000

typedef struct {
    uint32_t timestamp;
    uint32_t value;
} MetricHistoryEntry;

typedef struct {
    PerfStats stats;
    MetricHistoryEntry* history;
    uint32_t history_size;
    uint32_t history_index;
} MetricData;

typedef struct {
    PerfConfig config;
    MetricData metrics[PERF_METRIC_COUNT];
    uint32_t last_process_time;
    bool initialized;
} PerfMonitor;

static PerfMonitor perf_monitor;

static const char* metric_names[] = {
    "CPU_USAGE",
    "MEMORY_USAGE",
    "RESPONSE_TIME",
    "QUEUE_LENGTH",
    "ERROR_RATE"
};

bool Perf_Init(const PerfConfig* config) {
    if (!config) {
        return false;
    }

    memset(&perf_monitor, 0, sizeof(PerfMonitor));
    memcpy(&perf_monitor.config, config, sizeof(PerfConfig));

    // Allocate history buffers
    uint32_t history_size = config->max_history_size;
    if (history_size > MAX_HISTORY_ENTRIES) {
        history_size = MAX_HISTORY_ENTRIES;
    }

    for (int i = 0; i < PERF_METRIC_COUNT; i++) {
        perf_monitor.metrics[i].history = (MetricHistoryEntry*)malloc(
            history_size * sizeof(MetricHistoryEntry));
        if (!perf_monitor.metrics[i].history) {
            Perf_Deinit();
            return false;
        }
        perf_monitor.metrics[i].history_size = history_size;
        perf_monitor.metrics[i].stats.min_value = UINT32_MAX;
    }

    perf_monitor.last_process_time = Timer_GetMilliseconds();
    perf_monitor.initialized = true;

    Logger_Log(LOG_LEVEL_INFO, "PERF", "Performance monitor initialized");
    return true;
}

void Perf_Deinit(void) {
    if (!perf_monitor.initialized) {
        return;
    }

    for (int i = 0; i < PERF_METRIC_COUNT; i++) {
        if (perf_monitor.metrics[i].history) {
            free(perf_monitor.metrics[i].history);
        }
    }

    Logger_Log(LOG_LEVEL_INFO, "PERF", "Performance monitor deinitialized");
    memset(&perf_monitor, 0, sizeof(PerfMonitor));
}

void Perf_UpdateMetric(PerfMetric metric, uint32_t value) {
    if (!perf_monitor.initialized || metric >= PERF_METRIC_COUNT) {
        return;
    }

    enter_critical();

    MetricData* data = &perf_monitor.metrics[metric];
    PerfStats* stats = &data->stats;

    // Update current value and history
    stats->current_value = value;
    data->history[data->history_index].timestamp = Timer_GetMilliseconds();
    data->history[data->history_index].value = value;
    data->history_index = (data->history_index + 1) % data->history_size;

    // Update statistics
    if (value < stats->min_value) {
        stats->min_value = value;
    }
    if (value > stats->max_value) {
        stats->max_value = value;
    }

    stats->total_samples++;
    stats->avg_value = ((stats->avg_value * (stats->total_samples - 1)) + 
                       value) / stats->total_samples;

    // Check thresholds
    if (value > perf_monitor.config.error_thresholds[metric]) {
        stats->threshold_violations++;
        if (perf_monitor.config.enable_logging) {
            Logger_Log(LOG_LEVEL_ERROR, "PERF", 
                      "%s exceeded error threshold: %u (threshold: %u)",
                      metric_names[metric], value,
                      perf_monitor.config.error_thresholds[metric]);
        }
    } else if (value > perf_monitor.config.warning_thresholds[metric]) {
        if (perf_monitor.config.enable_logging) {
            Logger_Log(LOG_LEVEL_WARNING, "PERF", 
                      "%s exceeded warning threshold: %u (threshold: %u)",
                      metric_names[metric], value,
                      perf_monitor.config.warning_thresholds[metric]);
        }
    }

    exit_critical();
}

void Perf_GetStats(PerfMetric metric, PerfStats* stats) {
    if (!perf_monitor.initialized || metric >= PERF_METRIC_COUNT || !stats) {
        return;
    }

    enter_critical();
    memcpy(stats, &perf_monitor.metrics[metric].stats, sizeof(PerfStats));
    exit_critical();
}

void Perf_ResetStats(PerfMetric metric) {
    if (!perf_monitor.initialized || metric >= PERF_METRIC_COUNT) {
        return;
    }

    enter_critical();
    MetricData* data = &perf_monitor.metrics[metric];
    memset(&data->stats, 0, sizeof(PerfStats));
    data->stats.min_value = UINT32_MAX;
    data->history_index = 0;
    exit_critical();
}

void Perf_ProcessMetrics(void) {
    if (!perf_monitor.initialized) {
        return;
    }

    uint32_t current_time = Timer_GetMilliseconds();
    if (current_time - perf_monitor.last_process_time < 
        perf_monitor.config.sampling_interval_ms) {
        return;
    }

    enter_critical();
    perf_monitor.last_process_time = current_time;

    // Process and analyze metrics
    for (int i = 0; i < PERF_METRIC_COUNT; i++) {
        MetricData* data = &perf_monitor.metrics[i];
        
        // Calculate trends and patterns
        uint32_t trend = calculate_metric_trend(data);
        if (trend > perf_monitor.config.warning_thresholds[i]) {
            Logger_Log(LOG_LEVEL_WARNING, "PERF", 
                      "%s showing concerning trend: %u/s",
                      metric_names[i], trend);
        }
    }

    exit_critical();
} 