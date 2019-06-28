#include <arch.h>
#include <stdlib.h>
#include <mpu.h>
#include <misc_utils.h>
#include <debug.h>
#include <bsp_sys.h>

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

#define MALLOC_MAGIC       0x75738910

typedef struct {
    int32_t magic;
    int32_t size;
    int32_t freeable;
} mchunk_t;

static int heap_size_total = -1;

#if !defined(BOOT)

static inline void
__heap_check_margin (int size)
{
    size = heap_size_total - size - sizeof(mchunk_t);
    if (size < 0) {
        fatal_error("__heap_check_margin : exceeds by %d bytes\n", -size);
    }
}

#else

static uint8_t *heap_user_mem_ptr = NULL;
static arch_word_t heap_user_size = 0;

#endif

static inline void *
__heap_malloc (int size, int freeable)
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
__heap_free (void *_p)
{
    mchunk_t *p = (mchunk_t *)_p;
    if (!_p) {
        return;
    }
    p = p - 1;
    if (!p->freeable) {
        fatal_error("__heap_free : chunk cannot be freed\n");
    }
    if (p->magic != MALLOC_MAGIC) {
        fatal_error("__heap_free : magic fail, expected= 0x%08x, token= 0x%08x\n",
                    MALLOC_MAGIC, p->magic);
    }
    heap_size_total += p->size;
    free(p);
}

static inline void *
__heap_realloc (void *x, int32_t size)
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
    __heap_free(x);
    return __heap_malloc(size, 1);
}

void heap_leak_check (void)
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

void heap_init (void)
{
    arch_word_t heap_mem, heap_size;
    arch_word_t sp_mem, sp_size;

    arch_get_stack(&sp_mem, &sp_size);
    arch_get_heap(&heap_mem, &heap_size);

#if defined(APPLICATION) || defined(BSP_DRIVER)
    mpu_lock(sp_mem, MPU_CACHELINE, "xwr");
    mpu_lock(heap_mem - MPU_CACHELINE, MPU_CACHELINE, "xwr");
    /*According to code below, heap must be as last partition in memory pool*/
    mpu_lock(heap_mem + heap_size, MPU_CACHELINE, "xwr");
#endif
    dprintf("%s() :\n", __func__);
    dprintf("stack : <0x%p> + %u bytes\n", (void *)sp_mem, sp_size);
    dprintf("heap : <0x%p> + %u bytes\n", (void *)heap_mem, heap_size);
    heap_size_total = heap_size - MPU_CACHELINE * 2;
#ifdef BOOT
    extern void __arch_user_heap (void *mem, void *size);

    __arch_user_heap(&heap_user_mem_ptr, &heap_user_size);
    dprintf("user heap : <0x%p> + %u bytes\n", (void *)heap_user_mem_ptr, heap_user_size);
#endif /*BOOT*/
}

void heap_deinit (void)
{
    heap_leak_check();
}

#ifdef BOOT

void *heap_alloc_shared (int _size)
{
    mchunk_t *p = NULL;
    int size = _size + sizeof(mchunk_t);
    if (heap_user_size < size) {
        return NULL;
    }
    p = (mchunk_t *)heap_user_mem_ptr;
    heap_user_mem_ptr += size;
    heap_user_size -= size;
    p->freeable = 0;
    p->magic = MALLOC_MAGIC;
    p->size = size;
    return p + 1;
}

#else /*BOOT*/

void *heap_alloc_shared (int size)
{
    __heap_check_margin(size);
    return __heap_malloc(size, 1);
}

#endif /*BOOT*/

int heap_avail (void)
{
    return (heap_size_total - sizeof(mchunk_t));
}

void *heap_malloc (int size)
{
    return __heap_malloc(size, 1);
}

void *heap_realloc (void *x, int32_t size)
{
    return __heap_realloc(x, size);
}

void *heap_calloc (int32_t size)
{
    void *p = __heap_malloc(size, 1);
    if (p) {
        memset(p, 0, size);
    }
    return p;
}

#ifdef BOOT

void heap_free (void *p)
{
    __heap_free(p);
}

#else /*BOOT*/

void heap_free (void *p)
{
    __heap_free(p);
}

#endif /*BOOT*/
