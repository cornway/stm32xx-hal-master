#include <arch.h>
#include <stdlib.h>
#include "dev_conf.h"
#include "main.h"
#include "gfx.h"
#include <mpu.h>

#ifdef __MICROLIB
#error "I don't want to use microlib"
/*Microlib will reduce code size at the cost of performance,
    also due to HEAP replacement
*/
#endif

#ifdef USE_STM32F769I_DISCO

#include "stm32f769i_discovery_lcd.h"

#else /*USE_STM32F769I_DISCO*/

#error "Not supported"

#endif /*USE_STM32F769I_DISCO*/

#ifdef DATA_IN_ExtSDRAM

#define MALLOC_MAGIC       0x75738910

typedef struct {
    int32_t magic;
    int32_t size;
    int32_t freeable;
} mchunk_t;

static int heap_size_total;

static inline void
heap_check_margin (int size)
{
    size = heap_size_total - size - sizeof(mchunk_t);
    if (size < 0) {
        fatal_error("heap_check_margin : exceeds by %d bytes\n", -size);
    }
}

static inline void *
heap_malloc (int size, int freeable)
{
    size = size + sizeof(mchunk_t);
    mchunk_t *p;

    p = (mchunk_t *)malloc(size);
    if (!p) {
        fatal_error("heap_malloc : no free space left\n");
    }
    heap_size_total -= size;
    p->magic = MALLOC_MAGIC;
    p->freeable = freeable;
    p->size = size;
    return (void *)(p + 1);

}

static inline void
heap_free (void *_p)
{
    mchunk_t *p = (mchunk_t *)_p;
    if (!_p) {
        return;
    }
    p = p - 1;
    if (!p->freeable) {
        fatal_error("heap_free : chunk cannot be freed\n");
    }
    if (p->magic != MALLOC_MAGIC) {
        fatal_error("heap_free : magic fail, expected= 0x%08x, token= 0x%08x\n",
                    MALLOC_MAGIC, p->magic);
    }
    heap_size_total += p->size;
    free(p);

}

void Sys_AllocInit (void)
{
    arch_word_t heap_mem, heap_size;
    arch_word_t sp_mem, sp_size;

    arch_get_stack(&sp_mem, &sp_size);
    arch_get_heap(&heap_mem, &heap_size);

    heap_size_total = heap_size;
    mpu_lock(sp_mem, 32, "xwr");
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
    return (heap_size_total - sizeof(mchunk_t));
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

#error "Not supported"

#endif /*DATA_IN_ExtSDRAM*/
