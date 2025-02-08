#ifndef CANT_S32K3_TARGET_INFO_H
#define CANT_S32K3_TARGET_INFO_H

#include <stdint.h>
#include <stdbool.h>

// S32K3 target features
typedef enum {
    S32K3_FEATURE_SIMD = 1 << 0,
    S32K3_FEATURE_FPU = 1 << 1,
    S32K3_FEATURE_DSP = 1 << 2,
    S32K3_FEATURE_SECURITY = 1 << 3,
    S32K3_FEATURE_CAN_FD = 1 << 4
} S32K3Feature;

// Target-specific configuration
typedef struct {
    uint32_t cpu_frequency;
    uint32_t can_frequency;
    uint32_t ram_size;
    uint32_t flash_size;
    uint32_t features;
} S32K3Config;

// Target capabilities
typedef struct {
    uint32_t max_can_filters;
    uint32_t max_frame_size;
    uint32_t max_baudrate;
    uint32_t min_baudrate;
    bool has_fd_support;
    bool has_auto_retransmit;
} S32K3Capabilities;

// Target-specific optimizations
typedef struct {
    bool use_dma;
    bool use_simd;
    bool use_hardware_filtering;
    bool enable_fifo;
    uint32_t fifo_size;
    uint32_t filter_count;
} S32K3Optimizations;

// Target information API
const char* s32k3_get_target_name(void);
const char* s32k3_get_target_description(void);
S32K3Capabilities s32k3_get_capabilities(void);
bool s32k3_has_feature(S32K3Feature feature);
bool s32k3_validate_config(const S32K3Config* config);

// Target-specific intrinsics
typedef enum {
    S32K3_INTRINSIC_CAN_SEND,
    S32K3_INTRINSIC_CAN_RECEIVE,
    S32K3_INTRINSIC_PATTERN_MATCH,
    S32K3_INTRINSIC_DMA_TRANSFER,
    S32K3_INTRINSIC_SIMD_COMPARE
} S32K3Intrinsic;

// Intrinsic information
typedef struct {
    const char* name;
    uint32_t num_args;
    bool is_pure;
    bool has_side_effects;
} S32K3IntrinsicInfo;

const S32K3IntrinsicInfo* s32k3_get_intrinsic_info(S32K3Intrinsic intrinsic);
bool s32k3_can_inline_intrinsic(S32K3Intrinsic intrinsic);

#endif // CANT_S32K3_TARGET_INFO_H 