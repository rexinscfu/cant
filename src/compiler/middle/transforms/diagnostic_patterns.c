#include "diagnostic_patterns.h"
#include "../ir/ir_builder.h"
#include "../analysis/data_flow.h"
#include <string.h>

// Pattern analysis implementation
PatternAnalysis analyze_pattern(Node* pattern_node) {
    PatternAnalysis analysis = {0};
    
    if (!pattern_node || pattern_node->kind != NODE_DIAG_PATTERN) {
        return analysis;
    }
    
    const uint8_t* data = pattern_node->as.frame_data.data;
    const uint8_t* mask = pattern_node->as.frame_data.mask;
    uint8_t length = pattern_node->as.frame_data.length;
    
    // Analyze static prefix
    uint32_t prefix_len = 0;
    while (prefix_len < length && mask[prefix_len] == 0xFF) {
        prefix_len++;
    }
    analysis.static_prefix_len = prefix_len;
    
    // Count wildcards and analyze mask coverage
    uint32_t wildcards = 0;
    uint32_t mask_bits = 0;
    bool contiguous = true;
    bool in_wildcard = false;
    
    for (uint32_t i = 0; i < length; i++) {
        if (mask[i] == 0x00) {
            wildcards++;
            if (!in_wildcard && i > 0) {
                contiguous = false;
            }
            in_wildcard = true;
        } else {
            if (in_wildcard) {
                contiguous = false;
            }
            in_wildcard = false;
            mask_bits += __builtin_popcount(mask[i]);
        }
    }
    
    analysis.wildcard_positions = wildcards;
    analysis.mask_coverage = mask_bits;
    analysis.has_fixed_length = true;
    analysis.is_contiguous = contiguous;
    
    return analysis;
}

bool can_vectorize_pattern(const PatternAnalysis* analysis) {
    if (!analysis) return false;
    
    // Patterns suitable for vectorization should have:
    // 1. High mask coverage (>= 75% of bits)
    // 2. Few wildcards (< 25% of length)
    // 3. Fixed length
    uint32_t total_bits = analysis->mask_coverage + (analysis->wildcard_positions * 8);
    return analysis->has_fixed_length &&
           analysis->mask_coverage >= (total_bits * 3 / 4) &&
           analysis->wildcard_positions < (total_bits / 32);
}

bool should_use_lookup_table(const PatternAnalysis* analysis) {
    if (!analysis) return false;
    
    // Use lookup tables for patterns with:
    // 1. Small static prefix (≤ 4 bytes)
    // 2. High wildcard count
    // 3. Non-contiguous structure
    return analysis->static_prefix_len <= 4 &&
           analysis->wildcard_positions > 2 &&
           !analysis->is_contiguous;
}

// Pattern transformation implementation
bool transform_request_patterns(PatternContext* ctx, Node* service_node) {
    if (!ctx || !service_node) return false;
    
    NodeList* patterns = service_node->as.diag_service.config.patterns;
    bool success = true;
    bool use_simd = ctx->enable_simd && (ctx->target_features & TARGET_FEATURE_SIMD);
    
    while (patterns && success) {
        PatternAnalysis analysis = analyze_pattern(patterns->node);
        Node* optimized = NULL;
        
        if (use_simd && can_vectorize_pattern(&analysis)) {
            // Use SIMD-optimized matcher
            optimized = create_simd_matcher(ctx, patterns->node);
        } else if (should_use_lookup_table(&analysis)) {
            // Use lookup table for complex patterns
            optimized = create_lookup_matcher(ctx, patterns->node);
        } else {
            // Fallback to binary matcher
            optimized = create_binary_matcher(ctx, patterns->node);
        }
        
        if (!optimized) {
            success = false;
            break;
        }
        
        // Replace original pattern with optimized version
        patterns->node = optimized;
        patterns = patterns->next;
    }
    
    return success;
}

bool optimize_pattern_matching(PatternContext* ctx, Node* pattern_node) {
    if (!ctx || !pattern_node) return false;
    
    PatternAnalysis analysis = analyze_pattern(pattern_node);
    bool modified = false;
    
    // Try SIMD optimization first
    if (ctx->enable_simd && can_vectorize_pattern(&analysis)) {
        Node* simd_matcher = create_simd_matcher(ctx, pattern_node);
        if (simd_matcher) {
            // Replace with SIMD version
            memcpy(pattern_node, simd_matcher, sizeof(Node));
            ir_destroy_node(simd_matcher);
            modified = true;
        }
    }
    
    // Apply additional optimizations
    if (!modified) {
        if (should_use_lookup_table(&analysis)) {
            Node* lookup_matcher = create_lookup_matcher(ctx, pattern_node);
            if (lookup_matcher) {
                memcpy(pattern_node, lookup_matcher, sizeof(Node));
                ir_destroy_node(lookup_matcher);
                modified = true;
            }
        }
    }
    
    return modified;
}

