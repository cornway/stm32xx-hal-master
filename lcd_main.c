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


#include "lcd_main.h"
#include "stm32f769i_discovery_lcd.h"
#include "misc_utils.h"
#include <debug.h>
#include <heap.h>

typedef struct {
    void *fb_mem;
    void *lay_mem[LCD_MAX_LAYER];
    uint32_t fb_size;
    uint32_t lay_size;
    void *lay_halcfg;
    lcd_layers_t active_lay_idx;
    uint16_t w, h;
    uint8_t lay_cnt;
    uint8_t colormode;
} lcd_wincfg_t;

extern LTDC_HandleTypeDef  hltdc_discovery;

extern void *Sys_HeapAllocFb (int *size);

static void (*screen_update_handle) (screen_t *in);

#define LCD_MAX_SCALE 3

static void screen_update_no_scale (screen_t *in);
static void screen_update_2x2_8bpp (screen_t *in);
static void screen_update_3x3_8bpp (screen_t *in);

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

static const uint32_t screen_mode2fmt_map[] =
{
    [GFX_COLOR_MODE_CLUT] = LTDC_PIXEL_FORMAT_L8,
    [GFX_COLOR_MODE_RGB565] = LTDC_PIXEL_FORMAT_RGB565,
    [GFX_COLOR_MODE_RGBA8888] = LTDC_PIXEL_FORMAT_ARGB8888,
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

void screen_load_clut (void *_buf, int size, int layer)
{
    HAL_LTDC_ConfigCLUT(&hltdc_discovery, (uint32_t *)_buf, size, layer);
    HAL_LTDC_EnableCLUT(&hltdc_discovery, layer);
}

int vid_init (void)
{
    uint32_t status = BSP_LCD_Init();
    if(status)
    {
        fatal_error("BSP_LCD_Init : fail, %d\n", status);
    }

    bsp_lcd_width = BSP_LCD_GetXSize();
    bsp_lcd_height = BSP_LCD_GetYSize();

    BSP_LCD_SetBrightness(100);
    return 0;
}

static void vid_release (void)
{
    if (lcd_active_cfg && lcd_active_cfg->fb_mem) {
        memset(lcd_active_cfg->fb_mem, 0, lcd_active_cfg->fb_size);
    }
    if (lcd_active_cfg) {
        memset(lcd_active_cfg, 0, sizeof(*lcd_active_cfg));
    }
    lcd_active_cfg = NULL;
}

void vid_deinit (void)
{
    dprintf("%s() :\n", __func__);
    vid_release();
    BSP_LCD_DeInitEx();
}

void vid_wh (screen_t *s)
{
    assert(lcd_active_cfg && s);
    s->width = lcd_x_size_var;
    s->height = lcd_y_size_var;
}

static void * screen_alloc_fb (lcd_mem_malloc_t __malloc, lcd_wincfg_t *cfg, uint32_t w, uint32_t h, uint32_t pixel_deep, uint32_t layers_cnt)
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
    memset(fb_mem, 0, fb_size);

    cfg->fb_size = fb_size;
    cfg->fb_mem = fb_mem;
    cfg->lay_size = fb_size / layers_cnt;
    cfg->lay_cnt = layers_cnt;
    cfg->w = w;
    cfg->h = h;

    while (i-- > 0) {
        cfg->lay_mem[i] = fb_mem;
        fb_mem = fb_mem + cfg->lay_size;
    }

    return cfg->fb_mem;
}

void vid_ptr_align (int *x, int *y)
{
}

static LCD_LayerCfgTypeDef default_laycfg;


