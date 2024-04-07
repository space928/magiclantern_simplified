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

    // TODO: There appear to be some concurrency issues, unfortunately we can't seem to use semaphores here...

    const char* class_name = NULL;
    #if 0 && defined(CONFIG_DIGIC_VI)
    // Not tested on other versions, but an earlier comment indicated it might not work?
    // This might not be accurate for class IDs >= 0x54
    class = class & 0xff;
    if (class != 0 && class < 197)
        class_name = dm_names[class];
    #endif

    const char* level_str = "     ";
    if(level == 6)
        level_str = "ERROR";
    else if (level == 0x26)
        level_str = "WARN ";

    if(class_name == NULL)
        len += snprintf(buf+len, BUF_SIZE-len, "%8d\t%03d\t%s\t", ++msg_no, class, level_str);
    else
        len += snprintf(buf+len, BUF_SIZE-len, "%8d\t%s\t%s\t", ++msg_no, class_name, level_str);

    va_start( ap, fmt );
    len += vsnprintf(buf+len, BUF_SIZE-len-1, fmt, ap);
    va_end( ap );

    len += snprintf(buf+len, BUF_SIZE-len, "\n");

    if(BUF_SIZE - len < 0x7f)
    {
        // Not much space left, lets wrap around and overwrite earlier messages
        len = 0;
        // Write a little header to the log
        len += snprintf(buf, BUF_SIZE, "### DebugMsg Logging Start ###\n"
            "Magic Lantern Firmware version %s (%s)\n"
            "Built on %s by %s\n%s\n"
            "(Earlier messages have been overwritten)\n"
            "msg_no  \tclass\tlevel\tmsg\n",
            build_version,
            build_id,
            build_date,
            build_user,
            "http://www.magiclantern.fm/"
        );
    }
}

// call this from "don't click me"
void debug_intercept()
{
    if (!buf) // first call, intercept debug messages
    {
        buf = fio_malloc(BUF_SIZE);
        if(!buf)
        {
            bmp_printf(FONT_LARGE, 0, 80, "Couldn't allocate %d bytes!", BUF_SIZE);
            return;
        }

        // Write a little header to the log
        len += snprintf(buf, BUF_SIZE, "### DebugMsg Logging Start ###\n"
            "Magic Lantern Firmware version %s (%s)\n"
            "Built on %s by %s\n%s\n"
            "msg_no  \tclass\tlevel\tmsg\n",
            build_version,
            build_id,
            build_date,
            build_user,
            "http://www.magiclantern.fm/"
        );
        
        #if defined(CONFIG_DIGIC_V)
        uint32_t d = (uint32_t)&DryosDebugMsg;
        *(uint32_t*)(d) = B_INSTR((uint32_t)&DryosDebugMsg, my_DebugMsg);
        #elif defined(CONFIG_DIGIC_VI)
        uint32_t d = ((uint32_t)&DryosDebugMsg) & (~1);

        // Generate a Thumb b.w instruction to branch to my_DebugMsg and 
        // patch DryosDebugMsg
        *(uint32_t*)(d) = THUMB_B_INSTR((uint32_t)&DryosDebugMsg, my_DebugMsg);
        #else
        cache_fake((uint32_t)&DryosDebugMsg, B_INSTR((uint32_t)&DryosDebugMsg, my_DebugMsg), TYPE_ICACHE);
        #endif

        // Send a test message to the log
        DryosDebugMsg(0, 0, "DebugMsg hook installed successfully!");

        bmp_printf(FONT_LARGE, 0, 80, "Hook installed!");
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

