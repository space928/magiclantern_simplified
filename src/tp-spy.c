/** 
 * Attempt to intercept all TryPostEvent calls.
 * This file also allows hooks to be inserted into TryPostEvent calls.
 * 
 * Usage: 
 * 
 * 1) Make sure the cache hack is working.
 * For example, add this in boot-hack.c:
 *     // Make sure that our self-modifying code clears the cache
 *     clean_d_cache();
 *     flush_caches();
 *     + cache_lock();
 *
 * 
 * 2) call "tp_intercept" from "don't click me"
 * 
 **/

#include "tp-spy.h"
#include "dryos.h"
#include "bmp.h"
#include "reloc.h"
#include "version.h"

#if !(defined(CONFIG_DIGIC_V) || defined(CONFIG_DIGIC_VI)) //Digic 5-6 have this stuff inside RAM already
#include "cache_hacks.h"
#endif

unsigned int BUFF_SIZE = (1024*1024);
static char *tp_buf = 0;
static int tp_len = 0;
static uint32_t msg_no = 0;

static bool hook_installed = false;
static struct tp_hook hooks[MAX_TP_HOOKS];
static int installed_hooks = 0;

#if defined(CONFIG_DIGIC_VI)
// In Digic VI we can only hook TryPostEvent, and we avoid relocating code since Thumb relocation isn't implemented yet.
extern void TryPostEvent(void* taskClass, void* sender, uint32_t eventCode, void* msg, uint32_t arg);
// These methods are needed by the reimplementation of TryPostEvent
extern char* task_class_string;
extern void* _AllocateMemory(uint32_t size);
extern void _FreeMemory(void* ptr);
#else
extern thunk TryPostEvent;
extern thunk TryPostEvent_end;

extern thunk TryPostStageEvent;
extern thunk TryPostStageEvent_end;

#define reloc_start ((uintptr_t)&TryPostEvent)
#define reloc_end   ((uintptr_t)&TryPostEvent_end)
#define reloc_tp_len   (reloc_end - reloc_start)

#define reloc_start2 ((uintptr_t)&TryPostStageEvent)
#define reloc_end2   ((uintptr_t)&TryPostStageEvent_end)
#define reloc_tp_len2   (reloc_end2 - reloc_start2)

static uintptr_t reloc_tp_buf = 0;
static uintptr_t reloc_tp_buf2 = 0;

static int (*new_TryPostEvent)(void* taskClass, void* sender, uint32_t eventCode, void* msg, uint32_t arg) = 0;
static int (*new_TryPostStageEvent)(void* taskClass, void* sender, uint32_t eventCode, void* msg, uint32_t arg) = 0;
#endif

static char callstack[100];

char* get_call_stack()
{
    uintptr_t sp = 0;
    asm __volatile__ (
        "mov %0, %%sp"
        : "=&r"(sp)
    );
    
    callstack[0] = 0;
    for (int i = 0; i < 100; i++)
    {
        if ((MEM(sp+i*4) & 0xFF000000) == 0xFF000000)
        {
            STR_APPEND(callstack, "%x ", MEM(sp+i*4));
        }
    }
    return callstack;
}

// Instead of relocating Canon's TryPostEvent, we just reimplement it
#if defined(CONFIG_DIGIC_VI)
int orig_TryPostEvent(struct task_class* taskClass, void* sender, uint32_t eventCode, void* msg, uint32_t arg)
{
    if (taskClass->class_name != &task_class_string) 
        return 7;

    struct queue_msg* q_msg = _AllocateMemory(0x10);
    q_msg->sender = sender;
    q_msg->eventCode = eventCode;
    q_msg->msg = msg;
    q_msg->arg = arg;
    int res = msg_queue_post(taskClass->hQueue, (uint32_t)q_msg);
    if (res != 0) 
        _FreeMemory(q_msg);
    return res;
}
#endif

// Logs a TryPostEvent message to the log buffer
void tp_log_msg(struct task_class* taskClass, void* sender, uint32_t eventCode, void* msg, uint32_t arg, bool stage_event)
{
    #ifdef CONFIG_TP_SPY_FILTER
    if (event != CONFIG_TP_SPY_FILTER)
        return;
    #endif

    // It seems like the sender parameter isn't standard, sometimes the first 
    // arg of the sender object is a class name, other times is junk.
    //char* sender_name = MEM(sender);
    char* sender_name = "       ";

    if(stage_event)
        tp_len += snprintf(tp_buf + tp_len, BUFF_SIZE - tp_len, "%8d\tStageEvent\t%08x\t%08x\t%s\t%08x\t%08x\t%08x\n", 
                           ++msg_no, taskClass, sender, sender_name, eventCode, msg, arg);
    else
        tp_len += snprintf(tp_buf + tp_len, BUFF_SIZE - tp_len, "%8d\tEvent     \t%08x\t%08x\t%s\t%08x\t%08x\t%08x\n", 
                           ++msg_no, taskClass, sender, sender_name, eventCode, msg, arg);

    if(BUFF_SIZE - tp_len < 0x7f)
    {
        // Not much space left, lets wrap around and overwrite earlier messages
        tp_len = 0;
        // Write a little header to the log
        tp_len += snprintf(tp_buf, BUFF_SIZE, "### TryPostMessage Logging Start ###\n"
            "Magic Lantern Firmware version %s (%s)\n"
            "Built on %s by %s\n%s\n"
            "(Earlier messages have been overwritten)\n"
            "msg_no  \ttype    \ttaskclass\tsender  \tsender_name\tevent   \tmsg     \targ4    \n",
            build_version,
            build_id,
            build_date,
            build_user,
            "http://www.magiclantern.fm/"
        );
    }
}