int vid_config (lcd_mem_malloc_t __malloc, void *_cfg, screen_t *screen, uint32_t colormode, int layers_cnt)
{
    lcd_wincfg_t *cfg = _cfg;
    LCD_LayerCfgTypeDef *Layercfg;
    int in_w = screen->width > 0 ? screen->width : bsp_lcd_width,
        in_h = screen->height > 0 ? screen->height : bsp_lcd_height;
    uint32_t scale_w = bsp_lcd_width / in_w;
    uint32_t scale_h = bsp_lcd_height / in_h;
    uint32_t scale;
    uint32_t x, y, w, h;
    int layer;

    if (!cfg) {
        cfg = &lcd_def_cfg;
    }

    if (cfg == lcd_active_cfg) {
        return 0;
    }
    lcd_active_cfg = cfg;
    if (scale_w < scale_h) {
        scale_h = scale_w;
    } else {
        scale_w = scale_h;
    }
    scale = scale_w;
    if (scale > LCD_MAX_SCALE) {
        scale = LCD_MAX_SCALE;
    }

    if (colormode == GFX_COLOR_MODE_CLUT) {
        switch (scale) {
            case 1:
                screen_update_handle = screen_update_no_scale;
            case 2:
                screen_update_handle = screen_update_2x2_8bpp;
            break;
            case 3:
                screen_update_handle = screen_update_3x3_8bpp;
            break;

            default:
                fatal_error("%s() : Scale not supported yet!\n", __func__);
            break;
        }
    } else {
        switch (scale) {
            default:
                dprintf("%s() : Only \'no scale\' mode present\n", __func__);
                screen_update_handle = screen_update_no_scale;
            break;
        }
    }

    w = in_w * scale;
    h = in_h * scale;
    x = (bsp_lcd_width - w) / scale;
    y = (bsp_lcd_height - h) / scale;

    lcd_x_size_var = w;
    lcd_y_size_var = h;

    if (!screen_alloc_fb(__malloc, cfg, w, h, screen_mode2pixdeep[colormode], layers_cnt)) {
        return -1;
    }

    cfg->lay_halcfg = &default_laycfg;
    cfg->colormode = colormode;

    Layercfg = &default_laycfg;
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

    for (layer = 0; layer < layers_cnt; layer++) {
        Layercfg->FBStartAdress = (uint32_t)cfg->lay_mem[layer];
        HAL_LTDC_ConfigLayer(&hltdc_discovery, Layercfg, layer);
    }
    return 0;
}

uint32_t vid_mem_avail (void)
{
    assert(lcd_active_cfg);
    return ((lcd_active_cfg->lay_size * lcd_active_cfg->lay_cnt) / 1024);
}

/*
 * Set the layer to draw to
 *
 * This has no effect on the LCD itself, only to drawing routines
 *
 * @param[in]	layer	layer to change to
 */
static inline void lcd_set_layer (lcd_layers_t layer)
{
    BSP_LCD_SetTransparency(LCD_FOREGROUND, layer_transparency[layer]);
}

static inline void lcd_wait_ready ()
{
    while (!(LTDC->CDSR & LTDC_CDSR_VSYNCS));
}

static void vid_sync (int wait)
{
    assert(lcd_active_cfg);
    if (wait) {
        lcd_wait_ready();
    } else {
        lcd_set_layer(lcd_active_cfg->active_lay_idx);
        lcd_active_cfg->active_lay_idx = layer_switch[lcd_active_cfg->active_lay_idx];
    }
}

void vid_vsync (void)
{
    vid_sync(1);
}

static void screen_get_invis_screen (screen_t *screen)
{
    screen->width = bsp_lcd_width;
    screen->height = bsp_lcd_height;
    screen->buf = (void *)lcd_active_cfg->lay_mem[layer_switch[lcd_active_cfg->active_lay_idx]];
}

void vid_set_clut (void *palette, uint32_t clut_num_entries)
{
    int layer;
    LCD_LayerCfgTypeDef *Layercfg;

    assert(lcd_active_cfg);
    Layercfg = lcd_active_cfg->lay_halcfg;
    if (Layercfg->PixelFormat != screen_mode2fmt_map[GFX_COLOR_MODE_CLUT]) {
        return;
    }
    vid_sync(1);
    for (layer = 0; layer < lcd_active_cfg->lay_cnt; layer++) {
        screen_load_clut (palette, clut_num_entries, layer);
    }
}

void vid_upate (screen_t *in)
{
    screen_update_handle(in);
}

void vid_direct (screen_t *s)
{
    screen_get_invis_screen(s);
}

void vid_print_info (void)
{
    LCD_LayerCfgTypeDef *Layercfg;

    assert(lcd_active_cfg);

    Layercfg = lcd_active_cfg->lay_halcfg;
    dprintf("width=%4.3u height=%4.3u\n", lcd_active_cfg->w, lcd_active_cfg->h);
    dprintf("layers=%u, color mode=<%s>\n",
             lcd_active_cfg->lay_cnt, screen_mode2txt_map[lcd_active_cfg->colormode]);
    dprintf("memory= <0x%x> 0x%08x bytes\n", lcd_active_cfg->fb_mem, lcd_active_cfg->fb_size);
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

    vid_sync (1);
    screen_get_invis_screen(&screen);

    memcpy(screen.buf, in->buf, in->width * in->height);
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

    vid_sync (1);
    screen_get_invis_screen(&screen);

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

    vid_sync(1);
    screen_get_invis_screen(&screen);

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

#endif

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

