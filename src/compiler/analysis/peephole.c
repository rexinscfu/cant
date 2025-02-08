#include "peephole.h"
#include "../ir/ir_builder.h"
#include <string.h>

// Built-in peephole patterns
const PeepholePattern PEEPHOLE_PATTERNS[] = {
    {
        .type = PEEP_REDUNDANT_LOAD,
        .description = "Eliminate redundant load operations",
        .handler = optimize_redundant_loads,
        .window_size = 2
    },
    {
        .type = PEEP_DEAD_STORE,
        .description = "Remove dead store operations",
        .handler = optimize_dead_stores,
        .window_size = 2
    },
    {
        .type = PEEP_STRENGTH_REDUCTION,
        .description = "Reduce operation strength",
        .handler = optimize_strength_reduction,
        .window_size = 1
    },
    {
        .type = PEEP_CONSTANT_FOLDING,
        .description = "Fold constant expressions",
        .handler = optimize_constant_folding,
        .window_size = 1
    },
    {
        .type = PEEP_IDENTITY_OP,
        .description = "Remove identity operations",
        .handler = optimize_identity_operations,
        .window_size = 1
    },
    {
        .type = PEEP_COMMON_SUBEXPR,
        .description = "Eliminate common subexpressions",
        .handler = optimize_common_subexpressions,
        .window_size = 3
    },
    {
        .type = PEEP_FRAME_COMBINE,
        .description = "Combine consecutive frame operations",
        .handler = optimize_frame_combinations,
        .window_size = 2
    },
    {
        .type = PEEP_PATTERN_MERGE,
        .description = "Merge compatible patterns",
        .handler = optimize_pattern_merging,
        .window_size = 2
    }
};

const size_t PEEPHOLE_PATTERN_COUNT = sizeof(PEEPHOLE_PATTERNS) / sizeof(PEEPHOLE_PATTERNS[0]);

// Pattern matching and replacement
bool peephole_match_pattern(PeepholeContext* ctx, const PeepholePattern* pattern) {
    if (!ctx || !pattern || !ctx->current) return false;
    
    switch (pattern->type) {
        case PEEP_REDUNDANT_LOAD:
            return is_redundant_load(ctx->current);
            
        case PEEP_DEAD_STORE:
            return is_dead_store(ctx->current);
            
        case PEEP_STRENGTH_REDUCTION:
            return can_reduce_strength(ctx->current);
            
        case PEEP_CONSTANT_FOLDING:
            return can_fold_constants(ctx->current);
            
        case PEEP_IDENTITY_OP:
            return is_identity_operation(ctx->current);
            
        case PEEP_COMMON_SUBEXPR:
            return ctx->next && are_expressions_equal(ctx->current, ctx->next);
            
        case PEEP_FRAME_COMBINE:
            return ctx->next && can_combine_frames(ctx->current, ctx->next);
            
        case PEEP_PATTERN_MERGE:
            return ctx->next && can_merge_patterns(ctx->current, ctx->next);
            
        default:
            return false;
    }
}

bool peephole_optimize_block(IRBuilder* builder, Node* block) {
    if (!builder || !block) return false;
    
    bool modified = false;
    PeepholeContext ctx = {
        .builder = builder,
        .current = block,
        .next = NULL,
        .modified = false
    };
    
    // Apply patterns until no more optimizations are possible
    bool changed;
    do {
        changed = false;
        ctx.current = block;
        
        while (ctx.current) {
            ctx.next = ctx.current->next;
            
            // Try each pattern
            for (size_t i = 0; i < PEEPHOLE_PATTERN_COUNT; i++) {
                if (peephole_match_pattern(&ctx, &PEEPHOLE_PATTERNS[i])) {
                    if (PEEPHOLE_PATTERNS[i].handler(&ctx)) {
                        changed = true;
                        modified = true;
                        break;
                    }
                }
            }
            
            ctx.current = ctx.next;
        }
    } while (changed);
    
    return modified;
}

