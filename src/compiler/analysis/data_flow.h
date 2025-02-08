#ifndef CANT_DATA_FLOW_H
#define CANT_DATA_FLOW_H

#include "../ir/ir_builder.h"
#include <stdbool.h>

// Data flow analysis types
typedef enum {
    FLOW_REACHING_DEFS,
    FLOW_LIVE_VARIABLES,
    FLOW_AVAILABLE_EXPRS,
    FLOW_VERY_BUSY_EXPRS,
    FLOW_DOMINATORS,
    FLOW_LOOP_DETECTION
} DataFlowType;

// Analysis results
typedef struct {
    bool* in;
    bool* out;
    uint32_t size;
} DataFlowResult;

// Analysis context
typedef struct {
    IRBuilder* builder;
    Node* cfg;
    DataFlowType type;
    DataFlowResult result;
    bool forward;
    bool changed;
} DataFlowContext;

// Analysis functions
bool data_flow_analyze(DataFlowContext* ctx);
bool data_flow_init(DataFlowContext* ctx, DataFlowType type);
void data_flow_destroy(DataFlowContext* ctx);

// Transfer functions
void transfer_reaching_defs(DataFlowContext* ctx, Node* node);
void transfer_live_variables(DataFlowContext* ctx, Node* node);
void transfer_available_exprs(DataFlowContext* ctx, Node* node);
void transfer_very_busy_exprs(DataFlowContext* ctx, Node* node);

// Meet operations
void meet_reaching_defs(bool* result, bool* in1, bool* in2, uint32_t size);
void meet_live_variables(bool* result, bool* in1, bool* in2, uint32_t size);
void meet_available_exprs(bool* result, bool* in1, bool* in2, uint32_t size);
void meet_very_busy_exprs(bool* result, bool* in1, bool* in2, uint32_t size);

// Analysis utilities
bool is_definition(Node* node);
bool is_use(Node* node);
bool is_expression(Node* node);
uint32_t get_def_id(Node* node);
uint32_t get_use_id(Node* node);
uint32_t get_expr_id(Node* node);

// Optimization helpers
bool can_eliminate_redundant(DataFlowContext* ctx, Node* node);
bool can_eliminate_dead_code(DataFlowContext* ctx, Node* node);
bool should_hoist_expression(DataFlowContext* ctx, Node* node);
bool should_sink_expression(DataFlowContext* ctx, Node* node);

#endif // CANT_DATA_FLOW_H 