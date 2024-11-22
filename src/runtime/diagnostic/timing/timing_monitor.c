#include "timing_monitor.h"
#include "../logging/diag_logger.h"
#include "../os/critical.h"
#include "../os/timer.h"
#include <string.h>

typedef struct {
    TimingConfig config;
    TimingStats stats[TIMING_COUNT];
    uint32_t start_times[TIMING_COUNT];
    bool measurements_active[TIMING_COUNT];
    bool initialized;
} TimingMonitor;

static TimingMonitor timing_monitor;

static const char* metric_names[] = {
    "SERVICE_HANDLER",
    "SECURITY_CHECK",
    "DATA_TRANSFER",
    "ROUTINE_EXECUTION"
};

bool Timing_Init(const TimingConfig* config) {
    if (!config) {
        return false;
    }

    memset(&timing_monitor, 0, sizeof(TimingMonitor));
    memcpy(&timing_monitor.config, config, sizeof(TimingConfig));

    // Initialize stats with maximum values for min_time
    for (int i = 0; i < TIMING_COUNT; i++) {
        timing_monitor.stats[i].min_time_us = UINT32_MAX;
    }

    timing_monitor.initialized = true;
    Logger_Log(LOG_LEVEL_INFO, "TIMING", "Timing monitor initialized");
    return true;
}

void Timing_Deinit(void) {
    Logger_Log(LOG_LEVEL_INFO, "TIMING", "Timing monitor deinitialized");
    memset(&timing_monitor, 0, sizeof(TimingMonitor));
}

void Timing_StartMeasurement(TimingMetric metric) {
    if (!timing_monitor.initialized || metric >= TIMING_COUNT) {
        return;
    }

    enter_critical();
    
    if (timing_monitor.measurements_active[metric]) {
        Logger_Log(LOG_LEVEL_WARNING, "TIMING", 
                  "Nested timing measurement detected for %s", 
                  metric_names[metric]);
        exit_critical();
        return;
    }

    timing_monitor.start_times[metric] = Timer_GetMicroseconds();
    timing_monitor.measurements_active[metric] = true;
    
    exit_critical();
}

void Timing_StopMeasurement(TimingMetric metric) {
    if (!timing_monitor.initialized || metric >= TIMING_COUNT) {
        return;
    }

    enter_critical();
    
    if (!timing_monitor.measurements_active[metric]) {
        Logger_Log(LOG_LEVEL_WARNING, "TIMING", 
                  "Stop measurement without start for %s", 
                  metric_names[metric]);
        exit_critical();
        return;
    }

    uint32_t end_time = Timer_GetMicroseconds();
    uint32_t duration = end_time - timing_monitor.start_times[metric];
    TimingStats* stats = &timing_monitor.stats[metric];

    // Update statistics
    if (duration < stats->min_time_us) {
        stats->min_time_us = duration;
    }
    if (duration > stats->max_time_us) {
        stats->max_time_us = duration;
    }

    stats->total_samples++;
    stats->last_time_us = duration;
    
    // Update running average
    stats->avg_time_us = ((stats->avg_time_us * (stats->total_samples - 1)) + 
                         duration) / stats->total_samples;

    // Check for violations
    if (duration > timing_monitor.config.error_threshold_us) {
        stats->violations++;
        if (timing_monitor.config.log_violations) {
            Logger_Log(LOG_LEVEL_ERROR, "TIMING", 
                      "%s timing violation: %u us (threshold: %u us)",
                      metric_names[metric], duration,
                      timing_monitor.config.error_threshold_us);
        }
    } else if (duration > timing_monitor.config.warning_threshold_us) {
        if (timing_monitor.config.log_violations) {
            Logger_Log(LOG_LEVEL_WARNING, "TIMING", 
                      "%s timing warning: %u us (threshold: %u us)",
                      metric_names[metric], duration,
                      timing_monitor.config.warning_threshold_us);
        }
    }

    timing_monitor.measurements_active[metric] = false;

    // Limit the number of samples if configured
    if (timing_monitor.config.max_samples > 0 && 
        stats->total_samples >= timing_monitor.config.max_samples) {
        Timing_ResetStats(metric);
    }
    
    exit_critical();
}

void Timing_GetStats(TimingMetric metric, TimingStats* stats) {
    if (!timing_monitor.initialized || metric >= TIMING_COUNT || !stats) {
        return;
    }

    enter_critical();
    memcpy(stats, &timing_monitor.stats[metric], sizeof(TimingStats));
    exit_critical();
}

void Timing_ResetStats(TimingMetric metric) {
    if (!timing_monitor.initialized || metric >= TIMING_COUNT) {
        return;
    }

    enter_critical();
    TimingStats* stats = &timing_monitor.stats[metric];
    stats->min_time_us = UINT32_MAX;
    stats->max_time_us = 0;
    stats->avg_time_us = 0;
    stats->total_samples = 0;
    stats->violations = 0;
    exit_critical();
} 