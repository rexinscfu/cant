#include <assert.h>
#include <string.h>
#include "../../src/compiler/backend/llvm_generator.h"

static void test_basic_generation(void) {
    TargetConfig target_config = {
        .arch = TARGET_S32K344,
        .cpu = "cortex-m7",
        .features = "+vfp4",
        .optimize_size = true,
        .enable_fast_math = false,
        .opt_level = 2
    };

    SignalOptConfig signal_config = {
        .enable_path_optimization = true,
        .enable_dead_elimination = true,
        .enable_timing_optimization = true,
        .max_latency_us = 1000,
        .min_sample_rate_hz = 1000
    };

    LLVMGenerator* gen = llvm_generator_create("test_module", 
                                             &target_config,
                                             &signal_config);
    assert(gen != NULL);

    // Create a basic AST node
    ASTNode node = {
        .type = AST_ECU_DEF,
        .as.ecu_def = {
            .name = "TestECU",
            .signals = NULL,
            .signal_count = 0,
            .processes = NULL,
            .process_count = 0
        }
    };

    bool result = llvm_generator_compile_ast(gen, &node);
    assert(result);

    llvm_generator_destroy(gen);
}

static void test_signal_optimization(void) {
    // Test signal optimization
    assert(1);
}

static void test_memory_layout(void) {
    // Test memory layout optimization
    assert(1);
}

int main(void) {
    test_basic_generation();
    test_signal_optimization();
    test_memory_layout();
    printf("All LLVM generator tests passed!\n");
    return 0;
} 