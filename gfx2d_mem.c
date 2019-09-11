
#include <gfx.h>
#include <gfx2d_mem.h>

typedef uint8_t pix8_t;

void gfx2d_copy (gfx_2d_buf_t *dest2d, gfx_2d_buf_t *src2d)
{
    screen_t dest_s, src_s;
    __gfx2d_to_screen(&dest_s, dest2d);
    __gfx2d_to_screen(&src_s, src2d);
    vid_copy(&dest_s, &src_s);
}

typedef struct {
    pix8_t a[4];
} scanline8_t;

typedef union {
    uint32_t w;
    scanline8_t sl;
} scanline8_u;

#define W_STEP8 (sizeof(scanline8_t) / sizeof(pix8_t))
#define DST_NEXT_LINE8(x, w, lines) (((x) + (lines) * ((w) / sizeof(scanline8_u))))

typedef struct {
    scanline8_u a[2];
} pix_outx2_t;

void
gfx2d_scale2x2_8bpp (gfx_2d_buf_t *dest, gfx_2d_buf_t *src)
{
    scanline8_u d_yt0, d_yt1;
    scanline8_t *scanline;
    pix_outx2_t *d_y0;
    pix_outx2_t *d_y1;
    pix_outx2_t pix;
    pix8_t *rawptr;
    int s_y, i;

    d_y0 = (pix_outx2_t *)dest->buf;
    d_y1 = DST_NEXT_LINE8(d_y0, dest->wtotal, 1);

    rawptr = (pix8_t *)src->buf;
    for (s_y = 0; s_y < (src->wtotal * src->htotal); s_y += src->wtotal) {

        scanline = (scanline8_t *)&rawptr[s_y];

        for (i = 0; i < src->w; i += W_STEP8) {

            d_yt0.sl = *scanline++;
            d_yt1    = d_yt0;

            d_yt0.sl.a[3] = d_yt0.sl.a[1];
            d_yt0.sl.a[2] = d_yt0.sl.a[1];
            d_yt0.sl.a[1] = d_yt0.sl.a[0];

            d_yt1.sl.a[0] = d_yt1.sl.a[2];
            d_yt1.sl.a[1] = d_yt1.sl.a[2];
            d_yt1.sl.a[2] = d_yt1.sl.a[3];

            pix.a[0] = d_yt0;
            pix.a[1] = d_yt1;

            *d_y0++     = pix;
            *d_y1++     = pix;
        }
        d_y0 = DST_NEXT_LINE8(d_y0, dest->wtotal, 1);
        d_y1 = DST_NEXT_LINE8(d_y1, dest->wtotal, 1);
    }
}

typedef struct {
    scanline8_u a[3];
} pix_outx3_t;

void
gfx2d_scale3x3_8bpp (gfx_2d_buf_t *dest, gfx_2d_buf_t *src)
{
    scanline8_u d_yt0, d_yt1, d_yt2;
    pix_outx3_t *d_y0, *d_y1, *d_y2;
    scanline8_t *scanline;
    pix_outx3_t pix;
    pix8_t *rawptr;
    int s_y, i;

    d_y0 = (pix_outx3_t *)dest->buf;
    d_y1 = DST_NEXT_LINE8(d_y0, dest->wtotal, 1);
    d_y2 = DST_NEXT_LINE8(d_y1, dest->wtotal, 1);
    rawptr = (pix8_t *)src->buf;
    for (s_y = 0; s_y < (src->wtotal * src->htotal); s_y += src->wtotal) {

        scanline = (scanline8_t *)&rawptr[s_y];

        for (i = 0; i < src->w; i += W_STEP8) {

            d_yt0.sl = *scanline++;
            d_yt1    = d_yt0;
            d_yt2    = d_yt1;

            d_yt2.sl.a[2] = d_yt0.sl.a[3];
            d_yt2.sl.a[1] = d_yt0.sl.a[3];

            d_yt2.sl.a[0] = d_yt0.sl.a[2];
            d_yt1.sl.a[3] = d_yt0.sl.a[2];

            d_yt1.sl.a[0] = d_yt0.sl.a[1];
            d_yt0.sl.a[3] = d_yt0.sl.a[1];

            d_yt0.sl.a[2] = d_yt0.sl.a[0];
            d_yt0.sl.a[1] = d_yt0.sl.a[0];
            pix.a[0] = d_yt0;
            pix.a[1] = d_yt1;
            pix.a[2] = d_yt2;

            *d_y0++     = pix;
            *d_y1++     = pix;
            *d_y2++     = pix;
        }

        d_y0 = DST_NEXT_LINE8(d_y0, dest->wtotal, 2);
        d_y1 = DST_NEXT_LINE8(d_y1, dest->wtotal, 2);
        d_y2 = DST_NEXT_LINE8(d_y2, dest->wtotal, 2);
    }
}


