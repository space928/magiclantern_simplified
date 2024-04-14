#ifndef _tp_spy_h_
#define _tp_spy_h_

#include <stdint.h>

// An arbitrary limit, raise it as needed...
#define MAX_TP_HOOKS 8

struct queue_msg
{
    void* sender;
    uint32_t eventCode;
    void* msg;
    uint32_t arg;
};

struct task_class
{
    char** class_name;
    char* task_name;
    uint32_t unk_0x8;
    uint32_t hTask;
    struct msg_queue* hQueue;
    uint32_t unk_0x14;
};

typedef void (*TryPostEvent_handler)(struct task_class* taskClass, void* sender, uint32_t eventCode, void* msg, uint32_t arg);

struct tp_hook
{
    char* name;
    TryPostEvent_handler func;
};

/* Inserts the hook into TryPostEvent/TryPostStageEvent; This is called 
   automatically when either tp_log() or register_tp_hook() is called.*/
void tp_intercept();
// Starts/Saves a log of all TryPostEvent calls
void tp_log();
// Registers a new TryPostEvent hook
void register_tp_hook(struct tp_hook hook);

#endif //_tp_spy_h_
