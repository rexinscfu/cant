#ifndef CANT_LLVM_GENERATOR_H
#define CANT_LLVM_GENERATOR_H

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include <llvm-c/Transforms/IPO.h>
#include "../middle/ast.h"
#include "../middle/types.h"

// Target architecture configurations
typedef enum {
    TARGET_S32K344,    // NXP S32K3 series
    TARGET_TDA4VM,     // TI Jacinto 7 series
    TARGET_MPC5748G,   // NXP MPC57 series
    TARGET_GENERIC     // Generic target
} TargetArch;

typedef struct {
    TargetArch arch;
    const char* cpu;
    const char* features;
    bool optimize_size;
    bool enable_fast_math;
    unsigned opt_level;
    struct {
        uint32_t stack_size;
        uint32_t heap_size;
        bool enable_fpu;
        bool enable_simd;
    } hw_config;
} TargetConfig;

// Signal optimization configuration
typedef struct {
    bool enable_path_optimization;
    bool enable_dead_elimination;
    bool enable_timing_optimization;
    uint32_t max_latency_us;
    uint32_t min_sample_rate_hz;
} SignalOptConfig;

typedef struct LLVMGenerator LLVMGenerator;

// Generator API
LLVMGenerator* llvm_generator_create(const char* module_name, 
                                   const TargetConfig* target_config,
                                   const SignalOptConfig* signal_config);
void llvm_generator_destroy(LLVMGenerator* generator);
bool llvm_generator_compile_ast(LLVMGenerator* generator, ASTNode* ast);
bool llvm_generator_optimize(LLVMGenerator* generator);
bool llvm_generator_write_ir(LLVMGenerator* generator, const char* filename);
bool llvm_generator_write_object(LLVMGenerator* generator, const char* filename);

#endif // CANT_LLVM_GENERATOR_H 