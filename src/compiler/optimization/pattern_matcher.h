#ifndef CANT_PATTERN_MATCHER_H
#define CANT_PATTERN_MATCHER_H

#include "../ir/ir_builder.h"
#include <stdbool.h>

// Pattern types
typedef enum {
    PATTERN_ARITHMETIC,
    PATTERN_MEMORY,
    PATTERN_CONTROL,
    PATTERN_VECTOR,
    PATTERN_TARGET
} PatternType;

// Pattern match result
typedef struct {
    bool matched;
    Node** captures;
    uint32_t capture_count;
    float benefit;
} MatchResult;

// Pattern definition
typedef struct {
    PatternType type;
    const char* name;
    Node* pattern;
    Node* replacement;
    float benefit;
    bool (*predicate)(Node* node);
    bool (*verify)(MatchResult* result);
} Pattern;

// Pattern matcher context
typedef struct {
    IRBuilder* builder;
    Pattern** patterns;
    uint32_t pattern_count;
    bool enable_target_patterns;
    float min_benefit;
} MatchContext;

// Pattern matcher API
MatchContext* matcher_create(IRBuilder* builder);
void matcher_destroy(MatchContext* ctx);
bool matcher_add_pattern(MatchContext* ctx, Pattern* pattern);
MatchResult* matcher_match_node(MatchContext* ctx, Node* node);
bool matcher_apply_match(MatchContext* ctx, Node* node, MatchResult* result);

// Pattern utilities
Pattern* create_pattern(PatternType type, const char* name, Node* pattern, Node* replacement);
void destroy_pattern(Pattern* pattern);
bool verify_pattern(Pattern* pattern);
float compute_pattern_benefit(Pattern* pattern);

// Built-in patterns
extern const Pattern PATTERN_STRENGTH_REDUCTION[];
extern const Pattern PATTERN_IDIOM_RECOGNITION[];
extern const Pattern PATTERN_VECTOR_IDIOMS[];
extern const Pattern PATTERN_TARGET_SPECIFIC[];

// Pattern matching utilities
bool patterns_are_equivalent(Node* a, Node* b);
Node* clone_pattern(Node* pattern);
void print_pattern(Pattern* pattern);
void dump_match_result(MatchResult* result);

#endif // CANT_PATTERN_MATCHER_H 