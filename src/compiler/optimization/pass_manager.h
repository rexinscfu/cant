#ifndef CANT_PASS_MANAGER_H
#define CANT_PASS_MANAGER_H

#include "../ir/ir_builder.h"
#include <stdbool.h>

// Optimization pass types
typedef enum {
    PASS_CFG_SIMPLIFY,
    PASS_DEAD_CODE,
    PASS_CONSTANT_FOLD,
    PASS_STRENGTH_REDUCE,
    PASS_LOOP_OPTIMIZE,
    PASS_VECTORIZE,
    PASS_PATTERN_MATCH,
    PASS_PEEPHOLE,
    PASS_TARGET_SPECIFIC
} PassType;

// Pass configuration
typedef struct {
    bool enable_simd;
    bool aggressive_opts;
    uint32_t opt_level;
    uint32_t size_level;
    uint32_t target_features;
} PassConfig;

// Pass descriptor
typedef struct {
    PassType type;
    const char* name;
    bool (*run)(IRBuilder* builder);
    bool (*initialize)(PassConfig* config);
    void (*finalize)(void);
    bool requires_cfg;
    bool modifies_cfg;
    PassType* dependencies;
    uint32_t dep_count;
} PassDescriptor;

// Pass manager
typedef struct PassManager PassManager;

// Pass manager API
PassManager* pass_manager_create(PassConfig* config);
void pass_manager_destroy(PassManager* pm);
bool pass_manager_add_pass(PassManager* pm, PassDescriptor* pass);
bool pass_manager_run_passes(PassManager* pm, IRBuilder* builder);

// Built-in passes
extern const PassDescriptor PASS_CFG_SIMPLIFICATION;
extern const PassDescriptor PASS_DEAD_CODE_ELIMINATION;
extern const PassDescriptor PASS_CONSTANT_FOLDING;
extern const PassDescriptor PASS_STRENGTH_REDUCTION;
extern const PassDescriptor PASS_LOOP_OPTIMIZATION;
extern const PassDescriptor PASS_VECTORIZATION;
extern const PassDescriptor PASS_PATTERN_MATCHING;
extern const PassDescriptor PASS_PEEPHOLE_OPTS;

// Pass utilities
bool pass_verify_ir(IRBuilder* builder);
void pass_dump_ir(IRBuilder* builder, const char* message);
bool pass_requires_cfg(PassType type);
bool pass_modifies_cfg(PassType type);

#endif // CANT_PASS_MANAGER_H 