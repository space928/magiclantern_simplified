#define FEATURE_VRAM_RGBA

// Don't Click Me menu looks to be intended as a place
// for devs to put custom code in debug.c run_test(),
// and allowing triggering from a menu context.
#define FEATURE_DONT_CLICK_ME
#define CONFIG_HEXDUMP
#define CONFIG_DEBUG_INTERCEPT
// Use this to listen to only specific DebugMsg classes in dm_spy
#define CONFIG_DM_SPY_FILTER DM_MAGIC // 0x2f // MVR
//#define CONFIG_DEBUGMSG 1

#define FEATURE_SHOW_SHUTTER_COUNT

// working but incomplete, some allocators don't report
// anything yet as they're faked / not yet found
#define FEATURE_SHOW_FREE_MEMORY

#define CONFIG_ADDITIONAL_VERSION
#define FEATURE_SCREENSHOT

#define CONFIG_TSKMON
#define FEATURE_SHOW_TASKS
#define FEATURE_SHOW_CPU_USAGE
#define FEATURE_SHOW_GUI_EVENTS

// enable global draw
#define FEATURE_GLOBAL_DRAW
#define FEATURE_CROPMARKS

#define CONFIG_PROP_REQUEST_CHANGE
#define CONFIG_STATE_OBJECT_HOOKS
#define CONFIG_LIVEVIEW
#define FEATURE_POWERSAVE_LIVEVIEW

// explicitly disable stuff that don't work or may break things
#undef CONFIG_CRASH_LOG
#undef CONFIG_AUTOBACKUP_ROM
