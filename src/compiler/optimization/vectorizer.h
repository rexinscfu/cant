#ifndef CANT_VECTORIZER_H
#define CANT_VECTORIZER_H

#include "../ir/ir_builder.h"
#include "../analysis/loop_analysis.h"
#include <stdbool.h>

// Vector operation types
typedef enum {
    VEC_LOAD,
    VEC_STORE,
    VEC_ADD,
    VEC_SUB,
    VEC_MUL,
    VEC_DIV,
    VEC_AND,
    VEC_OR,
    VEC_XOR,
    VEC_CMP,
    VEC_SHUFFLE
} VectorOp;

// Vector pattern types
typedef enum {
    VEC_CONSECUTIVE,
    VEC_STRIDED,
    VEC_SCATTERED,
    VEC_MASKED,
    VEC_REDUCTION
} VectorPattern;

// Vectorization context
typedef struct {
    IRBuilder* builder;
    Loop* current_loop;
    uint32_t vector_width;
    bool use_masked_ops;
    bool use_scatter_gather;
    uint32_t min_trip_count;
} VectContext;

// Vectorization analysis
bool can_vectorize_loop(VectContext* ctx, Loop* loop);
bool can_vectorize_pattern(VectContext* ctx, Node* pattern);
VectorPattern analyze_access_pattern(Node* node);
uint32_t get_optimal_vector_width(Loop* loop);

// Vectorization transforms
bool vectorize_loop(VectContext* ctx, Loop* loop);
bool vectorize_pattern(VectContext* ctx, Node* pattern);
Node* create_vector_op(VectContext* ctx, VectorOp op, Node** operands, uint32_t count);
Node* create_vector_reduction(VectContext* ctx, VectorOp op, Node* vector);

// Cost model
typedef struct {
    uint32_t scalar_cost;
    uint32_t vector_cost;
    uint32_t setup_cost;
    uint32_t cleanup_cost;
    bool profitable;
} VectCost;

VectCost* analyze_vectorization_cost(VectContext* ctx, Loop* loop);
bool is_vectorization_profitable(VectCost* cost);

// Target-specific info
bool target_supports_vector_width(uint32_t width);
bool target_supports_masked_ops(void);
bool target_supports_scatter_gather(void);
uint32_t target_preferred_vector_width(void);

#endif // CANT_VECTORIZER_H 