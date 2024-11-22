#include "signal_analyzer.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

SignalStats analyze_signal_trace(const double* values, size_t count) {
    SignalStats stats = {0};
    if (!values || count == 0) return stats;

    // First pass: min, max, average
    stats.min_value = values[0];
    stats.max_value = values[0];
    double sum = 0.0;

    for (size_t i = 0; i < count; i++) {
        if (values[i] < stats.min_value) stats.min_value = values[i];
        if (values[i] > stats.max_value) stats.max_value = values[i];
        sum += values[i];
    }

    stats.average = sum / count;
    stats.sample_count = count;

    // Second pass: standard deviation
    double sum_sq_diff = 0.0;
    for (size_t i = 0; i < count; i++) {
        double diff = values[i] - stats.average;
        sum_sq_diff += diff * diff;
    }

    stats.std_dev = sqrt(sum_sq_diff / count);
    return stats;
}

void print_signal_report(const char* signal_name, const SignalStats* stats) {
    printf("Signal Analysis Report: %s\n", signal_name);
    printf("===================================\n");
    printf("Sample Count: %u\n", stats->sample_count);
    printf("Minimum Value: %.3f\n", stats->min_value);
    printf("Maximum Value: %.3f\n", stats->max_value);
    printf("Average: %.3f\n", stats->average);
    printf("Standard Deviation: %.3f\n", stats->std_dev);
    printf("===================================\n");
}

void export_signal_csv(const char* filename, const double* values, size_t count) {
    FILE* file = fopen(filename, "w");
    if (!file) return;

    fprintf(file, "Sample,Value\n");
    for (size_t i = 0; i < count; i++) {
        fprintf(file, "%zu,%.6f\n", i, values[i]);
    }

    fclose(file);
} 