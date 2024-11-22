#include "llvm_generator.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "llvm_optimizations.h"
#include "../utils/error.h"

// Internal structures for generator state
struct LLVMGenerator {
    LLVMContextRef context;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    LLVMTargetMachineRef target_machine;
    LLVMPassManagerRef module_pass_manager;
    LLVMPassManagerRef function_pass_manager;
    
    // Configuration
    TargetConfig target_config;
    SignalOptConfig signal_config;
    
    // Error handling
    char error_buffer[2048];
    bool has_error;
    
    // Symbol tables
    struct {
        LLVMValueRef* values;
        const char** names;
        size_t capacity;
        size_t count;
    } global_symbols;
    
    struct {
        LLVMValueRef* values;
        const char** names;
        size_t capacity;
        size_t count;
    } local_symbols;
    
    // Type cache
    struct {
        LLVMTypeRef i8;
        LLVMTypeRef i16;
        LLVMTypeRef i32;
        LLVMTypeRef i64;
        LLVMTypeRef float_type;
        LLVMTypeRef double_type;
        LLVMTypeRef void_type;
        LLVMTypeRef signal_type;
        LLVMTypeRef can_frame_type;
        LLVMTypeRef flexray_frame_type;
    } types;
    
    // Current function being generated
    LLVMValueRef current_function;
    
    // Basic block stack for control flow
    struct {
        LLVMBasicBlockRef* blocks;
        size_t capacity;
        size_t count;
    } block_stack;
    
    // Signal metadata
    struct {
        LLVMValueRef* signals;
        uint32_t* latencies;
        size_t count;
    } signal_info;
};

// Helper function prototypes
static bool initialize_target(LLVMGenerator* gen);
static bool create_target_machine(LLVMGenerator* gen);
static bool setup_module_passes(LLVMGenerator* gen);
static bool setup_function_passes(LLVMGenerator* gen);
static void setup_type_cache(LLVMGenerator* gen);
static bool generate_ecu_definition(LLVMGenerator* gen, ASTNode* node);
static bool generate_signal_definition(LLVMGenerator* gen, ASTNode* node);
static bool generate_process_definition(LLVMGenerator* gen, ASTNode* node);
static LLVMValueRef generate_expression(LLVMGenerator* gen, ASTNode* node);
static bool optimize_signal_paths(LLVMGenerator* gen);
static bool optimize_memory_layout(LLVMGenerator* gen);
static bool verify_timing_constraints(LLVMGenerator* gen);

// Error reporting helper
static void set_error(LLVMGenerator* gen, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(gen->error_buffer, sizeof(gen->error_buffer), format, args);
    va_end(args);
    gen->has_error = true;
}

// Symbol table management
static bool add_global_symbol(LLVMGenerator* gen, const char* name, LLVMValueRef value) {
    if (gen->global_symbols.count >= gen->global_symbols.capacity) {
        size_t new_capacity = gen->global_symbols.capacity * 2;
        LLVMValueRef* new_values = realloc(gen->global_symbols.values, 
                                          new_capacity * sizeof(LLVMValueRef));
        const char** new_names = realloc(gen->global_symbols.names,
                                       new_capacity * sizeof(char*));
        
        if (!new_values || !new_names) {
            set_error(gen, "Failed to expand global symbol table");
            return false;
        }
        
        gen->global_symbols.values = new_values;
        gen->global_symbols.names = new_names;
        gen->global_symbols.capacity = new_capacity;
    }
    
    gen->global_symbols.values[gen->global_symbols.count] = value;
    gen->global_symbols.names[gen->global_symbols.count] = strdup(name);
    gen->global_symbols.count++;
    
    return true;
}

// Type conversion helpers
static LLVMTypeRef get_llvm_type_for_signal(LLVMGenerator* gen, ASTNode* signal_node) {
    assert(signal_node->type == AST_SIGNAL_DEF);
    
    // Create struct type for signal based on properties
    LLVMTypeRef element_types[4];
    element_types[0] = gen->types.i32;  // ID
    element_types[1] = gen->types.i16;  // Length
    element_types[2] = gen->types.i8;   // Protocol
    element_types[3] = gen->types.i8;   // Flags
    
    return LLVMStructTypeInContext(gen->context, element_types, 4, false);
}

