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
#include "lcd_main.h"
#include "misc_utils.h"
#include <debug.h>

extern LTDC_HandleTypeDef  hltdc_discovery;

extern void *Sys_HeapAllocFb (int *size);

static void (*screen_update_handle) (screen_t *in);

#define LCD_MAX_SCALE 3

static void screen_update_1x1_truecolor (screen_t *in);
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

void screen_init (void)
{
    uint32_t status = BSP_LCD_Init();
    if(status)
    {
        fatal_error("BSP_LCD_Init : fail, %d\n", status);
    }

    bsp_lcd_width = BSP_LCD_GetXSize();
    bsp_lcd_height = BSP_LCD_GetYSize();

    BSP_LCD_SetBrightness(100);
}

void screen_deinit (void)
{
    dprintf("%s() :\n", __func__);
    BSP_LCD_DeInitEx();
    if (lcd_active_cfg && lcd_active_cfg->fb_mem) {
        Sys_Free(lcd_active_cfg->fb_mem);
    }
}

static void * screen_alloc_fb (lcd_wincfg_t *cfg, uint32_t w, uint32_t h, uint32_t pixel_deep, uint32_t layers_cnt)
{
    int fb_size;
    int i = layers_cnt;
    uint8_t *fb_mem;

    assert(layers_cnt <= LCD_MAX_LAYER)

    fb_size = ((w + 1) * h) * pixel_deep * layers_cnt;

    if (cfg->fb_mem) {
        Sys_Free(cfg->fb_mem);
    }
    fb_mem = Sys_AllocVideo(&fb_size);
    if (!fb_mem) {
        dprintf("%s() : failed to alloc %u bytes\n", __func__, fb_size);
        return NULL;
    }
    memset(fb_mem, 0, fb_size);

    cfg->fb_mem = fb_mem;
    cfg->lay_size = fb_size / layers_cnt;
    cfg->lay_cnt = layers_cnt;

    while (i-- > 0) {
        cfg->lay_mem[i] = fb_mem;
        fb_mem = fb_mem + cfg->lay_size;
    }

    return cfg->fb_mem;
}

void screen_ts_align (int *x, int *y)
{
    assert(lcd_active_cfg);

    *x = *x - lcd_active_cfg->lay_halcfg.WindowX0;
    *y = *y - lcd_active_cfg->lay_halcfg.WindowY0;
}

int screen_win_cfg (lcd_wincfg_t *cfg, screen_t *screen, uint32_t colormode, int layers_cnt)
{
    LCD_LayerCfgTypeDef *Layercfg;
    uint32_t scale_w = bsp_lcd_width / screen->width;
    uint32_t scale_h = bsp_lcd_height / screen->height;
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
                screen_update_handle = screen_update_1x1_truecolor;
            break;
        }
    }

    w = screen->width * scale;
    h = screen->height * scale;
    x = (bsp_lcd_width - w) / scale;
    y = (bsp_lcd_height - h) / scale;

    lcd_x_size_var = w;
    lcd_y_size_var = h;

    if (!screen_alloc_fb(cfg, w, h, screen_mode2pixdeep[colormode], layers_cnt)) {
        return -1;
    }

    Layercfg = &cfg->lay_halcfg;
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

uint32_t screen_total_mem_avail_kb (void)
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

static void screen_sync (int wait)
{
    assert(lcd_active_cfg);
    if (wait) {
        lcd_wait_ready();
    } else {
        lcd_set_layer(lcd_active_cfg->active_lay_idx);
        lcd_active_cfg->active_lay_idx = layer_switch[lcd_active_cfg->active_lay_idx];
    }
}

void screen_vsync (void)
{
    screen_sync(d_true);
}

static void screen_get_invis_screen (screen_t *screen)
{
    screen->width = bsp_lcd_width;
    screen->height = bsp_lcd_height;
    screen->buf = (pix_t *)lcd_active_cfg->lay_mem[layer_switch[lcd_active_cfg->active_lay_idx]];
}

void screen_set_clut (pal_t *palette, uint32_t clut_num_entries)
{
    int layer;

    assert(lcd_active_cfg);
    screen_sync(1);
    for (layer = 0; layer < lcd_active_cfg->lay_cnt; layer++) {
        screen_load_clut (palette, clut_num_entries, layer);
    }
}

void screen_update (screen_t *in)
{
    screen_update_handle(in);
}

