#ifndef __MISC_UTILS_H__
#define __MISC_UTILS_H__

#include "dev_conf.h"

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

#ifndef boolean
#define boolean int
#define true 1
#define false 0
#endif

extern void fatal_error (char *message, ...);

#endif /*__MISC_UTILS_H__*/
