#ifndef CANT_SIGNAL_ANALYZER_H
#define CANT_SIGNAL_ANALYZER_H

#include <stdint.h>

typedef struct {
    double min_value;
    double max_value;
    double average;
    double std_dev;
    uint32_t sample_count;
} SignalStats;

// Development utilities for signal analysis
SignalStats analyze_signal_trace(const double* values, size_t count);
void print_signal_report(const char* signal_name, const SignalStats* stats);
void export_signal_csv(const char* filename, const double* values, size_t count);

#endif // CANT_SIGNAL_ANALYZER_H 