// Initialize the LLVM generator
LLVMGenerator* llvm_generator_create(const char* module_name, 
                                   const TargetConfig* target_config,
                                   const SignalOptConfig* signal_config) {
    LLVMGenerator* gen = calloc(1, sizeof(LLVMGenerator));
    if (!gen) return NULL;
    
    // Copy configurations
    memcpy(&gen->target_config, target_config, sizeof(TargetConfig));
    memcpy(&gen->signal_config, signal_config, sizeof(SignalOptConfig));
    
    // Initialize LLVM
    gen->context = LLVMContextCreate();
    gen->module = LLVMModuleCreateWithNameInContext(module_name, gen->context);
    gen->builder = LLVMCreateBuilderInContext(gen->context);
    
    // Initialize symbol tables
    gen->global_symbols.capacity = 1024;
    gen->global_symbols.values = calloc(gen->global_symbols.capacity, 
                                      sizeof(LLVMValueRef));
    gen->global_symbols.names = calloc(gen->global_symbols.capacity, 
                                     sizeof(char*));
    
    gen->local_symbols.capacity = 1024;
    gen->local_symbols.values = calloc(gen->local_symbols.capacity, 
                                     sizeof(LLVMValueRef));
    gen->local_symbols.names = calloc(gen->local_symbols.capacity, 
                                    sizeof(char*));
    
    // Initialize basic block stack
    gen->block_stack.capacity = 64;
    gen->block_stack.blocks = calloc(gen->block_stack.capacity, 
                                   sizeof(LLVMBasicBlockRef));
    
    // Setup types and target
    setup_type_cache(gen);
    if (!initialize_target(gen)) {
        llvm_generator_destroy(gen);
        return NULL;
    }
    
    return gen;
}

// Setup type cache for commonly used types
static void setup_type_cache(LLVMGenerator* gen) {
    gen->types.i8 = LLVMInt8TypeInContext(gen->context);
    gen->types.i16 = LLVMInt16TypeInContext(gen->context);
    gen->types.i32 = LLVMInt32TypeInContext(gen->context);
    gen->types.i64 = LLVMInt64TypeInContext(gen->context);
    gen->types.float_type = LLVMFloatTypeInContext(gen->context);
    gen->types.double_type = LLVMDoubleTypeInContext(gen->context);
    gen->types.void_type = LLVMVoidTypeInContext(gen->context);
    
    // Create complex types for automotive signals
    LLVMTypeRef signal_elements[4] = {
        gen->types.i32,  // ID
        gen->types.i16,  // Length
        gen->types.i8,   // Protocol
        gen->types.i8    // Flags
    };
    gen->types.signal_type = LLVMStructTypeInContext(gen->context, 
                                                    signal_elements, 
                                                    4, false);
    
    // CAN frame type
    LLVMTypeRef can_elements[5] = {
        gen->types.i32,  // ID
        gen->types.i8,   // DLC
        LLVMArrayType(gen->types.i8, 8),  // Data
        gen->types.i8,   // Flags
        gen->types.i32   // Timestamp
    };
    gen->types.can_frame_type = LLVMStructTypeInContext(gen->context, 
                                                       can_elements, 
                                                       5, false);
    
    // FlexRay frame type
    LLVMTypeRef flexray_elements[6] = {
        gen->types.i16,  // Slot ID
        gen->types.i8,   // Cycle
        gen->types.i8,   // Channel
        LLVMArrayType(gen->types.i8, 254),  // Data
        gen->types.i8,   // Length
        gen->types.i32   // Timestamp
    };
    gen->types.flexray_frame_type = LLVMStructTypeInContext(gen->context, 
                                                           flexray_elements, 
                                                           6, false);
}

// Initialize target machine
static bool initialize_target(LLVMGenerator* gen) {
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();
    
    return create_target_machine(gen);
}

// Create target machine based on configuration
static bool create_target_machine(LLVMGenerator* gen) {
    char* error = NULL;
    const char* triple = NULL;
    const char* cpu = gen->target_config.cpu;
    const char* features = gen->target_config.features;
    
    // Select target triple based on architecture
    switch (gen->target_config.arch) {
        case TARGET_S32K344:
            triple = "arm-none-eabi";
            if (!cpu) cpu = "cortex-m7";
            if (!features) features = "+vfp4,+neon";
            break;
            
        case TARGET_TDA4VM:
            triple = "aarch64-none-elf";
            if (!cpu) cpu = "cortex-a72";
            if (!features) features = "+neon,+crypto";
            break;
            
        case TARGET_MPC5748G:
            triple = "powerpc-none-eabi";
            if (!cpu) cpu = "e200z4";
            break;
            
        case TARGET_GENERIC:
            triple = LLVMGetDefaultTargetTriple();
            break;
            
        default:
            set_error(gen, "Unsupported target architecture");
            return false;
    }
    
    LLVMTargetRef target;
    if (LLVMGetTargetFromTriple(triple, &target, &error) != 0) {
        set_error(gen, "Failed to get target: %s", error);
        LLVMDisposeMessage(error);
        return false;
    }
    
    // Create target machine with appropriate options
    LLVMCodeGenOptLevel opt_level = gen->target_config.optimize_size ? 
                                   LLVMCodeGenLevelDefault : 
                                   LLVMCodeGenLevelAggressive;
    
    LLVMRelocMode reloc = LLVMRelocDefault;
    LLVMCodeModel code_model = LLVMCodeModelDefault;
    
    gen->target_machine = LLVMCreateTargetMachine(target, triple, cpu, features,
                                                 opt_level, reloc, code_model);
    
    if (!gen->target_machine) {
        set_error(gen, "Failed to create target machine");
        return false;
    }
    
    // Set target data layout
    char* data_layout = LLVMCopyStringRepOfTargetData(
        LLVMCreateTargetDataLayout(gen->target_machine));
    LLVMSetDataLayout(gen->module, data_layout);
    LLVMDisposeMessage(data_layout);
    
    // Set target triple
    LLVMSetTarget(gen->module, triple);
    
    return true;
}

