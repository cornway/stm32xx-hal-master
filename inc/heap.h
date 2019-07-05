#ifndef __HEAP_H__
#define __HEAP_H__

#include <stdint.h>

#define ALIGN(x) __attribute__((aligned(x)))

#define SDRAM __attribute__ ((section ("dram")))
#define DTCM __attribute__ ((section ("dtcm")))
#define IRAM __attribute__ ((section ("iram")))
#define IRAM2 __attribute__ ((section ("iram2")))

typedef struct bsp_heap_api_s {
     void *(*malloc) (int size);
     void (*free) (void *p);
} bsp_heap_api_t;

void heap_init (void);
void *heap_alloc_shared (int size);
int heap_avail (void);
void *heap_malloc (int size);
void *heap_realloc (void *x, int32_t size);
void *heap_calloc (int32_t size);
void heap_free (void *p);

#endif /*__HEAP_H__*/
