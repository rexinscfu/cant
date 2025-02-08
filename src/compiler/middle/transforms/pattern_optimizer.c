#include "pattern_optimizer.h"
#include "diagnostic_patterns.h"
#include "../analysis/data_flow.h"
#include <string.h>

typedef struct {
    Node* pattern;
    uint32_t freq;
    uint32_t size;
    bool is_simd;
} PatternInfo;

static bool should_optimize_pattern(PatternInfo* info) {
    // Patterns worth optimizing should be:
    // 1. Frequently used (high freq)
    // 2. Large enough to benefit from SIMD
    // 3. Have good data locality
    return info->freq > 10 && info->size >= 16;
}

static void collect_pattern_stats(Node* node, PatternInfo* info) {
    if (!node || !info) return;
    
    switch (node->kind) {
        case NODE_FRAME_PATTERN:
            info->freq++;
            info->size = node->as.frame_pattern.data_length;
            break;
            
        case NODE_DIAG_PATTERN:
            info->freq += 2; // Diagnostic patterns are more critical
            info->size = node->as.diag_pattern.length;
            break;
            
        default:
            break;
    }
}

bool optimize_pattern_group(PatternContext* ctx, NodeList* patterns) {
    if (!ctx || !patterns) return false;
    
    PatternInfo info = {0};
    NodeList* current = patterns;
    
    // Collect pattern statistics
    while (current) {
        collect_pattern_stats(current->node, &info);
        current = current->next;
    }
    
    // Decide optimization strategy
    if (should_optimize_pattern(&info)) {
        if (ctx->enable_simd && info.size >= 16) {
            // Use SIMD optimization
            current = patterns;
            while (current) {
                Node* optimized = create_simd_matcher(ctx, current->node);
                if (optimized) {
                    current->node = optimized;
                }
                current = current->next;
            }
            return true;
        } else if (info.size <= 8) {
            // Use lookup table for small patterns
            current = patterns;
            while (current) {
                Node* optimized = create_lookup_matcher(ctx, current->node);
                if (optimized) {
                    current->node = optimized;
                }
                current = current->next;
            }
            return true;
        }
    }
    
    return false;
}

bool merge_similar_patterns(PatternContext* ctx, NodeList* patterns) {
    if (!ctx || !patterns) return false;
    
    bool modified = false;
    NodeList* current = patterns;
    
    while (current && current->next) {
        NodeList* next = current->next;
        
        // Check if patterns can be merged
        if (can_merge_patterns(current->node, next->node)) {
            // Create merged pattern
            Node* merged = create_merged_pattern(ctx, current->node, next->node);
            if (merged) {
                current->node = merged;
                current->next = next->next;
                ir_destroy_node(next->node);
                free(next);
                modified = true;
            }
        }
        
        current = current->next;
    }
    
    return modified;
}

static bool can_merge_patterns(Node* a, Node* b) {
    if (!a || !b || a->kind != b->kind) return false;
    
    // Check if patterns have similar structure
    if (a->kind == NODE_FRAME_PATTERN) {
        return a->as.frame_pattern.data_length == b->as.frame_pattern.data_length &&
               memcmp(a->as.frame_pattern.mask, b->as.frame_pattern.mask,
                     a->as.frame_pattern.data_length) == 0;
    }
    
    return false;
}

static Node* create_merged_pattern(PatternContext* ctx, Node* a, Node* b) {
    if (!ctx || !a || !b) return NULL;
    
    // Create new pattern node
    Node* merged = ir_create_node(ctx->builder, a->kind);
    if (!merged) return NULL;
    
    // Merge pattern data
    if (a->kind == NODE_FRAME_PATTERN) {
        uint8_t length = a->as.frame_pattern.data_length;
        uint8_t* data = malloc(length);
        uint8_t* mask = malloc(length);
        
        if (!data || !mask) {
            free(data);
            free(mask);
            ir_destroy_node(merged);
            return NULL;
        }
        
        // Combine pattern data and masks
        for (uint8_t i = 0; i < length; i++) {
            data[i] = a->as.frame_pattern.data[i] | b->as.frame_pattern.data[i];
            mask[i] = a->as.frame_pattern.mask[i] & b->as.frame_pattern.mask[i];
        }
        
        merged->as.frame_pattern.data = data;
        merged->as.frame_pattern.mask = mask;
        merged->as.frame_pattern.data_length = length;
        
        // Combine handlers
        NodeList* handlers = ir_create_node_list(a->as.frame_pattern.handler);
        ir_append_node(handlers, b->as.frame_pattern.handler);
        merged->as.frame_pattern.handlers = handlers;
    }
    
    return merged;
} 