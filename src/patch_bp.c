/*
   Memory patching for Digic VI using ARM Debug registers and breakpoints.
*/

#include "patch_bp.h"
#include "arm-mcr.h"
#include "stdbool.h"
#include "debug.h"
#include "dryos.h"

static bool initialized = false;
static volatile debug_registers* dbg_registers = NULL;

uint32_t get_debug_rom_addr()
{
    uint32_t ret;
    asm (
        "MRC     p14, 0, %0, c1, c0, 0"
        : "=r" (ret)
    );
    return ret;
}

uint32_t get_debug_self_addr_offset()
{
    uint32_t ret;
    asm (
        "MRC     p14, 0, %0, c2, c0, 0"
        : "=r" (ret)
    );
    return ret;
}

void get_debug_info(debug_info* info)
{
    /*
    On a 750D this returns:
        Rev: 3
        Variant: 1
        Unused: 0
        Security Extensions: 0
        PC Sampling: 0
        Secure User Halt: 1
        DEVID Implemented: 0
        Version: ARM_V7_BASE_DBG
        Num Context Compares: 0
        Num BP Reg Pairs: 7
        Num WP Reg Pairs: 7
    */
    debug_info_u tmp;
    asm volatile(
        "MRC     p14, 0, %0, c0, c0, 0\n"
        : "=r" (tmp.u)
    );
    *info = tmp.s;
}

void initialize_bp_patching()
{
    if(initialized)
        return;

    debug_info info;
    get_debug_info(&info);
    uint32_t romBase = get_debug_rom_addr();
    uint32_t dbgOffset = get_debug_self_addr_offset();

    DryosDebugMsg(DM_MAGIC, 0, "Debug Register info:");
    DryosDebugMsg(DM_MAGIC, 0, "\tRev: %d", info.revision);
    DryosDebugMsg(DM_MAGIC, 0, "\tVariant: %d", info.variant);
    DryosDebugMsg(DM_MAGIC, 0, "\tUnused: %d", info.unused);
    DryosDebugMsg(DM_MAGIC, 0, "\tSecurity Extensions: %d", info.security_ex_imp);
    DryosDebugMsg(DM_MAGIC, 0, "\tPC Sampling: %d", info.pc_sampling_reg_imp);
    DryosDebugMsg(DM_MAGIC, 0, "\tSecure User Halt: %d", !info.n_secure_user_halt_imp);
    DryosDebugMsg(DM_MAGIC, 0, "\tDEVID Implemented: %d", info.devid_imp);
    DryosDebugMsg(DM_MAGIC, 0, "\tVersion: %d", info.version);
    DryosDebugMsg(DM_MAGIC, 0, "\tNum Context Compares: %d", info.num_ctx_cmps);
    DryosDebugMsg(DM_MAGIC, 0, "\tNum BP Reg Pairs: %d", info.num_breakpoint_reg_pairs);
    DryosDebugMsg(DM_MAGIC, 0, "\tNum WP Reg Pairs: %d", info.num_watchpoint_reg_pairs);

    DryosDebugMsg(DM_MAGIC, 0, "\tDebug ROM Address: 0x%08x", romBase);
    DryosDebugMsg(DM_MAGIC, 0, "\tDebug Self Address Offset: 0x%08x", dbgOffset);

    if(info.version != ARM_V7_BASE_DBG 
        || info.security_ex_imp 
        || info.num_breakpoint_reg_pairs != 7 
        || info.num_watchpoint_reg_pairs != 7)
    {
        DryosDebugMsg(DM_MAGIC, DM_LEVEL_ERROR, "Processor does not support the necessary debugging features!");
        return;
    }
    if((romBase & 3) != 3)
    {
        DryosDebugMsg(DM_MAGIC, DM_LEVEL_ERROR, "Debug ROM Base Address is invalid!");
        return;
    }
    if((dbgOffset & 3) != 3)
    {
        DryosDebugMsg(DM_MAGIC, DM_LEVEL_ERROR, "Debug ROM Base Address is invalid!");
        return;
    }

    dbg_registers = (debug_registers*)((romBase & 0xFFFFF000) + (dbgOffset & 0xFFFFF000));
    DryosDebugMsg(DM_MAGIC, 0, "Debug Register Map Data:");
    DryosDebugMsg(DM_MAGIC, 0, "\taddress: 0x%08x", (uint32_t)dbg_registers);
    DryosDebugMsg(DM_MAGIC, 0, "\tdev_type: %d", dbg_registers->debug_device_type);  // Expect 0x15 ?
    DryosDebugMsg(DM_MAGIC, 0, "\tid: %x == %x", ((debug_info_u)dbg_registers->debug_id).u, ((debug_info_u)info).u);  // These should be equal
    DryosDebugMsg(DM_MAGIC, 0, "\tproc_id: %d", dbg_registers->processor_identification[0]);  // Main ID Register
    DryosDebugMsg(DM_MAGIC, 0, "\tauth: %d", dbg_registers->authentication_status);  // Expect 0xA0 (Debug enabled) or 0xF0 (Debug disabled)
    DryosDebugMsg(DM_MAGIC, 0, "\tstatus: %d", dbg_registers->debug_status_and_control); 

    DryosDebugMsg(DM_MAGIC, 0, "\tbp_0: %d", dbg_registers->bp_control[0]);
    DryosDebugMsg(DM_MAGIC, 0, "\tbp_1: %d", dbg_registers->bp_control[1]);
    DryosDebugMsg(DM_MAGIC, 0, "\tbp_2: %d", dbg_registers->bp_control[2]);
    DryosDebugMsg(DM_MAGIC, 0, "\tbp_3: %d", dbg_registers->bp_control[3]);
    DryosDebugMsg(DM_MAGIC, 0, "\tbp_4: %d", dbg_registers->bp_control[4]);
    DryosDebugMsg(DM_MAGIC, 0, "\tbp_5: %d", dbg_registers->bp_control[5]);
    DryosDebugMsg(DM_MAGIC, 0, "\tbp_6: %d", dbg_registers->bp_control[6]);

    //dump_seg();

    initialized = true;
}
