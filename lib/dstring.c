
#include <main.h>
#include <misc_utils.h>

#define ROUND_DOWN_BIN(x, a)    ((x) & ~((a) - 1))

#define D_DECL_MEMCPY(type, twidth)                     \
static inline void                                      \
d_memcpy_##twidth (void *_dst, const void *_src, int cnt, int offset)\
{                                                       \
    type *dst = (type *)_dst + offset;                           \
    const type *src = (const type *)_src + offset;               \
    while ((cnt--) - offset > 0) { *dst++ = *src++; }                  \
}

D_DECL_MEMCPY(uint8_t, 8);
D_DECL_MEMCPY(uint16_t, 16);
D_DECL_MEMCPY(uint32_t, 32);

void d_memcpy (void *_dst, const void *_src, int cnt)
{
    arch_word_t wdest = (arch_word_t)_dst,
                wsrc  = (arch_word_t)_src;

    const arch_word_t wsizeof = (sizeof(arch_word_t));
    const arch_word_t align_ms = (wsizeof - 1);

    if (cnt <= wsizeof) {
        d_memcpy_8(_dst, _src, cnt, 0);
    } else if (((wdest | wsrc) & align_ms) == 0) {
        uint32_t copylen = ROUND_DOWN_BIN(cnt, wsizeof);

        d_memcpy_32(_dst, _src, copylen / wsizeof, 0);
        d_memcpy_8(_dst, _src, cnt, copylen);
    } else if (((wdest | wsrc) & (align_ms >> 1)) == 0) {
        uint32_t copylen = ROUND_DOWN_BIN(cnt, wsizeof / 2);

        d_memcpy_16(_dst, _src, copylen / (wsizeof / 2), 0);
        d_memcpy_8(_dst, _src, cnt, copylen);
    } else {
        d_memcpy_8(_dst, _src, cnt, 0);
    }
    
}

void d_memset (void *_dst, int v, int cnt)
{
    uint8_t *dst = (uint8_t *)_dst;
    while (cnt--) {
        *dst++ = 0;
    }
}