void screen_direct (screen_t *s)
{
    screen_get_invis_screen(s);
}


typedef struct {
    pix_t a[4];
} scanline_t;

typedef union {
#if (GFX_COLOR_MODE == GFX_COLOR_MODE_CLUT)
    uint32_t w;
#elif (GFX_COLOR_MODE == GFX_COLOR_MODE_RGB565)
    uint64_t w;
#endif
    scanline_t sl;
} scanline_u;

#define W_STEP (sizeof(scanline_t) / sizeof(pix_t))
#define DST_NEXT_LINE(x, w, lines) (((x) + (lines) * ((w) / sizeof(scanline_u))))

typedef struct {
    scanline_u a[2];
} pix_outx2_t;

static void screen_update_1x1_truecolor (screen_t *in)
{
    screen_t screen;

    SCB_CleanDCache();
    screen_get_invis_screen(&screen);

    memcpy(in->buf, screen.buf, in->width * in->height);
}

static void screen_update_2x2_8bpp (screen_t *in)
{
    pix_outx2_t *d_y0;
    pix_outx2_t *d_y1;
    pix_outx2_t pix;
    int s_y, i;
    scanline_t *scanline;
    scanline_u d_yt0, d_yt1;
    screen_t screen;

    screen_sync (0);
    SCB_CleanDCache();
    screen_get_invis_screen(&screen);

    d_y0 = (pix_outx2_t *)screen.buf;
    d_y1 = DST_NEXT_LINE(d_y0, in->width, 1);

    for (s_y = 0; s_y < (in->width * in->height); s_y += in->width) {

        scanline = (scanline_t *)&in->buf[s_y];

        for (i = 0; i < in->width; i += W_STEP) {

            d_yt0.sl = *scanline++;
            d_yt1    = d_yt0;

            d_yt0.sl.a[3] = d_yt0.sl.a[1];
            d_yt0.sl.a[2] = d_yt0.sl.a[1];
            d_yt0.sl.a[1] = d_yt0.sl.a[0];

            d_yt1.sl.a[0] = d_yt1.sl.a[2];
            d_yt1.sl.a[1] = d_yt1.sl.a[2];
            d_yt1.sl.a[2] = d_yt1.sl.a[3];

#if (GFX_COLOR_MODE == GFX_COLOR_MODE_CLUT)
            pix.a[0] = d_yt0;
            pix.a[1] = d_yt1;
#elif (GFX_COLOR_MODE == GFX_COLOR_MODE_RGB565)
            fatal_error("%s() : needs fix\n");
            pix = d_yt0.w;
            *d_y0++     = pix;
            *d_y1++     = pix;

            pix = d_yt1.w;
#endif
            *d_y0++     = pix;
            *d_y1++     = pix;
        }
        d_y0 = DST_NEXT_LINE(d_y0, in->width, 1);
        d_y1 = DST_NEXT_LINE(d_y1, in->width, 1);
    }
}

typedef struct {
    scanline_u a[3];
} pix_outx3_t;

static void screen_update_3x3_8bpp(screen_t *in)
{
    pix_outx3_t *d_y0, *d_y1, *d_y2;
    pix_outx3_t pix;
    int s_y, i;
    scanline_t *scanline;
    scanline_u d_yt0, d_yt1, d_yt2;
    screen_t screen;

    screen_sync (0);
    SCB_CleanDCache();
    screen_get_invis_screen(&screen);

    d_y0 = (pix_outx3_t *)screen.buf;
    d_y1 = DST_NEXT_LINE(d_y0, in->width, 1);
    d_y2 = DST_NEXT_LINE(d_y1, in->width, 1);

    for (s_y = 0; s_y < (in->width * in->height); s_y += in->width) {

        scanline = (scanline_t *)&in->buf[s_y];

        for (i = 0; i < in->width; i += W_STEP) {

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
#if (GFX_COLOR_MODE == GFX_COLOR_MODE_CLUT)
            pix.a[0] = d_yt0;
            pix.a[1] = d_yt1;
            pix.a[2] = d_yt2;
#elif (GFX_COLOR_MODE == GFX_COLOR_MODE_RGB565)
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

        d_y0 = DST_NEXT_LINE(d_y0, in->width, 2);
        d_y1 = DST_NEXT_LINE(d_y1, in->width, 2);
        d_y2 = DST_NEXT_LINE(d_y2, in->width, 2);
    }
}


/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

