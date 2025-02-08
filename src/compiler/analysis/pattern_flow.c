#include "pattern_flow.h"
#include "data_flow.h"
#include <string.h>

typedef struct {
    uint32_t id;
    uint32_t freq;
    uint32_t deps;
    bool is_reachable;
} PatternFlowInfo;

static void analyze_pattern_flow(DataFlowContext* ctx, Node* pattern) {
    if (!ctx || !pattern) return;
    
    // Initialize pattern flow info
    PatternFlowInfo info = {0};
    info.id = get_pattern_id(pattern);
    info.is_reachable = true;
    
    // Analyze pattern dependencies
    NodeList* deps = get_pattern_deps(pattern);
    while (deps) {
        info.deps++;
        if (!is_pattern_reachable(ctx, deps->node)) {
            info.is_reachable = false;
        }
        deps = deps->next;
    }
    
    // Update flow info
    set_pattern_flow_info(ctx, pattern, &info);
}

bool analyze_pattern_reachability(DataFlowContext* ctx, Node* pattern) {
    if (!ctx || !pattern) return false;
    
    bool changed = false;
    NodeList* current = get_pattern_list(pattern);
    
    while (current) {
        PatternFlowInfo info;
        get_pattern_flow_info(ctx, current->node, &info);
        
        bool was_reachable = info.is_reachable;
        analyze_pattern_flow(ctx, current->node);
        get_pattern_flow_info(ctx, current->node, &info);
        
        if (was_reachable != info.is_reachable) {
            changed = true;
        }
        
        current = current->next;
    }
    
    return changed;
}

bool can_merge_pattern_flow(DataFlowContext* ctx, Node* a, Node* b) {
    if (!ctx || !a || !b) return false;
    
    PatternFlowInfo info_a, info_b;
    get_pattern_flow_info(ctx, a, &info_a);
    get_pattern_flow_info(ctx, b, &info_b);
    
    // Check if patterns can be merged based on flow analysis
    if (!info_a.is_reachable || !info_b.is_reachable) {
        return false;
    }
    
    // Check dependency compatibility
    NodeList* deps_a = get_pattern_deps(a);
    NodeList* deps_b = get_pattern_deps(b);
    
    while (deps_a && deps_b) {
        if (!are_deps_compatible(ctx, deps_a->node, deps_b->node)) {
            return false;
        }
        deps_a = deps_a->next;
        deps_b = deps_b->next;
    }
    
    return true;
}

static bool are_deps_compatible(DataFlowContext* ctx, Node* a, Node* b) {
    if (!ctx || !a || !b) return false;
    
    // Check if dependencies have compatible flow patterns
    PatternFlowInfo info_a, info_b;
    get_pattern_flow_info(ctx, a, &info_a);
    get_pattern_flow_info(ctx, b, &info_b);
    
    // Dependencies are compatible if:
    // 1. Both are reachable
    // 2. Have similar frequency characteristics
    // 3. Have compatible dependency chains
    return info_a.is_reachable && info_b.is_reachable &&
           abs((int)info_a.freq - (int)info_b.freq) <= 5 &&
           info_a.deps == info_b.deps;
}

static uint32_t get_pattern_id(Node* pattern) {
    static uint32_t next_id = 1;
    if (!pattern) return 0;
    
    // Generate unique pattern ID if not already assigned
    if (!pattern->id) {
        pattern->id = next_id++;
    }
    return pattern->id;
}

static NodeList* get_pattern_deps(Node* pattern) {
    if (!pattern) return NULL;
    
    switch (pattern->kind) {
        case NODE_FRAME_PATTERN:
            return pattern->as.frame_pattern.conditions;
            
        case NODE_DIAG_PATTERN:
            return pattern->as.diag_pattern.conditions;
            
        default:
            return NULL;
    }
}

static bool is_pattern_reachable(DataFlowContext* ctx, Node* pattern) {
    if (!ctx || !pattern) return false;
    
    PatternFlowInfo info;
    get_pattern_flow_info(ctx, pattern, &info);
    return info.is_reachable;
}

static void set_pattern_flow_info(DataFlowContext* ctx, Node* pattern, PatternFlowInfo* info) {
    if (!ctx || !pattern || !info) return;
    
    // Store flow info in context
    uint32_t id = get_pattern_id(pattern);
    uint32_t index = id % ctx->result.size;
    
    memcpy(&ctx->result.data[index], info, sizeof(PatternFlowInfo));
}

static void get_pattern_flow_info(DataFlowContext* ctx, Node* pattern, PatternFlowInfo* info) {
    if (!ctx || !pattern || !info) return;
    
    // Retrieve flow info from context
    uint32_t id = get_pattern_id(pattern);
    uint32_t index = id % ctx->result.size;
    
    memcpy(info, &ctx->result.data[index], sizeof(PatternFlowInfo));
} 