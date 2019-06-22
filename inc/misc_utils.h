#ifndef __MISC_UTILS_H__
#define __MISC_UTILS_H__

#include <dev_conf.h>
#include <arch.h>
#include <stdint.h>
#include <bsp_api.h>

enum {
    DBG_OFF,
    DBG_ERR,
    DBG_WARN,
    DBG_INFO,
};

extern int g_dev_debug_level;
#define DEV_DBG_LVL (g_dev_debug_level)

#define dbg_eval(lvl) \
    if (DEV_DBG_LVL >= (lvl))

#ifndef assert
#define assert(exp) \
{ if (!(exp)) fatal_error("assertion failed! : %s() : \"%s\"\n", __func__, #exp); }
#endif

#ifndef arrlen
#define arrlen(a) sizeof(a) / sizeof(a[0])
#endif

#ifndef howmany
#define howmany(a, b) (((a) + (b) - 1) / (b))
#endif

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#ifndef container_of
#define container_of(ptr, type, member)     \
    (type *)((uint8_t *)(ptr) - offsetof(type,member))
#endif

#ifndef sizeof_member
#define sizeof_member(type, member) \
    sizeof(((type *)0)->member)
#endif

#ifndef A_COMPILE_TIME_ASSERT
#define A_COMPILE_TIME_ASSERT(name, x) typedef int A_dummy_ ## name[(x) * 2 - 1]
#endif

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#define GET_PAD(x, a) ((a) - ((x) % (a)))
#define ROUND_UP(x, a) ((x) + GET_PAD(a, x))
#define ROUND_DOWN(x, a) ((x) - ((a) - GET_PAD(a, x)))

#ifndef d_bool
#define d_bool int
#define d_true 1
#define d_false 0
#endif

#define ATTR_UNUSED __attribute__((unused))

static inline void d_memcpy(void *_dst, const void *_src, int cnt)
{
    uint8_t *src = (uint8_t *)_src, *dst = (uint8_t *)_dst;
    while (cnt--) {
        *dst++ = *src++;
    }
}

#ifdef __LITTLE_ENDIAN__

static inline void
writeShort (void *_buf, unsigned short v)
{
    uint8_t *buf = (uint8_t *)_buf;
    buf[0] = v & 0xff;
    buf[1] = v >> 8;
}

static inline void
writeLong (void *_buf, unsigned long v)
{
    uint8_t *buf = (uint8_t *)_buf;
    buf[0] = v & 0xff;
    buf[1] = v >> 8;
    buf[2] = v >> 16;
    buf[3] = v >> 24;
}

static inline void
writePtr (void *_buf, void *v)
{
    writeLong(_buf, (unsigned long)v);
}

static inline short
readShort (const void *_p)
{
extern short asmread16 (const void *);
    return asmread16(_p);
}

static inline long
readLong (const void *_p)
{
extern long asmread32 (const void *);
    return asmread32(_p);
}

static inline void *
readPtr (const void *_p)
{
    return (void *)readLong(_p);
}

#else /*__LITTLE_ENDIAN__*/

#endif /*__LITTLE_ENDIAN__*/

#if BSP_INDIR_API

#define fatal_error   g_bspapi->sys.fatal
#define _profiler_enter   g_bspapi->sys.prof_enter
#define _profiler_exit   g_bspapi->sys.prof_exit
#define profiler_reset   g_bspapi->sys.prof_reset
#define profiler_init   g_bspapi->sys.prof_init

#else
extern void fatal_error (char *message, ...);

extern void Sys_AllocInit (void);
extern void *Sys_AllocShared (int *size);
extern void *Sys_AllocVideo (int *size);
extern int Sys_AllocBytesLeft (void);
extern void *Sys_Malloc (int size);
extern void *Sys_Realloc (void *x, int32_t size);
extern void *Sys_Calloc (int32_t size);
extern void Sys_Free (void *p);

void _profiler_enter (const char *func, int line);
void _profiler_exit (const char *func, int line);
void profiler_reset (void);
void profiler_init (void);
#endif

#define profiler_enter() _profiler_enter(__func__, __LINE__)
#define profiler_exit() _profiler_exit(__func__, __LINE__)


#endif /*__MISC_UTILS_H__*/
