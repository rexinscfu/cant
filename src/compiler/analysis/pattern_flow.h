#ifndef CANT_PATTERN_FLOW_H
#define CANT_PATTERN_FLOW_H

#include "data_flow.h"
#include "../ir/ir_builder.h"

// Pattern flow analysis functions
bool analyze_pattern_reachability(DataFlowContext* ctx, Node* pattern);
bool can_merge_pattern_flow(DataFlowContext* ctx, Node* a, Node* b);

// Pattern flow utilities
static uint32_t get_pattern_id(Node* pattern);
static NodeList* get_pattern_deps(Node* pattern);
static bool is_pattern_reachable(DataFlowContext* ctx, Node* pattern);

#endif // CANT_PATTERN_FLOW_H 