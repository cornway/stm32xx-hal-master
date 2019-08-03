/**
  ******************************************************************************
  * @file    DMA2D/DMA2D_MemToMemWithBlending/Src/main.c
  * @author  MCD Application Team
  * @brief   This example provides a description of how to configure
  *          DMA2D peripheral in Memory to Memory with Blending transfer mode
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/

#if defined(BSP_DRIVER)

#include "int/lcd_int.h"

#include <lcd_main.h>
#include <misc_utils.h>
#include <debug.h>
#include <heap.h>
#include <bsp_sys.h>

extern void *Sys_HeapAllocFb (int *size);

static void screen_update_no_scale (screen_t *in);
static void screen_update_1x1_fast (screen_t *in);
static void screen_update_2x2_fast (screen_t *in);
static void screen_update_2x2_8bpp (screen_t *in);
static void screen_update_3x3_8bpp (screen_t *in);

static screen_update_handler_t screen_update_handle;

int bsp_lcd_width = -1;
int bsp_lcd_height = -1;

lcd_wincfg_t lcd_def_cfg;
lcd_wincfg_t *lcd_active_cfg = NULL;

const lcd_layers_t layer_switch[LCD_MAX_LAYER] =
{
    [LCD_BACKGROUND] = LCD_FOREGROUND,
    [LCD_FOREGROUND] = LCD_BACKGROUND,
};

static const char *screen_mode2txt_map[] =
{
    [GFX_COLOR_MODE_CLUT] = "LTDC_L8",
    [GFX_COLOR_MODE_RGB565] = "LTDC_RGB565",
    [GFX_COLOR_MODE_RGBA8888] = "LTDC_ARGB8888",
};

const uint32_t screen_mode2pixdeep[GFX_COLOR_MODE_MAX] =
{
    [GFX_COLOR_MODE_CLUT]       = 1,
    [GFX_COLOR_MODE_RGB565]     = 2,
    [GFX_COLOR_MODE_RGBA8888]   = 4,
};


int vid_init (void)
{
    return screen_hal_init(1, 2);
}

static void vid_release (lcd_wincfg_t *cfg)
{
    if (cfg && cfg->fb_mem) {
        d_memset(cfg->fb_mem, 0, cfg->fb_size);
        heap_free(cfg->fb_mem);
    }
    if (cfg) {
        if (cfg->usermem) {
            heap_free(cfg->usermem);
            d_memset(cfg->usermem, 0, cfg->lay_size);
        }
        d_memset(cfg, 0, sizeof(*cfg));
    }
    cfg = NULL;
}

void vid_deinit (void)
{
    dprintf("%s() :\n", __func__);
    screen_hal_init(0, 0);
    vid_release(lcd_active_cfg);
    lcd_active_cfg = NULL;
    BSP_LCD_DeInitEx();
}

void vid_wh (screen_t *s)
{
    assert(lcd_active_cfg && s);
    s->width = lcd_active_cfg->w;
    s->height = lcd_active_cfg->h;
}

static void *
screen_alloc_fb (screen_alloc_t *alloc, lcd_wincfg_t *cfg,
                       uint32_t w, uint32_t h, uint32_t pixel_deep, uint32_t layers_cnt)
{
    int fb_size;
    int i = layers_cnt;
    uint8_t *fb_mem;

    assert(layers_cnt <= LCD_MAX_LAYER);

    fb_size = ((w + 1) * h) * pixel_deep * layers_cnt;

    if (cfg->fb_mem) {
        heap_free(cfg->fb_mem);
    }
    fb_mem = alloc->malloc(fb_size);
    if (!fb_mem) {
        dprintf("%s() : failed to alloc %u bytes\n", __func__, fb_size);
        return NULL;
    }
    d_memset(fb_mem, 0, fb_size);

    cfg->fb_size = fb_size;
    cfg->fb_mem = fb_mem;
    cfg->lay_size = fb_size / layers_cnt;
    cfg->config.laynum = layers_cnt;
    cfg->w = w;
    cfg->h = h;

    for (i = 0; i < layers_cnt; i++) {
        cfg->lay_mem[i] = fb_mem;
        fb_mem = fb_mem + cfg->lay_size;
    }

    return cfg->fb_mem;
}

void vid_ptr_align (int *x, int *y)
{
}

static screen_update_handler_t vid_set_scaler (int scale, uint8_t colormode)
{
    screen_update_handler_t h = NULL;

    if (colormode == GFX_COLOR_MODE_CLUT) {
        switch (scale) {
            case 1:
                if (lcd_active_cfg->config.hwaccel) {
                    h = screen_update_1x1_fast;
                } else {
                    h = screen_update_no_scale;
                }
            case 2:
                if (lcd_active_cfg->config.hwaccel) {
                    h = screen_update_2x2_fast;
                } else {
                    h = screen_update_2x2_8bpp;
                }
            break;
            case 3:
                h = screen_update_3x3_8bpp;
            break;

            default:
                fatal_error("%s() : Scale not supported yet!\n", __func__);
            break;
        }
    } else {
        switch (scale) {
            default:
                h = screen_update_no_scale;
            break;
        }
    }
    return h;
}

static lcd_wincfg_t *vid_get_config (lcd_wincfg_t *cfg)
{
    if (!cfg) {
        cfg = &lcd_def_cfg;
        d_memset(&lcd_def_cfg, 0, sizeof(lcd_def_cfg));
    }
    return cfg;
}

static int vid_set_win_size (int screen_w, int screen_h, int lcd_w, int lcd_h, int *x, int *y, int *w, int *h)
{
    int scale = -1;
    int win_w = screen_w > 0 ? screen_w : lcd_w,
        win_h = screen_h > 0 ? screen_h : lcd_h;

    int sw = lcd_w / win_w;
    int sh = lcd_h / win_h;

    if (sw < sh) {
        sh = sw;
    } else {
        sw = sh;
    }
    scale = sw;
    if (scale > LCD_MAX_SCALE) {
        scale = LCD_MAX_SCALE;
    }

    *w = win_w * scale;
    *h = win_h * scale;
    *x = (lcd_w - *w) / scale;
    *y = (lcd_h - *h) / scale;
    return scale;
}

int vid_config (screen_conf_t *conf)
{
    uint32_t scale;
    int x, y, w, h;
    lcd_wincfg_t *cfg;

    cfg = vid_get_config(NULL);
    if ((lcd_wincfg_t *)cfg == lcd_active_cfg) {
        return 0;
    }
    lcd_active_cfg = cfg;
    lcd_active_cfg->config = *conf;
    scale = vid_set_win_size(conf->res_x, conf->res_y, bsp_lcd_width, bsp_lcd_height, &x, &y, &w, &h);

    screen_update_handle = vid_set_scaler(scale, conf->colormode);

    lcd_x_size_var = w;
    lcd_y_size_var = h;

    if (!screen_alloc_fb(&conf->alloc, cfg, w, h, screen_mode2pixdeep[conf->colormode], conf->laynum)) {
        return -1;
    }

    screen_hal_set_config(cfg, x, y, w, h, conf->colormode);

    return 0;
}

uint32_t vid_mem_avail (void)
{
    assert(lcd_active_cfg);
    return ((lcd_active_cfg->lay_size * lcd_active_cfg->config.laynum) / 1024);
}

void vid_vsync (int mode)
{
    profiler_enter();
    if (mode) {
        screen_hal_layreload(lcd_active_cfg);
    }
    screen_hal_sync(lcd_active_cfg, 1);
    profiler_exit();
}

static void vid_get_ready_screen (screen_t *screen)
{
    screen->width = lcd_active_cfg->w;
    screen->height = lcd_active_cfg->h;
    screen->buf = (void *)lcd_active_cfg->lay_mem[lcd_active_cfg->ready_lay_idx];
    screen->x = 0;
    screen->y = 0;
    screen->colormode = lcd_active_cfg->config.colormode;
}

void vid_set_clut (void *palette, uint32_t clut_num_entries)
{
    int layer;

    assert(lcd_active_cfg);
    screen_hal_sync(lcd_active_cfg, 1);
    for (layer = 0; layer < lcd_active_cfg->config.laynum; layer++) {
        screen_hal_set_clut (lcd_active_cfg, palette, clut_num_entries, layer);
    }
}

void vid_update (screen_t *in)
{
    if (in == NULL) {
        vid_vsync(1);
    } else {
        screen_update_handle(in);
    }
}

void vid_direct (screen_t *s)
{
    vid_get_ready_screen(s);
}

void vid_print_info (void)
{
    assert(lcd_active_cfg);

    dprintf("width=%4.3u height=%4.3u\n", lcd_active_cfg->w, lcd_active_cfg->h);
    dprintf("layers=%u, color mode=<%s>\n",
             lcd_active_cfg->config.laynum, screen_mode2txt_map[lcd_active_cfg->config.colormode]);
    dprintf("memory= <0x%p> 0x%08x bytes\n", lcd_active_cfg->fb_mem, lcd_active_cfg->fb_size);
}

typedef uint8_t pix8_t;

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

static void screen_update_no_scale (screen_t *in)
{
    screen_t screen;

    vid_vsync(1);
    vid_get_ready_screen(&screen);

    d_memcpy(screen.buf, in->buf, in->width * in->height);
}

static void screen_update_1x1_fast (screen_t *in)
{
    screen_t screen;
    copybuf_t copybuf;

    vid_vsync(1);
    vid_get_ready_screen(&screen);
    copybuf.dest = screen;
    copybuf.src = *in;

    screen_hal_copy(lcd_active_cfg, &copybuf, 1);
}

static void screen_update_2x2_fast (screen_t *in)
{
    screen_t screen;
    copybuf_t copybuf;

    vid_vsync(1);
    vid_get_ready_screen(&screen);
    copybuf.dest = screen;
    copybuf.src = *in;

    copybuf.dest.colormode = lcd_active_cfg->config.colormode;
    copybuf.src.colormode = lcd_active_cfg->config.colormode;
    screen_hal_copy_h8(lcd_active_cfg, &copybuf, lcd_active_cfg->config.hwaccel > 1);
}

static void screen_update_2x2_8bpp (screen_t *in)
{
    pix_outx2_t *d_y0;
    pix_outx2_t *d_y1;
    pix_outx2_t pix;
    pix8_t *rawptr;
    int s_y, i;
    scanline8_t *scanline;
    scanline8_u d_yt0, d_yt1;
    screen_t screen;

    vid_vsync(1);
    vid_get_ready_screen(&screen);

    d_y0 = (pix_outx2_t *)screen.buf;
    d_y1 = DST_NEXT_LINE8(d_y0, in->width, 1);

    rawptr = (pix8_t *)in->buf;
    for (s_y = 0; s_y < (in->width * in->height); s_y += in->width) {

        scanline = (scanline8_t *)&rawptr[s_y];

        for (i = 0; i < in->width; i += W_STEP8) {

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
        d_y0 = DST_NEXT_LINE8(d_y0, in->width, 1);
        d_y1 = DST_NEXT_LINE8(d_y1, in->width, 1);
    }
}

typedef struct {
    scanline8_u a[3];
} pix_outx3_t;

static void screen_update_3x3_8bpp(screen_t *in)
{
    pix_outx3_t *d_y0, *d_y1, *d_y2;
    pix_outx3_t pix;
    pix8_t *rawptr;
    int s_y, i;
    scanline8_t *scanline;
    scanline8_u d_yt0, d_yt1, d_yt2;
    screen_t screen;

    screen_hal_sync(lcd_active_cfg, 1);
    vid_get_ready_screen(&screen);

    d_y0 = (pix_outx3_t *)screen.buf;
    d_y1 = DST_NEXT_LINE8(d_y0, in->width, 1);
    d_y2 = DST_NEXT_LINE8(d_y1, in->width, 1);
    rawptr = (pix8_t *)in->buf;
    for (s_y = 0; s_y < (in->width * in->height); s_y += in->width) {

        scanline = (scanline8_t *)&rawptr[s_y];

        for (i = 0; i < in->width; i += W_STEP8) {

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
#if 0/* (GFX_COLOR_MODE == GFX_COLOR_MODE_RGB565)*/
            fatal_error("%s() : needs fix\n");
            pix = d_yt0.w;
            *d_y0++     = pix;
            *d_y1++     = pix;
            *d_y2++     = pix;

            pix = d_yt1.w;
