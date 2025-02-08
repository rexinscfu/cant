#ifndef CANT_PEEPHOLE_H
#define CANT_PEEPHOLE_H

#include "../ir/ir_builder.h"
#include <stdbool.h>

// Peephole pattern types
typedef enum {
    PEEP_REDUNDANT_LOAD,
    PEEP_DEAD_STORE,
    PEEP_STRENGTH_REDUCTION,
    PEEP_CONSTANT_FOLDING,
    PEEP_IDENTITY_OP,
    PEEP_COMMON_SUBEXPR,
    PEEP_FRAME_COMBINE,
    PEEP_PATTERN_MERGE
} PeepholeType;

// Pattern matching context
typedef struct {
    IRBuilder* builder;
    Node* current;
    Node* next;
    bool modified;
} PeepholeContext;

// Pattern handlers
typedef bool (*PeepholeHandler)(PeepholeContext* ctx);

// Pattern definition
typedef struct {
    PeepholeType type;
    const char* description;
    PeepholeHandler handler;
    uint32_t window_size;
} PeepholePattern;

// Pattern matching and replacement
bool peephole_match_pattern(PeepholeContext* ctx, const PeepholePattern* pattern);
bool peephole_apply_pattern(PeepholeContext* ctx, const PeepholePattern* pattern);
bool peephole_optimize_block(IRBuilder* builder, Node* block);

// Built-in patterns
extern const PeepholePattern PEEPHOLE_PATTERNS[];
extern const size_t PEEPHOLE_PATTERN_COUNT;

// Pattern-specific optimizations
bool optimize_redundant_loads(PeepholeContext* ctx);
bool optimize_dead_stores(PeepholeContext* ctx);
bool optimize_strength_reduction(PeepholeContext* ctx);
bool optimize_constant_folding(PeepholeContext* ctx);
bool optimize_identity_operations(PeepholeContext* ctx);
bool optimize_common_subexpressions(PeepholeContext* ctx);
bool optimize_frame_combinations(PeepholeContext* ctx);
bool optimize_pattern_merging(PeepholeContext* ctx);

// Utility functions
bool is_redundant_load(Node* node);
bool is_dead_store(Node* node);
bool can_reduce_strength(Node* node);
bool can_fold_constants(Node* node);
bool is_identity_operation(Node* node);
bool are_expressions_equal(Node* a, Node* b);
bool can_combine_frames(Node* a, Node* b);
bool can_merge_patterns(Node* a, Node* b);

#endif // CANT_PEEPHOLE_H 