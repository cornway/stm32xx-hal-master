#ifndef __MISC_UTILS_H__
#define __MISC_UTILS_H__

#include <dev_conf.h>
#include <arch.h>
#include <stdint.h>

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

#ifndef boolean
#define boolean int
#define true 1
#define false 0
#endif

#define ATTR_UNUSED __attribute__((unused))

extern void fatal_error (char *message, ...);

extern void Sys_AllocInit (void);
extern void *Sys_AllocShared (int *size);
extern void *Sys_AllocVideo (int *size);
extern int Sys_AllocBytesLeft (void);
extern void *Sys_Malloc (int size);
extern void *Sys_Realloc (void *x, int32_t size);
extern void *Sys_Calloc (int32_t size);
extern void Sys_Free (void *p);
extern void *Sys_HeapCacheTop (int size);
extern void *Sys_HeapCachePop (int size);
extern void Sys_HeapCachePush (int size);


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

#else /*__LITTLE_ENDIAN__*/

#endif /*__LITTLE_ENDIAN__*/

#endif /*__MISC_UTILS_H__*/
