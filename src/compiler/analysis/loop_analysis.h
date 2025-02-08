#ifndef CANT_LOOP_ANALYSIS_H
#define CANT_LOOP_ANALYSIS_H

#include "cfg.h"
#include "dominators.h"
#include <stdbool.h>

// Loop types
typedef enum {
    LOOP_NATURAL,
    LOOP_IRREDUCIBLE,
    LOOP_MULTI_ENTRY
} LoopType;

// Loop structure
typedef struct Loop {
    LoopType type;
    CFGNode* header;
    CFGNode* latch;
    CFGNode** body;
    uint32_t body_size;
    struct Loop* parent;
    struct Loop** children;
    uint32_t child_count;
    uint32_t depth;
    uint32_t trip_count;
    bool is_reducible;
} Loop;

// Loop tree
typedef struct {
    Loop* root;
    Loop** loops;
    uint32_t loop_count;
    DomTree* dom_tree;
} LoopTree;

// Loop analysis functions
LoopTree* loop_create_tree(CFG* cfg, DomTree* dom_tree);
void loop_destroy_tree(LoopTree* tree);
bool loop_identify_loops(LoopTree* tree);
bool loop_analyze_bounds(LoopTree* tree);

// Loop queries
bool loop_is_innermost(Loop* loop);
bool loop_contains_node(Loop* loop, CFGNode* node);
Loop* loop_find_common_loop(LoopTree* tree, CFGNode* a, CFGNode* b);
uint32_t loop_nesting_depth(Loop* loop);

// Loop transformations
bool loop_can_unroll(Loop* loop);
bool loop_can_vectorize(Loop* loop);
bool loop_has_carried_dependencies(Loop* loop);
bool loop_is_parallel(Loop* loop);

// Loop optimization helpers
bool loop_hoist_invariants(Loop* loop);
bool loop_unroll(Loop* loop, uint32_t factor);
bool loop_vectorize(Loop* loop);
bool loop_distribute(Loop* loop);

// Utility functions
Loop* loop_get_innermost_containing(LoopTree* tree, CFGNode* node);
void loop_print_tree(LoopTree* tree);
bool loop_verify_tree(LoopTree* tree);

#endif // CANT_LOOP_ANALYSIS_H 