
#include <main.h>
#include <misc_utils.h>

static inline void
d_memcpy_8 (uint8_t *dst, uint8_t *src, int cnt)
{
    while (cnt--) { *dst++ = *src++; }
}

static inline void
d_memcpy_16 (uint16_t *dst, uint16_t *src, int cnt)
{
    while (cnt--) { *dst++ = *src++; }
}

static inline void
d_memcpy_32 (uint32_t *dst, uint32_t *src, int cnt)
{
    while (cnt--) { *dst++ = *src++; }
}

void d_memcpy (void *_dst, const void *_src, int cnt)
{
    arch_word_t wdest = (arch_word_t)_dst,
                wsrc  = (arch_word_t)_src;

    const arch_word_t wsizeof = (sizeof(arch_word_t));
    const arch_word_t align_ms = (wsizeof - 1);
    uint32_t copy32 = 0;

    if (cnt <= wsizeof) {
    } if (((wdest | wsrc) & align_ms) == 0) {
        copy32 = ROUND_DOWN(cnt, wsizeof);

        d_memcpy_32((uint32_t *)_dst, (uint32_t *)_src, copy32 / wsizeof);
    } else if (((wdest | wsrc) & (align_ms >> 1)) == 0) {
        d_memcpy_16((uint16_t *)_dst, (uint16_t *)_src, copy32 / (wsizeof / 2));
    }
    d_memcpy_8((uint8_t *)_dst + copy32, (uint8_t *)_src + copy32, cnt - copy32);
}

void d_memset (void *_dst, int v, int cnt)
{
    uint8_t *dst = (uint8_t *)_dst;
    while (cnt--) {
        *dst++ = 0;
    }
}