#endif
            *d_y0++     = pix;
            *d_y1++     = pix;
            *d_y2++     = pix;
        }

        d_y0 = DST_NEXT_LINE8(d_y0, in->width, 2);
        d_y1 = DST_NEXT_LINE8(d_y1, in->width, 2);
        d_y2 = DST_NEXT_LINE8(d_y2, in->width, 2);
    }
}

static int vid_priv_vram_alloc (lcd_wincfg_t *cfg, screen_alloc_t *alloc, arch_word_t *ptr, arch_word_t *size);
static int vid_priv_vram_copy (lcd_wincfg_t *cfg, screen_t screen[2]);

int vid_priv_ctl (int c, void *v)
{
    int ret = 0;
    switch (c) {
        case VCTL_VRAM_ALLOC:
        {
            arch_word_t *data = (arch_word_t *)v;
            screen_alloc_t *alloc = (screen_alloc_t *)&data[0];

            ret = vid_priv_vram_alloc(lcd_active_cfg, alloc, &data[1], &data[2]);
        }
        break;
        case VCTL_VRAM_COPY:
        {
            screen_t *s = (screen_t *)v;
            ret = vid_priv_vram_copy(lcd_active_cfg, s);
        }
        break;
        default:
            dprintf("%s() : unknown ctl id [%i]", __func__, c);
            ret = -1;
        break;
    }
    return ret;
}

static int vid_priv_vram_alloc (lcd_wincfg_t *cfg, screen_alloc_t *alloc,
                                        arch_word_t *ptr, arch_word_t *size)
{
    lcd_wincfg_t cfgtmp = {0};

    if (cfg->usermem) {
        heap_free(cfg->usermem);
        cfg->usermem = NULL;
    }
    cfg->usermem = screen_alloc_fb(alloc, &cfgtmp, cfg->w, cfg->h,
                    screen_mode2pixdeep[cfg->config.colormode], 1);
    *ptr = (arch_word_t)cfgtmp.usermem;
    *size = cfgtmp.fb_size;
    return 0;
}

static int vid_priv_vram_copy (lcd_wincfg_t *cfg, screen_t screen[2])
{
    copybuf_t copybuf = {NULL, screen[0], screen[1]};

    if (copybuf.src.buf == NULL) {
        vid_get_ready_screen(&copybuf.src);
    } else if (copybuf.dest.buf == NULL) {
        vid_get_ready_screen(&copybuf.dest);
    }

    return screen_hal_copy(cfg, &copybuf, screen_mode2pixdeep[copybuf.dest.colormode]);
}

int vid_priv_updown (int up)
{
    return 0;
}

#endif

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

