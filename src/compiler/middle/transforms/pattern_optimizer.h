#ifndef CANT_PATTERN_OPTIMIZER_H
#define CANT_PATTERN_OPTIMIZER_H

#include "diagnostic_patterns.h"
#include "../ir/ir_builder.h"

// Pattern optimization functions
bool optimize_pattern_group(PatternContext* ctx, NodeList* patterns);
bool merge_similar_patterns(PatternContext* ctx, NodeList* patterns);

// Pattern analysis helpers
bool can_optimize_pattern(Node* pattern);
bool can_merge_patterns(Node* a, Node* b);

#endif // CANT_PATTERN_OPTIMIZER_H 