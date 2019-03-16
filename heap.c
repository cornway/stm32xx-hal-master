#include <stdlib.h>
#include "main.h"
#include "gfx.h"

#ifdef __MICROLIB
#error "I don't want to use microlib"
/*Microlib will reduce code size at the cost of performance,
    also due to HEAP replacement
*/
#endif

#ifdef USE_STM32F769I_DISCO

#include "stm32f769i_discovery_lcd.h"

#define SDRAM_VOL_START 0xC0000000
#define SDRAM_VOL_END   0xC1000000

#else /*USE_STM32F769I_DISCO*/

#error "Not supported"

#endif /*USE_STM32F769I_DISCO*/

#define HEAP_CACHE_SIZE (0x0007ff00)

static uint8_t *__heap_buf_cache;
static uint8_t *__heap_buf_cache_top;
static size_t __heap_buf_cache_size;

#define SDRAM_VOL_SIZE      (SDRAM_VOL_END - SDRAM_VOL_START)
#define HEAP_CACHE_OFFSET   (SDRAM_VOL_START + BSP_LCD_FB_MEM_SIZE_MAX)

#define HEAP_BUF_OFFSET (HEAP_CACHE_OFFSET + HEAP_CACHE_SIZE)
#define HEAP_BUF_SIZE   (SDRAM_VOL_SIZE - BSP_LCD_FB_MEM_SIZE_MAX - HEAP_CACHE_SIZE);

#ifdef DATA_IN_ExtSDRAM

#define HEAP_MALLOC_MARGIN 0x1000

typedef struct {
    int size;
    int freeable;
} mchunk_t;

static int heap_size_total = HEAP_BUF_SIZE;

static inline void
heap_check_margin (int size)
{
    size = heap_size_total - HEAP_MALLOC_MARGIN - size - sizeof(mchunk_t);
    if (size < 0) {
        fatal_error("");
    }
}

static inline void *
heap_malloc (int size, int freeable)
{
    size = size + sizeof(mchunk_t);
    mchunk_t *p;

    p = (mchunk_t *)malloc(size);
    if (!p) {
        fatal_error("");
    }
    heap_size_total -= size;
    p->freeable = freeable;
    p->size = size;
    return (void *)(p + 1);

}

static inline void
heap_free (void *_p)
{
    mchunk_t *p = (mchunk_t *)_p;
    
    p = p - 1;
    if (!p->freeable) {
        fatal_error("");
    }
    heap_size_total += p->size;
    free(p);

}

void Sys_AllocInit (void)
{
    __heap_buf_cache_size    = (HEAP_CACHE_SIZE);
    __heap_buf_cache          = malloc(__heap_buf_cache_size);
    __heap_buf_cache_top      = __heap_buf_cache + __heap_buf_cache_size;
    heap_size_total = heap_size_total - HEAP_CACHE_SIZE;
}

void *Sys_AllocShared (int *size)
{
    heap_check_margin(*size);
    return heap_malloc(*size, 1);
}

void *Sys_AllocVideo (int *size)
{
    return Sys_AllocShared(size);
}

int Sys_AllocBytesLeft (void)
{
    return (heap_size_total - HEAP_MALLOC_MARGIN - sizeof(mchunk_t));
}

void *Sys_Malloc (int size)
{
    return heap_malloc(size, 1);
}

void Sys_Free (void *p)
{
    heap_free(p);
}

#else /*DATA_IN_ExtSDRAM*/

void Sys_AllocInit (void)
{
    __heap_buf_cache          = (void *)(HEAP_CACHE_OFFSET);
    __heap_buf_cache_top      = (void *)(HEAP_CACHE_OFFSET + HEAP_CACHE_SIZE);
    __heap_buf_cache_size    = (HEAP_CACHE_SIZE);
}

void *Sys_AllocShared (int *size)
{
    *size = HEAP_BUF_SIZE;
    return (void *)HEAP_BUF_OFFSET;
}

void *Sys_AllocVideo (int *size)
{
    *size = BSP_LCD_FB_MEM_SIZE_MAX;
    return (void *)SDRAM_VOL_START;
}

int Sys_AllocBytesLeft (void)
{
    return HEAP_BUF_SIZE;
}

void *Sys_Malloc (int size)
{
    return malloc(*size);
}

void Sys_Free (void *p)
{
    free(p);
}

#endif /*DATA_IN_ExtSDRAM*/

void *Sys_HeapCacheTop (int size)
{
    if (__heap_buf_cache_size < size) {
        fatal_error("");
    }

    __heap_buf_cache_top -= size;
    __heap_buf_cache_size -= size;
    return __heap_buf_cache_top;
}


void *Sys_HeapCachePop (int size)
{
    void *p;
    if (__heap_buf_cache_size < size) {
        fatal_error("");
    }
    p = __heap_buf_cache;
    __heap_buf_cache += size;
    __heap_buf_cache_size -= size;
    return p;
}

void Sys_HeapCachePush (int size)
{
    __heap_buf_cache -= size;
    __heap_buf_cache_size += size;
}