// Replacement TryPostEvent which calls the logging code and any installed hooks before calling the original TryPostEvent method
int my_TryPostEvent(struct task_class* taskclass, void* obj, uint32_t event, void* arg3, uint32_t arg4)
{	
    if(tp_buf)
        tp_log_msg(taskclass, obj, event, arg3, arg4, false);

    // Invoke any hooks...
    for(int i = 0; i < installed_hooks; i++)
        (*hooks[i].func)(taskclass, obj, event, arg3, arg4);

    #ifdef CONFIG_DIGIC_VI
    return orig_TryPostEvent(taskclass, obj, event, arg3, arg4);
    #else
	return new_TryPostEvent(taskclass, obj, event, arg3, arg4);
    #endif
}

#if !defined(CONFIG_DIGIC_VI)
// Replacement TryPostStageEvent which calls the logging code before calling the original TryPostStageEvent method
int my_TryPostStageEvent(struct task_class* taskclass, void* obj, uint32_t event, void* arg3, uint32_t arg4)
{	
    if(tp_buf)
        tp_log_msg(taskclass, obj, event, arg3, arg4, true);

    return new_TryPostStageEvent(taskclass, obj, event, arg3, arg4);
}
#endif

// Installs the global TryPostMessage/TryPostStageMessage hooks
void tp_intercept()
{
    if(hook_installed)
        return;

    DebugMsg(DM_MAGIC, DM_LEVEL_INFO, "Installing TryPostMessage Hook...");

#if defined(CONFIG_DIGIC_VI)
    uint32_t d = ((uint32_t)&TryPostEvent) & (~1);
    *(uint32_t*)(d) = THUMB_B_INSTR((uint32_t)&TryPostEvent, my_TryPostEvent);

    // This method doesn't exist in RAM and as such can't currently be patched
    // uint32_t e = ((uint32_t)&TryPostStageEvent) & (~1);
    // *(uint32_t*)(e) = THUMB_B_INSTR((uint32_t)&TryPostStageEvent, my_TryPostStageEvent);
#else
    if (!reloc_tp_buf) reloc_tp_buf = (uintptr_t) malloc(reloc_tp_len + 64);
    if (!reloc_tp_buf2) reloc_tp_buf2 = (uintptr_t) malloc(reloc_tp_len2 + 64);

    new_TryPostEvent = (void *)reloc(
        0,      // we have physical memory
        0,      // with no virtual offset
        reloc_start,
        reloc_end,
        reloc_tp_buf
    );

    new_TryPostStageEvent = (void *)reloc(
        0,      // we have physical memory
        0,      // with no virtual offset
        reloc_start2,
        reloc_end2,
        reloc_tp_buf2
    );

    #if defined(CONFIG_DIGIC_V)
    uint32_t d = (uint32_t)&TryPostEvent;
    *(uint32_t*)(d) = B_INSTR((uint32_t)&TryPostEvent, my_TryPostEvent);

    uint32_t e = (uint32_t)&TryPostStageEvent;
    *(uint32_t*)(e) = B_INSTR((uint32_t)&TryPostStageEvent, my_TryPostStageEvent);
    #else
    cache_fake((uint32_t)&TryPostEvent, B_INSTR((uint32_t)&TryPostEvent, my_TryPostEvent), TYPE_ICACHE);
    cache_fake((uint32_t)&TryPostStageEvent, B_INSTR((uint32_t)&TryPostStageEvent, my_TryPostStageEvent), TYPE_ICACHE);
    #endif
#endif
    DebugMsg(DM_MAGIC, DM_LEVEL_INFO, "TryPostMessage Hook installed!");
    hook_installed = true;
}

// Begins TryPostEvent logging/saves the current log to disk
void tp_log()
{
    if (!tp_buf) // first call, intercept debug messages
    {
	    tp_buf = fio_malloc(BUFF_SIZE);
	    tp_len = 0;
        if(!tp_buf)
        {
            bmp_printf(FONT_LARGE, 0, 80, "Couldn't allocate %d bytes!", BUFF_SIZE);
            return;
        }

        // Write a little header to the log
        tp_len += snprintf(tp_buf, BUFF_SIZE, "### TryPostMessage Logging Start ###\n"
            "Magic Lantern Firmware version %s (%s)\n"
            "Built on %s by %s\n%s\n"
            "msg_no  \ttype    \ttaskclass\tsender  \tsender_name\tevent   \tmsg     \targ4    \n",
            build_version,
            build_id,
            build_date,
            build_user,
            "http://www.magiclantern.fm/"
        );

        if(!hook_installed)
            tp_intercept();

        NotifyBox(2000, "Now logging... ALL TryPostEvent's :)");
    } else // subsequent call, save log to file
    {
		tp_buf[tp_len] = 0;
        dump_seg(tp_buf, tp_len, "tp.log");
        NotifyBox(2000, "Saved %d bytes.", tp_len);
		tp_len = 0;
    }
}

void register_tp_hook(struct tp_hook hook)
{
    // Install the global hook if needed
    if(!hook_installed)
        tp_intercept();

    if(installed_hooks >= MAX_TP_HOOKS - 1)
    {
        DebugMsg(DM_MAGIC, DM_LEVEL_ERROR, "Couldn't install TryPostEvent hook '%s', too many hooks installed!", hook.name);
        return;
    }

    // Check if the hook is already installed
    for(int i = 0; i < installed_hooks; i++)
        if(hooks[i].name == hook.name)
            return;

    hooks[installed_hooks] = hook;
    installed_hooks++;
}

