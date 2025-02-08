#include "llvm_gen.h"
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>
#include <llvm-c/Transforms/Scalar.h>
#include <llvm-c/Transforms/Vectorize.h>
#include <stdlib.h>
#include <string.h>

// Error handling
static LLVMGenError current_error = {NULL, NULL, 0};

static void set_error(const char* message, const char* function, int line) {
    current_error.message = message;
    current_error.function = function;
    current_error.line = line;
}

LLVMGenError llvm_gen_get_last_error(void) {
    return current_error;
}

void llvm_gen_clear_error(void) {
    current_error.message = NULL;
    current_error.function = NULL;
    current_error.line = 0;
}

// Context management
LLVMGenContext* llvm_gen_context_create(void) {
    LLVMGenContext* ctx = malloc(sizeof(LLVMGenContext));
    if (!ctx) {
        set_error("Failed to allocate context", __func__, __LINE__);
        return NULL;
    }

    ctx->context = LLVMContextCreate();
    ctx->module = LLVMModuleCreateWithNameInContext("cant_module", ctx->context);
    ctx->builder = LLVMCreateBuilderInContext(ctx->context);
    ctx->target_data = LLVMCreateTargetData("");

    return ctx;
}

void llvm_gen_context_destroy(LLVMGenContext* ctx) {
    if (!ctx) return;
    
    LLVMDisposeBuilder(ctx->builder);
    LLVMDisposeModule(ctx->module);
    LLVMContextDispose(ctx->context);
    LLVMDisposeTargetData(ctx->target_data);
    free(ctx);
}

// Type definitions
static LLVMTypeRef get_frame_type(LLVMGenContext* ctx) {
    LLVMTypeRef frame_elements[] = {
        LLVMInt32TypeInContext(ctx->context),  // id
        LLVMInt8TypeInContext(ctx->context),   // dlc
        LLVMArrayType(LLVMInt8TypeInContext(ctx->context), 64), // data
        LLVMInt1TypeInContext(ctx->context)    // extended
    };
    
    return LLVMStructTypeInContext(ctx->context, frame_elements, 4, false);
}

static LLVMTypeRef get_pattern_type(LLVMGenContext* ctx) {
    LLVMTypeRef pattern_elements[] = {
        LLVMInt32TypeInContext(ctx->context),  // id_mask
        LLVMInt8TypeInContext(ctx->context),   // dlc_mask
        LLVMArrayType(LLVMInt8TypeInContext(ctx->context), 64), // data_mask
        LLVMArrayType(LLVMInt8TypeInContext(ctx->context), 64)  // data_pattern
    };
    
    return LLVMStructTypeInContext(ctx->context, pattern_elements, 4, false);
}

// SIMD helper functions
static LLVMValueRef create_simd_matcher(LLVMGenContext* ctx, LLVMValueRef data, LLVMValueRef pattern, LLVMValueRef mask) {
    // Create vector types for 16-byte SIMD operations
    LLVMTypeRef v16i8 = LLVMVectorType(LLVMInt8TypeInContext(ctx->context), 16);
    
    // Load data into vector registers
    LLVMValueRef data_vec = LLVMBuildBitCast(ctx->builder, data, v16i8, "data_vec");
    LLVMValueRef pattern_vec = LLVMBuildBitCast(ctx->builder, pattern, v16i8, "pattern_vec");
    LLVMValueRef mask_vec = LLVMBuildBitCast(ctx->builder, mask, v16i8, "mask_vec");
    
    // Apply mask: (data & mask) == (pattern & mask)
    LLVMValueRef masked_data = LLVMBuildAnd(ctx->builder, data_vec, mask_vec, "masked_data");
    LLVMValueRef masked_pattern = LLVMBuildAnd(ctx->builder, pattern_vec, mask_vec, "masked_pattern");
    LLVMValueRef cmp = LLVMBuildICmp(ctx->builder, LLVMIntEQ, masked_data, masked_pattern, "cmp");
    
    // Reduce vector comparison to scalar
    return LLVMBuildExtractElement(ctx->builder, cmp, LLVMConstInt(LLVMInt32TypeInContext(ctx->context), 0, false), "match");
}

// Diagnostic service code generation
LLVMValueRef gen_diagnostic_service(LLVMGenContext* ctx, Node* service_node) {
    if (!ctx || !service_node) {
        set_error("Invalid context or node", __func__, __LINE__);
        return NULL;
    }
    
    // Create service function type
    LLVMTypeRef frame_type = get_frame_type(ctx);
    LLVMTypeRef param_types[] = { frame_type };
    LLVMTypeRef func_type = LLVMFunctionType(
        LLVMInt1TypeInContext(ctx->context),
        param_types,
        1,
        false
    );
    
    // Create function
    char func_name[256];
    snprintf(func_name, sizeof(func_name), "diagnostic_service_%d", service_node->as.diag_service.id);
    LLVMValueRef func = LLVMAddFunction(ctx->module, func_name, func_type);
    
    // Create entry block
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx->context, func, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, entry);
    
    // Generate pattern matching code
    NodeList* pattern = service_node->as.diag_service.config.patterns;
    while (pattern) {
        gen_diagnostic_pattern(ctx, pattern->node);
        pattern = pattern->next;
    }
    
    // Add return
    LLVMBuildRet(ctx->builder, LLVMConstInt(LLVMInt1TypeInContext(ctx->context), 0, false));
    
    return func;
}