// Pattern-specific optimizations
bool optimize_redundant_loads(PeepholeContext* ctx) {
    if (!ctx || !ctx->current || !ctx->next) return false;
    
    // Check if current node is a load followed by another load of same location
    if (ctx->current->kind == NODE_LOAD && ctx->next->kind == NODE_LOAD) {
        if (are_expressions_equal(ctx->current->as.load.address, 
                                ctx->next->as.load.address)) {
            // Replace second load with reference to first
            ctx->next = ctx->current;
            return true;
        }
    }
    
    return false;
}

bool optimize_dead_stores(PeepholeContext* ctx) {
    if (!ctx || !ctx->current) return false;
    
    // Check if store is immediately overwritten or never read
    if (ctx->current->kind == NODE_STORE) {
        Node* next = ctx->next;
        while (next) {
            if (next->kind == NODE_STORE &&
                are_expressions_equal(ctx->current->as.store.address,
                                   next->as.store.address)) {
                // Store is overwritten without being read
                ctx->current = next;
                return true;
            }
            if (next->kind == NODE_LOAD &&
                are_expressions_equal(ctx->current->as.store.address,
                                   next->as.load.address)) {
                // Store is read, keep it
                return false;
            }
            next = next->next;
        }
    }
    
    return false;
}

bool optimize_strength_reduction(PeepholeContext* ctx) {
    if (!ctx || !ctx->current) return false;
    
    // Replace expensive operations with cheaper equivalents
    if (ctx->current->kind == NODE_BINARY_EXPR) {
        switch (ctx->current->as.binary_expr.op) {
            case IR_OP_MUL:
                // Replace multiplication by power of 2 with shift
                if (is_power_of_two(ctx->current->as.binary_expr.right)) {
                    uint32_t shift = count_trailing_zeros(
                        ctx->current->as.binary_expr.right->as.int_value);
                    ctx->current->as.binary_expr.op = IR_OP_SHL;
                    ctx->current->as.binary_expr.right = 
                        ir_create_constant(ctx->builder, shift);
                    return true;
                }
                break;
                
            case IR_OP_DIV:
                // Replace division by power of 2 with shift
                if (is_power_of_two(ctx->current->as.binary_expr.right)) {
                    uint32_t shift = count_trailing_zeros(
                        ctx->current->as.binary_expr.right->as.int_value);
                    ctx->current->as.binary_expr.op = IR_OP_SHR;
                    ctx->current->as.binary_expr.right = 
                        ir_create_constant(ctx->builder, shift);
                    return true;
                }
                break;
                
            default:
                break;
        }
    }
    
    return false;
}

bool optimize_frame_combinations(PeepholeContext* ctx) {
    if (!ctx || !ctx->current || !ctx->next) return false;
    
    // Combine consecutive frame operations if possible
    if (ctx->current->kind == NODE_FRAME_PATTERN && 
        ctx->next->kind == NODE_FRAME_PATTERN) {
        // Check if patterns can be combined
        if (can_combine_frames(ctx->current, ctx->next)) {
            // Merge patterns into single node
            Node* combined = ir_create_node(ctx->builder, NODE_FRAME_PATTERN);
            combined->as.frame_pattern.conditions = 
                merge_conditions(ctx->current->as.frame_pattern.conditions,
                               ctx->next->as.frame_pattern.conditions);
            combined->as.frame_pattern.handler = 
                ctx->next->as.frame_pattern.handler;
            
            ctx->current = combined;
            ctx->next = ctx->next->next;
            return true;
        }
    }
    
    return false;
}

// Utility functions
static bool is_power_of_two(Node* node) {
    if (!node || node->kind != NODE_INTEGER_LITERAL) return false;
    uint64_t value = node->as.int_value;
    return value && !(value & (value - 1));
}

static uint32_t count_trailing_zeros(uint64_t value) {
    uint32_t count = 0;
    while ((value & 1) == 0) {
        count++;
        value >>= 1;
    }
    return count;
}

static NodeList* merge_conditions(NodeList* a, NodeList* b) {
    if (!a) return b;
    if (!b) return a;
    
    NodeList* result = a;
    NodeList* current = result;
    while (current->next) {
        current = current->next;
    }
    current->next = b;
    return result;
} 