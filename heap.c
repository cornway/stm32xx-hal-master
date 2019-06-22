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

static int heap_size_total = -1;
static uint8_t *heap_user_mem_ptr = NULL;
static arch_word_t heap_user_size = 0;

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
        return NULL;
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

static inline void *
heap_realloc (void *x, int32_t size)
{
    mchunk_t *p = (mchunk_t *)x;
    if (!p) {
        return NULL;
    }
    p = p - 1;
    if (p->size >= size) {
        return x;
    }
    if (heap_size_total < size) {
        fatal_error("%s() : size= %d, avail= %d\n",
            __func__, size, heap_size_total);
    }
    assert(p->freeable);
    heap_free(x);
    return heap_malloc(size, 1);
}

void Sys_AllocInit (void)
{
    arch_word_t heap_mem, heap_size;
    arch_word_t sp_mem, sp_size;

    arch_get_stack(&sp_mem, &sp_size);
    arch_get_heap(&heap_mem, &heap_size);

    mpu_lock(sp_mem, MPU_CACHELINE, "xwr");
    mpu_lock(heap_mem - MPU_CACHELINE, MPU_CACHELINE, "xwr");
    /*According to code below, heap must be as last partition in memory pool*/
    mpu_lock(heap_mem + heap_size, MPU_CACHELINE, "xwr");

    dprintf("%s() :\n", __func__);
    dprintf("stack : <0x%p> + %u bytes\n", (void *)sp_mem, sp_size);
    dprintf("heap : <0x%p> + %u bytes\n", (void *)heap_mem, heap_size);
    heap_size_total = heap_size - MPU_CACHELINE * 2;
#ifdef BOOT 
    extern void __arch_user_heap (void *mem, void *size);

    __arch_user_heap(&heap_user_mem_ptr, &heap_user_size);
#endif
}

void Sys_AllocDeInit (void)
{
    arch_word_t heap_mem, heap_size, heap_size_left;

    dprintf("%s() :\n", __func__);
    arch_get_heap(&heap_mem, &heap_size);

    heap_size_left = heap_size - MPU_CACHELINE * 2 - heap_size_total;
    assert(heap_size_left <= heap_size);
    if (heap_size_left) {
        dprintf("%s() : Unfreed left : %u bytes\n", __func__, heap_size_left);
    }
}

#ifdef BOOT

void *Sys_AllocShared (int *size)
{
    mchunk_t *p = NULL;
    int _size = *size + sizeof(mchunk_t);
    if (heap_user_size < *size) {
        return NULL;
    }
    p = (mchunk_t *)heap_user_mem_ptr;
    heap_user_mem_ptr += _size;
    heap_user_size -= _size;
    p->freeable = 0;
    p->magic = MALLOC_MAGIC;
    p->size = _size;
    return p - 1;
}

#else

void *Sys_AllocShared (int *size)
{
    heap_check_margin(*size);
    return heap_malloc(*size, 1);
}

#endif /*BOOT*/

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

void *Sys_Realloc (void *x, int32_t size)
{
    return heap_realloc(x, size);
}

void *Sys_Calloc (int32_t size)
{
    void *p = heap_malloc(size, 1);
    if (p) {
        memset(p, 0, size);
    }
    return p;
}

#ifdef BOOT

void Sys_Free (void *p)
{
    heap_free(p);
}

#else

void Sys_Free (void *p)
{
    heap_free(p);
}

#endif

#else /*DATA_IN_ExtSDRAM*/

#error "Not supported"

#endif /*DATA_IN_ExtSDRAM*/
