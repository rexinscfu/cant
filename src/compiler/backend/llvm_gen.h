#ifndef CANT_LLVM_GEN_H
#define CANT_LLVM_GEN_H

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>
#include "../frontend/parser.h"

// LLVM Context wrapper
typedef struct {
    LLVMContextRef context;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    LLVMTargetDataRef target_data;
} LLVMGenContext;

// Diagnostic service code generation
LLVMValueRef gen_diagnostic_service(LLVMGenContext* ctx, Node* service_node);
LLVMValueRef gen_diagnostic_pattern(LLVMGenContext* ctx, Node* pattern_node);
LLVMValueRef gen_frame_matcher(LLVMGenContext* ctx, Node* frame_node);
LLVMValueRef gen_data_matcher(LLVMGenContext* ctx, Node* data_node);

// Session and security management
LLVMValueRef gen_session_fsm(LLVMGenContext* ctx, Node* session_node);
LLVMValueRef gen_security_check(LLVMGenContext* ctx, Node* security_node);
LLVMValueRef gen_access_validator(LLVMGenContext* ctx, Node* access_node);

// Frame handling and optimization
LLVMValueRef gen_frame_handler(LLVMGenContext* ctx, Node* frame_node);
LLVMValueRef gen_frame_filter(LLVMGenContext* ctx, Node* filter_node);
LLVMValueRef gen_frame_buffer(LLVMGenContext* ctx, Node* buffer_node);

// SIMD optimizations for frame processing
LLVMValueRef gen_simd_frame_matcher(LLVMGenContext* ctx, Node* frame_node);
LLVMValueRef gen_simd_data_filter(LLVMGenContext* ctx, Node* data_node);

// Intrinsic functions for diagnostic operations
LLVMValueRef gen_can_send_intrinsic(LLVMGenContext* ctx);
LLVMValueRef gen_can_receive_intrinsic(LLVMGenContext* ctx);
LLVMValueRef gen_pattern_match_intrinsic(LLVMGenContext* ctx);
LLVMValueRef gen_flow_control_intrinsic(LLVMGenContext* ctx);

// Context management
LLVMGenContext* llvm_gen_context_create(void);
void llvm_gen_context_destroy(LLVMGenContext* ctx);
bool llvm_gen_verify_module(LLVMGenContext* ctx);
void llvm_gen_optimize_module(LLVMGenContext* ctx, int opt_level);

// Error handling
typedef struct {
    const char* message;
    const char* function;
    int line;
} LLVMGenError;

LLVMGenError llvm_gen_get_last_error(void);
void llvm_gen_clear_error(void);

#endif // CANT_LLVM_GEN_H
