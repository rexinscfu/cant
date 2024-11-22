#ifndef CANT_LLVM_OPTIMIZATIONS_H
#define CANT_LLVM_OPTIMIZATIONS_H

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Transforms/PassManager.h>

typedef struct {
    uint32_t min_latency_us;
    uint32_t max_latency_us;
    uint32_t sample_rate_hz;
    bool is_critical;
} SignalTimingInfo;

// Custom optimization pass registration
void register_signal_optimization_passes(LLVMPassManagerRef pm);
void register_memory_layout_passes(LLVMPassManagerRef pm);
void register_automotive_passes(LLVMPassManagerRef pm);

// Signal path analysis
bool analyze_signal_timing(LLVMValueRef signal_func, SignalTimingInfo* timing);
bool optimize_signal_chain(LLVMValueRef signal_func);
bool eliminate_dead_signals(LLVMModuleRef module);

#endif // CANT_LLVM_OPTIMIZATIONS_H 