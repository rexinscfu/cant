#ifndef CANT_CFG_H
#define CANT_CFG_H

#include "../ir/ir_builder.h"
#include <stdbool.h>

// CFG node types
typedef enum {
    CFG_ENTRY,
    CFG_EXIT,
    CFG_BASIC_BLOCK,
    CFG_BRANCH,
    CFG_LOOP_HEADER,
    CFG_LOOP_TAIL
} CFGNodeType;

// CFG node structure
typedef struct CFGNode {
    CFGNodeType type;
    Node* ir_node;
    struct CFGNode** predecessors;
    struct CFGNode** successors;
    uint32_t pred_count;
    uint32_t succ_count;
    uint32_t id;
    bool visited;
} CFGNode;

// CFG structure
typedef struct {
    CFGNode* entry;
    CFGNode* exit;
    CFGNode** nodes;
    uint32_t node_count;
    uint32_t capacity;
} CFG;

// CFG construction and manipulation
CFG* cfg_create(void);
void cfg_destroy(CFG* cfg);
CFGNode* cfg_add_node(CFG* cfg, CFGNodeType type, Node* ir_node);
bool cfg_add_edge(CFGNode* from, CFGNode* to);
bool cfg_remove_edge(CFGNode* from, CFGNode* to);

// CFG analysis
bool cfg_compute_dominators(CFG* cfg);
bool cfg_identify_loops(CFG* cfg);
bool cfg_is_reducible(CFG* cfg);
uint32_t cfg_compute_depth(CFGNode* node);

// CFG optimization
bool cfg_eliminate_dead_code(CFG* cfg);
bool cfg_merge_blocks(CFG* cfg);
bool cfg_hoist_invariants(CFG* cfg);
bool cfg_unroll_loops(CFG* cfg, uint32_t max_unroll);

// CFG utilities
CFGNode* cfg_get_immediate_dominator(CFGNode* node);
bool cfg_dominates(CFGNode* a, CFGNode* b);
bool cfg_is_back_edge(CFGNode* from, CFGNode* to);
bool cfg_is_loop_header(CFGNode* node);

#endif // CANT_CFG_H 