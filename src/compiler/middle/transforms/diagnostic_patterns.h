#ifndef CANT_DIAGNOSTIC_PATTERNS_H
#define CANT_DIAGNOSTIC_PATTERNS_H

#include "../../frontend/parser.h"
#include "../ir/ir_builder.h"

// Pattern transformation context
typedef struct {
    IRBuilder* builder;
    bool optimize_size;
    bool enable_simd;
    uint32_t target_features;
} PatternContext;

// Pattern transformation passes
bool transform_request_patterns(PatternContext* ctx, Node* service_node);
bool transform_response_patterns(PatternContext* ctx, Node* service_node);
bool transform_flow_patterns(PatternContext* ctx, Node* service_node);
bool transform_security_patterns(PatternContext* ctx, Node* service_node);

// Pattern optimization strategies
bool optimize_pattern_matching(PatternContext* ctx, Node* pattern_node);
bool optimize_data_extraction(PatternContext* ctx, Node* pattern_node);
bool optimize_frame_filtering(PatternContext* ctx, Node* pattern_node);
bool optimize_flow_control(PatternContext* ctx, Node* pattern_node);

// Pattern analysis
typedef struct {
    uint32_t static_prefix_len;
    uint32_t wildcard_positions;
    uint32_t mask_coverage;
    bool has_fixed_length;
    bool is_contiguous;
} PatternAnalysis;

PatternAnalysis analyze_pattern(Node* pattern_node);
bool can_vectorize_pattern(const PatternAnalysis* analysis);
bool should_use_lookup_table(const PatternAnalysis* analysis);

// Pattern transformation utilities
Node* create_optimized_matcher(PatternContext* ctx, Node* pattern_node);
Node* create_simd_matcher(PatternContext* ctx, Node* pattern_node);
Node* create_lookup_matcher(PatternContext* ctx, Node* pattern_node);
Node* create_binary_matcher(PatternContext* ctx, Node* pattern_node);

#endif // CANT_DIAGNOSTIC_PATTERNS_H 