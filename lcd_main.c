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


#include <stm32f7xx_it.h>
#include "stm32f769i_discovery_lcd.h"

#include <lcd_main.h>
#include <misc_utils.h>
#include <debug.h>
#include <heap.h>
#include <bsp_sys.h>

#define GET_VHAL_CTXT(cfg) (((lcd_wincfg_t *)(cfg))->hal_ctxt)
#define GET_VHAL_LTDC(cfg) (GET_VHAL_CTXT(cfg)->hltdc)
#define VHAL_PIX_FMT(cfg, num) (GET_VHAL_LTDC(cfg)->LayerCfg[num].PixelFormat)

struct screen_hal_ctxt_s;

typedef struct {
    struct screen_hal_ctxt_s *hal_ctxt;
    void *usermem;
    void *fb_mem;
    void *lay_mem[LCD_MAX_LAYER];
    uint32_t fb_size;
    uint32_t lay_size;
    lcd_layers_t ready_lay_idx;
    uint16_t w, h;
    uint8_t lay_cnt;
    uint8_t colormode;
    __IO uint8_t waitreload;
    uint8_t poll;
    uint8_t layreload;
} lcd_wincfg_t;

extern void *Sys_HeapAllocFb (int *size);

typedef void (*screen_update_handler_t) (screen_t *in);

screen_update_handler_t screen_update_handle;

#define LCD_MAX_SCALE 3

static void screen_update_no_scale (screen_t *in);
static void screen_update_2x2_8bpp (screen_t *in);
static void screen_update_3x3_8bpp (screen_t *in);
static int screen_hal_init (int init, uint8_t clockpresc);
static void screen_hal_attach (lcd_wincfg_t *cfg);
static void *screen_hal_set_config (lcd_wincfg_t *cfg, int x, int y,
                                            int w, int h, uint8_t colormode);
static void screen_hal_set_clut (lcd_wincfg_t *cfg, void *_buf, int size, int layer);

static void screen_hal_sync (lcd_wincfg_t *cfg, int wait);

static int bsp_lcd_width = -1;
static int bsp_lcd_height = -1;

lcd_wincfg_t lcd_def_cfg;
lcd_wincfg_t *lcd_active_cfg = NULL;

static const lcd_layers_t layer_switch[] =
{
    [LCD_BACKGROUND] = LCD_FOREGROUND,
    [LCD_FOREGROUND] = LCD_BACKGROUND,
};

static const uint8_t layer_transparency[] =
{
    [LCD_BACKGROUND] = GFX_TRANSPARENT,
    [LCD_FOREGROUND] = GFX_OPAQUE,
};

static const char *screen_mode2txt_map[] =
{
    [GFX_COLOR_MODE_CLUT] = "LTDC_L8",
    [GFX_COLOR_MODE_RGB565] = "LTDC_RGB565",
    [GFX_COLOR_MODE_RGBA8888] = "LTDC_ARGB8888",
};