// Pattern matcher creation
Node* create_optimized_matcher(PatternContext* ctx, Node* pattern_node) {
    PatternAnalysis analysis = analyze_pattern(pattern_node);
    
    if (can_vectorize_pattern(&analysis) && ctx->enable_simd) {
        return create_simd_matcher(ctx, pattern_node);
    } else if (should_use_lookup_table(&analysis)) {
        return create_lookup_matcher(ctx, pattern_node);
    } else {
        return create_binary_matcher(ctx, pattern_node);
    }
}

Node* create_simd_matcher(PatternContext* ctx, Node* pattern_node) {
    if (!ctx || !pattern_node) return NULL;
    
    // Create SIMD-based pattern matcher
    Node* matcher = ir_create_node(ctx->builder, NODE_FRAME_PATTERN);
    
    // Extract pattern data and mask
    const uint8_t* data = pattern_node->as.frame_data.data;
    const uint8_t* mask = pattern_node->as.frame_data.mask;
    uint8_t length = pattern_node->as.frame_data.length;
    
    // Align data for SIMD operations (16-byte alignment)
    uint8_t aligned_data[16] = {0};
    uint8_t aligned_mask[16] = {0};
    memcpy(aligned_data, data, length < 16 ? length : 16);
    memcpy(aligned_mask, mask, length < 16 ? length : 16);
    
    // Create SIMD vector loads
    Node* data_vec = ir_create_simd_load(ctx->builder, aligned_data);
    Node* mask_vec = ir_create_simd_load(ctx->builder, aligned_mask);
    
    // Create vectorized comparison
    Node* compare = ir_create_simd_compare(ctx->builder, data_vec, mask_vec);
    
    // Add pattern conditions
    matcher->as.frame_pattern.conditions = ir_create_node_list(compare);
    matcher->as.frame_pattern.handler = pattern_node->as.frame_pattern.handler;
    
    return matcher;
}

Node* create_lookup_matcher(PatternContext* ctx, Node* pattern_node) {
    // Create lookup table-based pattern matcher
    Node* matcher = ir_create_node(ctx->builder, NODE_FRAME_PATTERN);
    
    // Generate lookup table
    uint32_t table_size = 1 << (pattern_node->as.frame_data.length * 8);
    uint8_t* lookup_table = calloc(table_size, 1);
    
    // Populate lookup table based on pattern
    for (uint32_t i = 0; i < table_size; i++) {
        if (matches_pattern(i, pattern_node->as.frame_data.data,
                          pattern_node->as.frame_data.mask,
                          pattern_node->as.frame_data.length)) {
            lookup_table[i] = 1;
        }
    }
    
    // Create lookup table access
    Node* table_load = ir_create_lookup(ctx->builder, lookup_table, table_size);
    matcher->as.frame_pattern.conditions = ir_create_node_list(table_load);
    matcher->as.frame_pattern.handler = pattern_node->as.frame_pattern.handler;
    
    free(lookup_table);
    return matcher;
}

Node* create_binary_matcher(PatternContext* ctx, Node* pattern_node) {
    // Create binary comparison-based pattern matcher
    Node* matcher = ir_create_node(ctx->builder, NODE_FRAME_PATTERN);
    NodeList* conditions = NULL;
    
    // Create byte-by-byte comparisons
    for (uint32_t i = 0; i < pattern_node->as.frame_data.length; i++) {
        if (pattern_node->as.frame_data.mask[i] != 0x00) {
            Node* byte_compare = ir_create_binary_op(ctx->builder,
                ir_create_byte_extract(ctx->builder, i),
                ir_create_constant(ctx->builder, pattern_node->as.frame_data.data[i]),
                IR_OP_EQ
            );
            conditions = ir_append_node(conditions, byte_compare);
        }
    }
    
    matcher->as.frame_pattern.conditions = conditions;
    matcher->as.frame_pattern.handler = pattern_node->as.frame_pattern.handler;
    
    return matcher;
}

// Helper function for lookup table generation
static bool matches_pattern(uint32_t value, const uint8_t* pattern,
                          const uint8_t* mask, uint8_t length) {
    for (uint8_t i = 0; i < length; i++) {
        uint8_t byte = (value >> (i * 8)) & 0xFF;
        if ((byte & mask[i]) != (pattern[i] & mask[i])) {
            return false;
        }
    }
    return true;
}

// Helper function for SIMD capability check
static bool has_simd_support(uint32_t target_features) {
    return (target_features & TARGET_FEATURE_SIMD) != 0;
}

// Helper function for pattern analysis
static bool is_pattern_simd_friendly(const PatternAnalysis* analysis) {
    // Patterns suitable for SIMD should have:
    // 1. Fixed length data (no variable length patterns)
    // 2. High mask coverage (few wildcards)
    // 3. Aligned access patterns
    return analysis->has_fixed_length &&
           analysis->mask_coverage >= (analysis->static_prefix_len * 8 * 3 / 4) &&
           (analysis->static_prefix_len % 16) == 0;
} 