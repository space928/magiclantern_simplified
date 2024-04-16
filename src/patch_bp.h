#ifndef _patch_bp_h_
#define _patch_bp_h_

#include "arm-mcr.h"
#include "stdbool.h"

enum debug_version
{
    INVALID = 0, 
    ARM_V6_DBG = 1, 
    ARM_V61_DBG = 2, 
    ARM_V7_EX_DBG = 3, 
    ARM_V7_BASE_DBG = 4
};

typedef struct debug_info
{
    uint8_t revision : 4;
    uint8_t variant : 4;
    uint8_t unused : 4;
    bool security_ex_imp : 1;
    bool pc_sampling_reg_imp : 1;
    bool n_secure_user_halt_imp : 1; // True when NOT implemented
    bool devid_imp : 1;
    enum debug_version version : 4;
    uint8_t num_ctx_cmps : 4; 
    uint8_t num_breakpoint_reg_pairs : 4;
    uint8_t num_watchpoint_reg_pairs : 4;
} PACKED debug_info;
SIZE_CHECK_STRUCT(debug_info, 4);

typedef union
{
    debug_info s;
    uint32_t u;
} debug_info_u;

// https://developer.arm.com/documentation/ddi0406/b/Debug-Architecture/Debug-Register-Interfaces/Debug-register-map
// Be aware that some of these registers are read-only/write-only
typedef struct debug_registers
{
    /*debug_info*/uint32_t debug_id;
    uint32_t reserved_0[5];
    uint32_t wp_fault_addr;
    uint32_t vector_catch;
    uint32_t reserved_1[1];
    uint32_t event_catch;
    uint32_t debug_state_cache_control;
    uint32_t debug_mmu_control;
    uint32_t reserved_2[20];
    uint32_t host_to_target_data_xfer;
    union {
        uint32_t instruction_transfer;
        uint32_t pc_sampling;
    };
    uint32_t debug_status_and_control;
    uint32_t target_to_host_data_xfer;
    uint32_t run_control;
    uint32_t reserved_3[3];
    uint32_t pc_sampling_1;
    uint32_t context_id_sampling;
    uint32_t reserved_4[22];
    uint32_t bp_val[16];
    uint32_t bp_control[16];
    uint32_t wp_val[16];
    uint32_t wp_control[16];
    uint32_t reserved_5[64];
    uint32_t os_lock_access;
    uint32_t os_lock_status;
    uint32_t os_save_restore;
    uint32_t reserved_6[1];
    uint32_t device_power_down_reset_control;
    uint32_t device_power_down_reset_status;
    uint32_t reserved_7[314];
    uint32_t reserved_8[64];
    uint32_t reserved_9[256];
    uint32_t processor_identification[64];
    uint32_t reserved_10[32];
    uint32_t reserved_11[32];
    uint32_t integration_mode_control;
    uint32_t reserved_12[39];
    uint32_t claim_tag_set;
    uint32_t claim_tag_clear;
    uint32_t reserved_13[2];
    uint32_t lock_access;
    uint32_t lock_status;
    uint32_t authentication_status;
    uint32_t reserved_14[3];
    uint32_t debug_device_id;
    uint32_t debug_device_type;
    uint32_t debug_peripheral_id[8];
    uint32_t debug_component_id[4];
} PACKED debug_registers;
SIZE_CHECK_STRUCT(debug_registers, 4096);

void initialize_bp_patching();

#endif