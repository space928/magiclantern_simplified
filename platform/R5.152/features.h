#define FEATURE_VRAM_RGBA

//enable XCM only in full build
#ifndef ML_MINIMAL_OBJ
#define CONFIG_COMPOSITOR_XCM
#endif

#undef CONFIG_CRASH_LOG
#undef CONFIG_PROP_REQUEST_CHANGE
#undef CONFIG_AUTOBACKUP_ROM