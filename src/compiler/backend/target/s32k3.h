#ifndef CANT_TARGET_S32K3_H
#define CANT_TARGET_S32K3_H

#include "../llvm_generator.h"

// S32K3-specific target configuration
bool configure_s32k3_target(LLVMGenerator* gen);
bool generate_s32k3_specific_code(LLVMGenerator* gen);
bool optimize_for_s32k3(LLVMGenerator* gen);

#endif // CANT_TARGET_S32K3_H 