static const uint32_t screen_mode2pixdeep[] =
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
screen_alloc_fb (lcd_mem_malloc_t __malloc, lcd_wincfg_t *cfg,
                       uint32_t w, uint32_t h, uint32_t pixel_deep, uint32_t layers_cnt)
{
    int fb_size;
    int i = layers_cnt;
    uint8_t *fb_mem;

    assert(layers_cnt <= LCD_MAX_LAYER)

    fb_size = ((w + 1) * h) * pixel_deep * layers_cnt;

    if (cfg->fb_mem) {
        heap_free(cfg->fb_mem);
    }
    fb_mem = __malloc(fb_size);
    if (!fb_mem) {
        dprintf("%s() : failed to alloc %u bytes\n", __func__, fb_size);
        return NULL;
    }
    d_memset(fb_mem, 0, fb_size);

    cfg->fb_size = fb_size;
    cfg->fb_mem = fb_mem;
    cfg->lay_size = fb_size / layers_cnt;
    cfg->lay_cnt = layers_cnt;
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

/*
 * Set the layer to draw to
 *
 * This has no effect on the LCD itself, only to drawing routines
 *
 * @param[in]	layer	layer to change to
 */
static inline lcd_layers_t screen_hal_set_layer (lcd_wincfg_t *cfg)
{
    switch (cfg->ready_lay_idx) {
        case LCD_BACKGROUND:
            BSP_LCD_SelectLayer(LCD_BACKGROUND);
            if (cfg->layreload == 0) {
                BSP_LCD_SetTransparency(LCD_FOREGROUND, GFX_OPAQUE);
                BSP_LCD_SetTransparency(LCD_BACKGROUND, GFX_TRANSPARENT);
            } else {
                BSP_LCD_SetLayerVisible_NoReload(LCD_FOREGROUND, ENABLE);
                BSP_LCD_SetLayerVisible_NoReload(LCD_BACKGROUND, DISABLE);
            }
            return LCD_BACKGROUND;
        break;
        case LCD_FOREGROUND:
            BSP_LCD_SelectLayer(LCD_FOREGROUND);
            if (cfg->layreload == 0) {
                BSP_LCD_SetTransparency(LCD_BACKGROUND, GFX_OPAQUE);
                BSP_LCD_SetTransparency(LCD_FOREGROUND, GFX_TRANSPARENT);
            } else {
                BSP_LCD_SetLayerVisible_NoReload(LCD_BACKGROUND, ENABLE);
                BSP_LCD_SetLayerVisible_NoReload(LCD_FOREGROUND, DISABLE);
            }
            return LCD_FOREGROUND;
        break;
        default:
        break;
    }
    assert(0);
    return LCD_FOREGROUND;
}

static screen_update_handler_t vid_set_scaler (int scale, uint8_t colormode)
{
    screen_update_handler_t h = NULL;

    if (colormode == GFX_COLOR_MODE_CLUT) {
        switch (scale) {
            case 1:
                h = screen_update_no_scale;
            case 2:
                h = screen_update_2x2_8bpp;
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

static int vid_set_win_size (screen_t *screen, int lcd_w, int lcd_h, int *x, int *y, int *w, int *h)
{
    int scale = -1;
    int win_w = screen->width > 0 ? screen->width : lcd_w,
        win_h = screen->height > 0 ? screen->height : lcd_h;

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

int vid_config (lcd_mem_malloc_t __malloc, void *cfg, screen_t *screen, uint32_t colormode, int layers_cnt)
{
    uint32_t scale;
    int x, y, w, h;

    cfg = vid_get_config((lcd_wincfg_t *)cfg);
    if ((lcd_wincfg_t *)cfg == lcd_active_cfg) {
        return 0;
    }
    lcd_active_cfg = cfg;
    scale = vid_set_win_size(screen, bsp_lcd_width, bsp_lcd_height, &x, &y, &w, &h);

    screen_update_handle = vid_set_scaler(scale, colormode);

    lcd_x_size_var = w;
    lcd_y_size_var = h;

    if (!screen_alloc_fb(__malloc, cfg, w, h, screen_mode2pixdeep[colormode], layers_cnt)) {
        return -1;
    }

    screen_hal_set_config(cfg, x, y, w, h, colormode);

    return 0;
}

uint32_t vid_mem_avail (void)
{
    assert(lcd_active_cfg);
    return ((lcd_active_cfg->lay_size * lcd_active_cfg->lay_cnt) / 1024);
}

void vid_vsync (void)
{
    profiler_enter();
    screen_hal_sync(lcd_active_cfg, 1);
    profiler_exit();
}

static void vid_get_ready_screen (screen_t *screen)
{
    screen->width = bsp_lcd_width;
    screen->height = bsp_lcd_height;
    screen->buf = (void *)lcd_active_cfg->lay_mem[lcd_active_cfg->ready_lay_idx];
    screen->x = 0;
    screen->y = 0;
    screen->colormode = lcd_active_cfg->colormode;
}

void vid_set_clut (void *palette, uint32_t clut_num_entries)
{
    int layer;

    assert(lcd_active_cfg);
    screen_hal_sync(lcd_active_cfg, 1);
    for (layer = 0; layer < lcd_active_cfg->lay_cnt; layer++) {
        screen_hal_set_clut (lcd_active_cfg, palette, clut_num_entries, layer);
    }
}

void vid_upate (screen_t *in)
{
    if (in == NULL) {
        vid_vsync();
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
             lcd_active_cfg->lay_cnt, screen_mode2txt_map[lcd_active_cfg->colormode]);
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

    screen_hal_sync (lcd_active_cfg, 1);
    vid_get_ready_screen(&screen);

    d_memcpy(screen.buf, in->buf, in->width * in->height);
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

    screen_hal_sync (lcd_active_cfg, 1);
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

typedef struct copybuf_s {
    struct copybuf_s *next;
    screen_t dest, src;
} copybuf_t;

typedef struct screen_hal_ctxt_s {
    DMA2D_HandleTypeDef dma2d;
    LCD_LayerCfgTypeDef cfg;
    LTDC_HandleTypeDef *hltdc;
    copybuf_t *bufq;
    uint8_t busy;
    uint8_t poll;
    uint8_t prescaler;
} screen_hal_ctxt_t;

static screen_hal_ctxt_t screen_hal_ctxt = {{0}};

static const uint32_t dma2d_color_mode2fmt_map[] =
{
    [GFX_COLOR_MODE_CLUT] = DMA2D_OUTPUT_ARGB8888,
    [GFX_COLOR_MODE_RGB565] = DMA2D_OUTPUT_RGB565,
    [GFX_COLOR_MODE_RGBA8888] = DMA2D_OUTPUT_ARGB8888,
};

static const uint32_t screen_mode2fmt_map[] =
{
    [GFX_COLOR_MODE_CLUT] = LTDC_PIXEL_FORMAT_L8,
    [GFX_COLOR_MODE_RGB565] = LTDC_PIXEL_FORMAT_RGB565,
    [GFX_COLOR_MODE_RGBA8888] = LTDC_PIXEL_FORMAT_ARGB8888,
};

static void screen_dma2d_irq_hdlr (screen_hal_ctxt_t *ctxt);
static void screen_copybuf_split (screen_hal_ctxt_t *ctxt, copybuf_t *buf, int parts);
static int screen_hal_copy_next (screen_hal_ctxt_t *ctxt);
static int screen_hal_clock_cfg (screen_hal_ctxt_t *ctxt);

static void screen_hal_set_clut (lcd_wincfg_t *cfg, void *_buf, int size, int layer)
{
    if (VHAL_PIX_FMT(cfg, layer) != screen_mode2fmt_map[GFX_COLOR_MODE_CLUT]) {
        return;
    }
    HAL_LTDC_ConfigCLUT(GET_VHAL_LTDC(cfg), (uint32_t *)_buf, size, layer);
    HAL_LTDC_EnableCLUT(GET_VHAL_LTDC(cfg), layer);
}

int screen_hal_clk_presc (void)
{
    return lcd_active_cfg->hal_ctxt->prescaler ? lcd_active_cfg->hal_ctxt->prescaler : 0;
}

static int screen_hal_init (int init, uint8_t clockpresc)
{
    uint32_t status;

    d_memset(&screen_hal_ctxt, 0, sizeof(screen_hal_ctxt));
    if (init) {

        screen_hal_ctxt.prescaler = clockpresc;
        status = BSP_LCD_Init();
        assert(!status);

        bsp_lcd_width = BSP_LCD_GetXSize();
        bsp_lcd_height = BSP_LCD_GetYSize();

        BSP_LCD_SetBrightness(100);
    } else {
        BSP_LCD_SetBrightness(0);
        HAL_Delay(1000);
    }
}

static void screen_hal_attach (lcd_wincfg_t *cfg)
{
extern LTDC_HandleTypeDef hltdc_discovery;
    cfg->hal_ctxt = &screen_hal_ctxt;
    cfg->hal_ctxt->hltdc = &hltdc_discovery;
}

static void *
screen_hal_set_config (lcd_wincfg_t *cfg, int x, int y, int w, int h, uint8_t colormode)
{
    int layer;
    LCD_LayerCfgTypeDef *Layercfg;

    screen_hal_attach(cfg);

    cfg->colormode = colormode;

    Layercfg = &cfg->hal_ctxt->cfg;
    /* Layer Init */
    Layercfg->WindowX0 = x;
    Layercfg->WindowX1 = x + w;
    Layercfg->WindowY0 = y;
    Layercfg->WindowY1 = y + h;
    Layercfg->PixelFormat = screen_mode2fmt_map[colormode];
    Layercfg->Alpha = 255;
    Layercfg->Alpha0 = 0;
    Layercfg->Backcolor.Blue = 0;
    Layercfg->Backcolor.Green = 0;
    Layercfg->Backcolor.Red = 0;
    Layercfg->BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
    Layercfg->BlendingFactor2 = LTDC_BLENDING_FACTOR1_CA;
    Layercfg->ImageWidth = w;
    Layercfg->ImageHeight = h;

    for (layer = 0; layer < cfg->lay_cnt; layer++) {
        Layercfg->FBStartAdress = (uint32_t)cfg->lay_mem[layer];
        HAL_LTDC_ConfigLayer(cfg->hal_ctxt->hltdc, Layercfg, layer);
    }

    return Layercfg;
}

static inline void __screen_hal_vsync (lcd_wincfg_t *cfg)
{
    if (cfg->poll) {
        while ((LTDC->CDSR & LTDC_CDSR_VSYNCS)) {}
    } else {
        while (cfg->waitreload) {}
    }
}

static void screen_hal_sync (lcd_wincfg_t *cfg, int wait)
{
    while (cfg->hal_ctxt->bufq) {
        HAL_Delay(1);
    }
    if (cfg->lay_cnt > 1) {
        cfg->waitreload = wait & cfg->layreload;
        cfg->ready_lay_idx = screen_hal_set_layer(cfg);
    }
    if (wait) {
        __screen_hal_vsync(cfg);
    }
}

void DMA2D_IRQHandler(void)
{
    HAL_DMA2D_IRQHandler(&lcd_active_cfg->hal_ctxt->dma2d);
    lcd_active_cfg->hal_ctxt->busy = 0;
}

void DMA2D_XferCpltCallback (struct __DMA2D_HandleTypeDef * hdma2d)
{
    if (&lcd_active_cfg->hal_ctxt->dma2d == hdma2d) {
        screen_dma2d_irq_hdlr(lcd_active_cfg->hal_ctxt);
    }
}

void BSP_LCD_LTDC_IRQHandler (void)
{
extern LTDC_HandleTypeDef hltdc_discovery;
    HAL_LTDC_IRQHandler(lcd_active_cfg->hal_ctxt->hltdc);
}

void HAL_LTDC_ReloadEventCallback(LTDC_HandleTypeDef *hltdc)
{
    lcd_active_cfg->waitreload = 0;
}

static int
screen_hal_copy (screen_hal_ctxt_t *ctxt, copybuf_t *copybuf);

static int screen_update_direct (lcd_wincfg_t *cfg, screen_t *psrc)
{
    copybuf_t buf = {NULL};
    screen_t *dest = &buf.dest, *src = &buf.src;

    if (!psrc) {
        psrc = src;
        src->buf = cfg->lay_mem[layer_switch[cfg->ready_lay_idx]];
        src->x = 0;
        src->y = 0;
        src->width = cfg->w;
        src->height = cfg->h;
        src->colormode = cfg->colormode;
    }
    dest->buf = cfg->lay_mem[cfg->ready_lay_idx];
    dest->x = 0;
    dest->y = 0;
    dest->width = cfg->w;
    dest->height = cfg->h;
    dest->colormode = cfg->colormode;
    if (GET_VHAL_CTXT(cfg)->poll) {
        return screen_hal_copy(GET_VHAL_CTXT(cfg), &buf);
    } else {
        irqmask_t irq;
        int ret;

        irq_save(&irq);
        if (GET_VHAL_CTXT(cfg)->bufq) {
            return 0;
        }
        screen_copybuf_split(GET_VHAL_CTXT(cfg), &buf, 4);
        ret = screen_hal_copy_next(GET_VHAL_CTXT(cfg));
        irq_restore(irq);
        return ret;
    }
}

static DMA2D_HandleTypeDef *
__screen_hal_copy_setup (screen_hal_ctxt_t *ctxt, screen_t *dest, screen_t *src)
{
    DMA2D_HandleTypeDef *hdma2d = &ctxt->dma2d;
    uint32_t destination = (uint32_t)dest->buf + (dest->y * dest->width + dest->x) * sizeof(uint32_t);
    uint32_t source      = (uint32_t)src->buf + (src->y * src->width + src->x) * sizeof(uint32_t);

    hdma2d->Init.Mode         = DMA2D_M2M;
    hdma2d->Init.ColorMode    = dma2d_color_mode2fmt_map[dest->colormode];
    hdma2d->Init.OutputOffset = dest->width - src->width;
    hdma2d->Init.AlphaInverted = DMA2D_REGULAR_ALPHA;
    hdma2d->Init.RedBlueSwap   = DMA2D_RB_REGULAR;

    hdma2d->XferCpltCallback  = DMA2D_XferCpltCallback;

    hdma2d->LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    hdma2d->LayerCfg[1].InputAlpha = 0xFF;
    hdma2d->LayerCfg[1].InputColorMode = dma2d_color_mode2fmt_map[src->colormode];
    hdma2d->LayerCfg[1].InputOffset = 0;
    hdma2d->LayerCfg[1].RedBlueSwap = DMA2D_RB_REGULAR;
    hdma2d->LayerCfg[1].AlphaInverted = DMA2D_REGULAR_ALPHA;

    hdma2d->Instance          = DMA2D;

    dest->buf = (void *)destination;
    src->buf = (void *)source;
    return hdma2d;
}

int screen_hal_copy (screen_hal_ctxt_t *ctxt, copybuf_t *copybuf)
{
    HAL_StatusTypeDef status = HAL_OK;
    screen_t *dest = &copybuf->dest;
    screen_t *src = &copybuf->src;
    DMA2D_HandleTypeDef *hdma2d = __screen_hal_copy_setup(ctxt, dest, src);

    if(HAL_DMA2D_Init(hdma2d) != HAL_OK) {
        return -1;
    }
    if (HAL_DMA2D_ConfigLayer(hdma2d, 1) != HAL_OK) {
        return -1;
    }
    if (HAL_DMA2D_Start_IT(hdma2d, (uint32_t)src->buf, (uint32_t)dest->buf, src->width, src->height) != HAL_OK) {
        return -1;
    }
    if (ctxt->poll) {
        status = HAL_DMA2D_PollForTransfer(hdma2d, ctxt->poll);
    }
    if (status != HAL_OK) {
        return -1;
    }
    return 0;
}

static int
screen_hal_copy_next (screen_hal_ctxt_t *ctxt)
{
    copybuf_t *bufq = ctxt->bufq;
    copybuf_t *buf;
    int ret;

    if (!bufq) {
        return 0;
    }
    buf = bufq;
    bufq = bufq->next;
    ret = screen_hal_copy(ctxt, buf);
    heap_free(buf);
    ctxt->bufq = bufq;
    return ret;
}

static copybuf_t *
screen_hal_copybuf_alloc (screen_hal_ctxt_t *ctxt, screen_t *dest, screen_t *src)
{
    copybuf_t *buf = heap_malloc(sizeof(*buf));

    d_memcpy(&buf->dest, dest, sizeof(buf->dest));
    d_memcpy(&buf->src, src, sizeof(buf->src));

    buf->next = ctxt->bufq;
    ctxt->bufq = buf;
    return buf;
}

static void screen_dma2d_irq_hdlr (screen_hal_ctxt_t *ctxt)
{
    if (ctxt->bufq == NULL) {
        return;
    }
    screen_hal_copy_next(ctxt);
}

static void screen_copybuf_split (screen_hal_ctxt_t *ctxt, copybuf_t *buf, int parts)
{
    screen_t dest = buf->dest, src = buf->src;
    int h, i;

    h = src.height / parts;
    src.height = h;
    dest.height = h;

    for (i = 0; i < parts; i++) {
        screen_hal_copybuf_alloc(ctxt, &dest, &src);
        dest.y += h;
        src.y += h;
    }
    parts = src.height % parts;
    if (parts) {
        dest.height = parts;
        src.height = parts;
        screen_hal_copybuf_alloc(ctxt, &dest, &src);
    }
}

static int screen_hal_clock_cfg (screen_hal_ctxt_t *ctxt)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;
    uint8_t prescaler = ctxt->prescaler;

    if (!prescaler)
        prescaler = 1;

    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    PeriphClkInitStruct.PLLSAI.PLLSAIN = 384 / prescaler;
    PeriphClkInitStruct.PLLSAI.PLLSAIR = 7;
    PeriphClkInitStruct.PLLSAIDivR = RCC_PLLSAIDIVR_2;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);
}

static int vid_priv_vram_alloc (lcd_wincfg_t *cfg, lcd_mem_malloc_t __malloc, arch_word_t *ptr, arch_word_t *size);
static int vid_priv_vram_copy (lcd_wincfg_t *cfg, screen_t screen[2]);

int vid_priv_ctl (int c, void *v)
{
    int ret = 0;
    switch (c) {
        case VCTL_VRAM_ALLOC:
        {
            arch_word_t *data = (arch_word_t *)v;
            lcd_mem_malloc_t __malloc = (lcd_mem_malloc_t)data[0];

            ret = vid_priv_vram_alloc(lcd_active_cfg, __malloc, &data[1], &data[2]);
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

static int vid_priv_vram_alloc (lcd_wincfg_t *cfg, lcd_mem_malloc_t __malloc,
                                        arch_word_t *ptr, arch_word_t *size)
{
    lcd_wincfg_t cfgtmp = {0};

    if (cfg->usermem) {
        heap_free(cfg->usermem);
        cfg->usermem = NULL;
    }
    cfg->usermem = screen_alloc_fb(__malloc, &cfgtmp, cfg->w, cfg->h,
                    screen_mode2pixdeep[cfg->colormode], 1);
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

    return screen_hal_copy(GET_VHAL_CTXT(cfg), &copybuf);
}

int vid_priv_updown (int up)
{
    if (up) {
        __HAL_LTDC_ENABLE(lcd_active_cfg->hal_ctxt->hltdc);
    } else {
        __HAL_LTDC_DISABLE(lcd_active_cfg->hal_ctxt->hltdc);
    }
}

#endif

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