LLVMValueRef gen_diagnostic_pattern(LLVMGenContext* ctx, Node* pattern_node) {
    if (!ctx || !pattern_node) {
        set_error("Invalid context or node", __func__, __LINE__);
        return NULL;
    }
    
    // Create pattern matching function
    LLVMTypeRef frame_type = get_frame_type(ctx);
    LLVMTypeRef pattern_type = get_pattern_type(ctx);
    
    LLVMTypeRef param_types[] = { frame_type, pattern_type };
    LLVMTypeRef func_type = LLVMFunctionType(
        LLVMInt1TypeInContext(ctx->context),
        param_types,
        2,
        false
    );
    
    LLVMValueRef func = LLVMAddFunction(ctx->module, "pattern_match", func_type);
    
    // Generate optimized pattern matching code using SIMD
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx->context, func, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, entry);
    
    // Extract frame data and pattern
    LLVMValueRef frame = LLVMGetParam(func, 0);
    LLVMValueRef pattern = LLVMGetParam(func, 1);
    
    // Generate SIMD pattern matching
    LLVMValueRef data = LLVMBuildExtractValue(ctx->builder, frame, 2, "data");
    LLVMValueRef pattern_data = LLVMBuildExtractValue(ctx->builder, pattern, 3, "pattern_data");
    LLVMValueRef mask = LLVMBuildExtractValue(ctx->builder, pattern, 2, "mask");
    
    LLVMValueRef match = create_simd_matcher(ctx, data, pattern_data, mask);
    
    // Return result
    LLVMBuildRet(ctx->builder, match);
    
    return func;
}

// Frame handling optimizations
LLVMValueRef gen_frame_handler(LLVMGenContext* ctx, Node* frame_node) {
    if (!ctx || !frame_node) {
        set_error("Invalid context or node", __func__, __LINE__);
        return NULL;
    }
    
    // Create frame handler function
    LLVMTypeRef frame_type = get_frame_type(ctx);
    LLVMTypeRef func_type = LLVMFunctionType(
        LLVMVoidTypeInContext(ctx->context),
        &frame_type,
        1,
        false
    );
    
    LLVMValueRef func = LLVMAddFunction(ctx->module, "frame_handler", func_type);
    
    // Generate handler code
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx->context, func, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, entry);
    
    // Extract frame data
    LLVMValueRef frame = LLVMGetParam(func, 0);
    LLVMValueRef data = LLVMBuildExtractValue(ctx->builder, frame, 2, "data");
    
    // Generate SIMD-optimized data processing
    gen_simd_data_filter(ctx, frame_node);
    
    LLVMBuildRetVoid(ctx->builder);
    
    return func;
}

// Intrinsic functions
LLVMValueRef gen_can_send_intrinsic(LLVMGenContext* ctx) {
    LLVMTypeRef frame_type = get_frame_type(ctx);
    LLVMTypeRef func_type = LLVMFunctionType(
        LLVMInt1TypeInContext(ctx->context),
        &frame_type,
        1,
        false
    );
    
    return LLVMAddFunction(ctx->module, "can_send", func_type);
}

LLVMValueRef gen_can_receive_intrinsic(LLVMGenContext* ctx) {
    LLVMTypeRef frame_type = get_frame_type(ctx);
    LLVMTypeRef func_type = LLVMFunctionType(
        frame_type,
        NULL,
        0,
        false
    );
    
    return LLVMAddFunction(ctx->module, "can_receive", func_type);
}

// Module verification and optimization
bool llvm_gen_verify_module(LLVMGenContext* ctx) {
    char* error = NULL;
    LLVMVerifyModule(ctx->module, LLVMAbortProcessAction, &error);
    if (error) {
        set_error(error, __func__, __LINE__);
        LLVMDisposeMessage(error);
        return false;
    }
    return true;
}

void llvm_gen_optimize_module(LLVMGenContext* ctx, int opt_level) {
    // Create pass manager
    LLVMPassManagerRef pass_manager = LLVMCreatePassManager();
    
    // Add optimization passes
    if (opt_level >= 1) {
        LLVMAddPromoteMemoryToRegisterPass(pass_manager);
        LLVMAddInstructionCombiningPass(pass_manager);
        LLVMAddReassociatePass(pass_manager);
        LLVMAddGVNPass(pass_manager);
        LLVMAddCFGSimplificationPass(pass_manager);
    }
    
    if (opt_level >= 2) {
        LLVMAddBasicAliasAnalysisPass(pass_manager);
        LLVMAddBBVectorizePass(pass_manager);
        LLVMAddLoopVectorizePass(pass_manager);
        LLVMAddSLPVectorizePass(pass_manager);
    }
    
    // Run optimizations
    LLVMRunPassManager(pass_manager, ctx->module);
    
    // Cleanup
    LLVMDisposePassManager(pass_manager);
}