// Main compilation entry point
bool llvm_generator_compile_ast(LLVMGenerator* gen, ASTNode* ast) {
    if (!gen || !ast) return false;
    
    gen->has_error = false;
    
    // Process top-level nodes
    switch (ast->type) {
        case AST_ECU_DEF:
            return generate_ecu_definition(gen, ast);
            
        case AST_SIGNAL_DEF:
            return generate_signal_definition(gen, ast);
            
        case AST_PROCESS_DEF:
            return generate_process_definition(gen, ast);
            
        default:
            set_error(gen, "Unexpected top-level node type");
            return false;
    }
}

// Generate code for ECU definition
static bool generate_ecu_definition(LLVMGenerator* gen, ASTNode* node) {
    assert(node->type == AST_ECU_DEF);
    
    // Create ECU initialization function
    LLVMTypeRef init_func_type = LLVMFunctionType(gen->types.void_type, NULL, 0, false);
    char* init_func_name = NULL;
    asprintf(&init_func_name, "%s_init", node->as.ecu_def.name);
    
    LLVMValueRef init_func = LLVMAddFunction(gen->module, init_func_name, init_func_type);
    free(init_func_name);
    
    // Create entry block
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(gen->context, 
                                                           init_func, "entry");
    LLVMPositionBuilderAtEnd(gen->builder, entry);
    
    // Generate initialization code for each signal
    for (size_t i = 0; i < node->as.ecu_def.signal_count; i++) {
        if (!generate_signal_definition(gen, node->as.ecu_def.signals[i])) {
            return false;
        }
    }
    
    // Generate initialization code for each process
    for (size_t i = 0; i < node->as.ecu_def.process_count; i++) {
        if (!generate_process_definition(gen, node->as.ecu_def.processes[i])) {
            return false;
        }
    }
    
    // Add return instruction
    LLVMBuildRetVoid(gen->builder);
    
    // Verify the function
    char* error = NULL;
    if (LLVMVerifyFunction(init_func, LLVMPrintMessageAction)) {
        set_error(gen, "Invalid ECU initialization function");
        return false;
    }
    
    return true;
}

// Clean up resources
void llvm_generator_destroy(LLVMGenerator* gen) {
    if (!gen) return;
    
    // Clean up LLVM resources
    if (gen->module_pass_manager)
        LLVMDisposePassManager(gen->module_pass_manager);
    if (gen->function_pass_manager)
        LLVMDisposePassManager(gen->function_pass_manager);
    if (gen->target_machine)
        LLVMDisposeTargetMachine(gen->target_machine);
    if (gen->builder)
        LLVMDisposeBuilder(gen->builder);
    if (gen->module)
        LLVMDisposeModule(gen->module);
    if (gen->context)
        LLVMContextDispose(gen->context);
    
    // Clean up symbol tables
    for (size_t i = 0; i < gen->global_symbols.count; i++) {
        free((void*)gen->global_symbols.names[i]);
    }
    free(gen->global_symbols.values);
    free(gen->global_symbols.names);
    
    for (size_t i = 0; i < gen->local_symbols.count; i++) {
        free((void*)gen->local_symbols.names[i]);
    }
    free(gen->local_symbols.values);
    free(gen->local_symbols.names);
    
    // Clean up basic block stack
    free(gen->block_stack.blocks);
    
    free(gen);
}

// Write LLVM IR to file
bool llvm_generator_write_ir(LLVMGenerator* gen, const char* filename) {
    if (!gen || !filename) return false;
    
    char* error = NULL;
    if (LLVMPrintModuleToFile(gen->module, filename, &error)) {
        set_error(gen, "Failed to write IR: %s", error);
        LLVMDisposeMessage(error);
        return false;
    }
    
    return true;
}

// Write object file
bool llvm_generator_write_object(LLVMGenerator* gen, const char* filename) {
    if (!gen || !filename) return false;
    
    char* error = NULL;
    if (LLVMTargetMachineEmitToFile(gen->target_machine, gen->module, 
                                   (char*)filename, LLVMObjectFile, &error)) {
        set_error(gen, "Failed to write object file: %s", error);
        LLVMDisposeMessage(error);
        return false;
    }
    
    return true;
}

// Get last error message
const char* llvm_generator_get_error(LLVMGenerator* gen) {
    return gen && gen->has_error ? gen->error_buffer : NULL;
} 