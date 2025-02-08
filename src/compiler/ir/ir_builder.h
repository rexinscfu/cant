#ifndef CANT_IR_BUILDER_H
#define CANT_IR_BUILDER_H

#include "../frontend/parser.h"
#include <stdint.h>

typedef struct IRBuilder IRBuilder;

// IR Operations
typedef enum {
    IR_OP_ADD,
    IR_OP_SUB,
    IR_OP_MUL,
    IR_OP_DIV,
    IR_OP_MOD,
    IR_OP_AND,
    IR_OP_OR,
    IR_OP_XOR,
    IR_OP_NOT,
    IR_OP_SHL,
    IR_OP_SHR,
    IR_OP_EQ,
    IR_OP_NEQ,
    IR_OP_LT,
    IR_OP_GT,
    IR_OP_LE,
    IR_OP_GE
} IROp;

// IR Builder API
IRBuilder* ir_create_builder(void);
void ir_destroy_builder(IRBuilder* builder);

// Node creation
Node* ir_create_node(IRBuilder* builder, NodeKind kind);
Node* ir_create_binary_op(IRBuilder* builder, Node* left, Node* right, IROp op);
Node* ir_create_constant(IRBuilder* builder, uint64_t value);
Node* ir_create_simd_load(IRBuilder* builder, const uint8_t* data);
Node* ir_create_simd_compare(IRBuilder* builder, Node* a, Node* b);
Node* ir_create_lookup(IRBuilder* builder, const uint8_t* table, uint32_t size);
Node* ir_create_byte_extract(IRBuilder* builder, uint32_t index);

// Node list operations
NodeList* ir_create_node_list(Node* node);
NodeList* ir_append_node(NodeList* list, Node* node);
void ir_destroy_node(Node* node);

#endif // CANT_IR_BUILDER_H 