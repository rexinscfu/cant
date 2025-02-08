#include "ir_builder.h"
#include <stdlib.h>
#include <string.h>

struct IRBuilder {
    Node* current_block;
    uint32_t temp_count;
};

IRBuilder* ir_create_builder(void) {
    IRBuilder* builder = malloc(sizeof(IRBuilder));
    if (!builder) return NULL;
    
    builder->current_block = NULL;
    builder->temp_count = 0;
    return builder;
}

void ir_destroy_builder(IRBuilder* builder) {
    if (!builder) return;
    free(builder);
}

Node* ir_create_node(IRBuilder* builder, NodeKind kind) {
    if (!builder) return NULL;
    
    Node* node = malloc(sizeof(Node));
    if (!node) return NULL;
    
    memset(node, 0, sizeof(Node));
    node->kind = kind;
    return node;
}

Node* ir_create_binary_op(IRBuilder* builder, Node* left, Node* right, IROp op) {
    if (!builder || !left || !right) return NULL;
    
    Node* node = ir_create_node(builder, NODE_BINARY_EXPR);
    if (!node) return NULL;
    
    node->as.binary_expr.left = left;
    node->as.binary_expr.right = right;
    node->as.binary_expr.op.kind = op;
    return node;
}

Node* ir_create_constant(IRBuilder* builder, uint64_t value) {
    if (!builder) return NULL;
    
    Node* node = ir_create_node(builder, NODE_INTEGER_LITERAL);
    if (!node) return NULL;
    
    node->as.int_value = value;
    return node;
}

Node* ir_create_simd_load(IRBuilder* builder, const uint8_t* data) {
    if (!builder || !data) return NULL;
    
    Node* node = ir_create_node(builder, NODE_FRAME_DATA);
    if (!node) return NULL;
    
    uint8_t* data_copy = malloc(16);
    if (!data_copy) {
        free(node);
        return NULL;
    }
    
    memcpy(data_copy, data, 16);
    node->as.frame_data.data = data_copy;
    node->as.frame_data.length = 16;
    return node;
}

Node* ir_create_simd_compare(IRBuilder* builder, Node* a, Node* b) {
    if (!builder || !a || !b) return NULL;
    
    return ir_create_binary_op(builder, a, b, IR_OP_EQ);
}

Node* ir_create_lookup(IRBuilder* builder, const uint8_t* table, uint32_t size) {
    if (!builder || !table || size == 0) return NULL;
    
    Node* node = ir_create_node(builder, NODE_FRAME_PATTERN);
    if (!node) return NULL;
    
    uint8_t* table_copy = malloc(size);
    if (!table_copy) {
        free(node);
        return NULL;
    }
    
    memcpy(table_copy, table, size);
    node->as.frame_pattern.data = table_copy;
    node->as.frame_pattern.data_length = size;
    return node;
}

Node* ir_create_byte_extract(IRBuilder* builder, uint32_t index) {
    if (!builder) return NULL;
    
    Node* node = ir_create_node(builder, NODE_FRAME_DATA);
    if (!node) return NULL;
    
    node->as.frame_data.data = malloc(1);
    if (!node->as.frame_data.data) {
        free(node);
        return NULL;
    }
    
    node->as.frame_data.data[0] = index;
    node->as.frame_data.length = 1;
    return node;
}

NodeList* ir_create_node_list(Node* node) {
    if (!node) return NULL;
    
    NodeList* list = malloc(sizeof(NodeList));
    if (!list) return NULL;
    
    list->node = node;
    list->next = NULL;
    return list;
}

NodeList* ir_append_node(NodeList* list, Node* node) {
    if (!node) return list;
    if (!list) return ir_create_node_list(node);
    
    NodeList* current = list;
    while (current->next) {
        current = current->next;
    }
    
    current->next = ir_create_node_list(node);
    return list;
}

void ir_destroy_node(Node* node) {
    if (!node) return;
    
    switch (node->kind) {
        case NODE_FRAME_DATA:
            free(node->as.frame_data.data);
            free(node->as.frame_data.mask);
            break;
            
        case NODE_FRAME_PATTERN:
            free(node->as.frame_pattern.data);
            free(node->as.frame_pattern.mask);
            break;
            
        case NODE_BINARY_EXPR:
            ir_destroy_node(node->as.binary_expr.left);
            ir_destroy_node(node->as.binary_expr.right);
            break;
            
        default:
            break;
    }
    
    free(node);
} 