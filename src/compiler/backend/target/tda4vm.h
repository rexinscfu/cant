#ifndef CANT_TARGET_TDA4VM_H
#define CANT_TARGET_TDA4VM_H

#include "../llvm_generator.h"

// TDA4VM-specific target configuration
bool configure_tda4vm_target(LLVMGenerator* gen);
bool generate_tda4vm_specific_code(LLVMGenerator* gen);
bool optimize_for_tda4vm(LLVMGenerator* gen);

#endif // CANT_TARGET_TDA4VM_H 