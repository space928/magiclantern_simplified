/** 
 * Attempt to intercept all Canon debug messages by overriding DebugMsg call with cache hacks
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
 * 2) call "debug_intercept" from "don't click me"
 * 
 **/

#include "dm-spy.h"
#include "dryos.h"
#include "bmp.h"
#include "version.h"

#if !(defined(CONFIG_DIGIC_V) || defined(CONFIG_DIGIC_VI)) // D5-6 have this stuff in RAM already
#include "cache_hacks.h"
#endif

unsigned int BUF_SIZE = (1024*1024);
static char* buf = 0;
static int len = 0;
static uint32_t msg_no = 0;

void my_DebugMsg(int class, int level, char* fmt, ...)
{
    if (!buf) return;
        
    if (class == 21) // engio
        return;

    va_list            ap;

    // not quite working due to concurrency issues
    // semaphores don't help, camera locks
    // same thing happens with recursive locks
    // cli/sei don't lock the camera, but didn't seem to help
    
    //~ len += snprintf( buf+len, MIN(10, BUF_SIZE-len), "%s%d ", dm_names[class], level );

    // char* msg = buf+len;

    va_start( ap, fmt );
    len += vsnprintf( buf+len, BUF_SIZE-len-1, fmt, ap );
    va_end( ap );

    len += snprintf( buf+len, BUF_SIZE-len, "\n%8d\t", ++msg_no);
    
    //~ static int y = 0;
    //~ bmp_printf(FONT_SMALL, 0, y, "%s\n                                                               ", msg);
    //~ y += font_small.height;
    //~ if (y > 450) y = 0;
}

// call this from "don't click me"
void debug_intercept()
{
    if (!buf) // first call, intercept debug messages
    {
        bmp_draw_rect(COLOR_BLACK, 0, 80, 600, 80);
        bmp_draw_rect(COLOR_BLACK, 0, 80, 600, 80);
        msleep(200);
        buf = fio_malloc(BUF_SIZE);
        
        #if defined(CONFIG_DIGIC_V) || defined(CONFIG_DIGIC_VI)
        uint32_t d = ((uint32_t)&DryosDebugMsg) & (~1);

        bmp_printf(FONT_SMALL, 0, 80, "Hooking DebugMsg @ 0x%08x; my_DebugMsg = 0x%08x; patch = 0x%08x; buf = 0x%08x...", 
            (uint32_t)&DryosDebugMsg, (uint32_t)&my_DebugMsg, THUMB_B_INSTR((uint32_t)&DryosDebugMsg, my_DebugMsg), (uint32_t)buf);

        // Generate a Thumb b.w instruction to branch to my_DebugMsg and 
        // patch DryosDebugMsg
        *(uint32_t*)(d) = THUMB_B_INSTR((uint32_t)&DryosDebugMsg, my_DebugMsg);
        // Send a test message to the log
        DryosDebugMsg(31, 14, "## DebugMsg Logging Start ##\n"
            "  Magic Lantern Firmware version %s (%s)\n  Built on %s by %s\n%s",
            build_version,
            build_id,
            build_date,
            build_user,
            "http://www.magiclantern.fm/"
        );

        bmp_printf(FONT_LARGE, 0, 80, "Hook installed!", 
            (uint32_t)&DryosDebugMsg, (uint32_t)&my_DebugMsg, (uint32_t)buf);
        #else
        cache_fake((uint32_t)&DryosDebugMsg, B_INSTR((uint32_t)&DryosDebugMsg, my_DebugMsg), TYPE_ICACHE);
        #endif
    }
    else // subsequent call, save log to file
    {
        buf[len] = 0;
        dump_seg(buf, len, "dm.log");
        NotifyBox(2000, "Saved %d bytes.", len);
        len = 0;
    }
    info_led_blink(1, 100, 50);
}

