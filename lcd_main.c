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

#ifndef SCREEN_MAX_SCALE
#define SCREEN_MAX_SCALE 2
#endif

extern LTDC_HandleTypeDef  hltdc_discovery;

extern void *Sys_HeapAllocFb (int *size);

static int bsp_lcd_width = -1;
static int bsp_lcd_height = -1;
static int bsp_lcd_fbsize = -1;
static int bsp_lcd_laysize = -1;

static uint32_t layer_addr [LCD_MAX_LAYER] = {0, 0};
static lcd_layers_t screen_layer_current = LCD_BACKGROUND;

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

void screen_load_clut (void *_buf, int size, int layer)
{
    HAL_LTDC_ConfigCLUT(&hltdc_discovery, (uint32_t *)_buf, size, layer);
    HAL_LTDC_EnableCLUT(&hltdc_discovery, layer);
}

static void
_alloc_fb_ondemand (int w, int h)
{
    uint32_t bsp_lcd_fb;

    if (bsp_lcd_laysize > 0) {
        return;
    }

    bsp_lcd_laysize = (w * h) * sizeof(pix_t);
    bsp_lcd_fbsize = bsp_lcd_laysize * LTDC_NB_OF_LAYERS;

    bsp_lcd_fb = (uint32_t)Sys_AllocVideo(&bsp_lcd_fbsize);

    layer_addr[LCD_BACKGROUND] = bsp_lcd_fb + bsp_lcd_laysize;
    layer_addr[LCD_FOREGROUND] = bsp_lcd_fb;

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

    BSP_LCD_SetLayerVisible(LCD_FOREGROUND, ENABLE);
    BSP_LCD_SetLayerVisible(LCD_BACKGROUND, ENABLE);
    BSP_LCD_SetBrightness(100);
}

void screen_win_cfg (screen_t *screen)
{
    int layer;
    uint32_t scale_w = (bsp_lcd_width / screen->width);
    uint32_t scale_h = (bsp_lcd_height / screen->height);
    uint32_t x, y, w, h;

    LCD_LayerCfgTypeDef  Layercfg;

    if (scale_w > SCREEN_MAX_SCALE) {
        scale_w = SCREEN_MAX_SCALE;
    }

    if (scale_h > SCREEN_MAX_SCALE) {
        scale_h = SCREEN_MAX_SCALE;
    }

    w = screen->width * scale_w;
    h = screen->height * scale_h;
    x = (bsp_lcd_width - w) / scale_w;
    y = (bsp_lcd_height - h) / scale_h;

    _alloc_fb_ondemand(w, h);

    /* Layer Init */
    Layercfg.WindowX0 = x;
    Layercfg.WindowX1 = x + w;
    Layercfg.WindowY0 = y;
    Layercfg.WindowY1 = y + h;
    Layercfg.PixelFormat = screen_mode2fmt_map[GFX_COLOR_MODE];
    Layercfg.Alpha = 255;
    Layercfg.Alpha0 = 0;
    Layercfg.Backcolor.Blue = 0;
    Layercfg.Backcolor.Green = 0;
    Layercfg.Backcolor.Red = 0;
    Layercfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
    Layercfg.BlendingFactor2 = LTDC_BLENDING_FACTOR1_CA;
    Layercfg.ImageWidth = w;
    Layercfg.ImageHeight = h;

    for (layer = 0; layer < (int)LCD_MAX_LAYER; layer++) {
        Layercfg.FBStartAdress = layer_addr[layer];
        HAL_LTDC_ConfigLayer(&hltdc_discovery, &Layercfg, layer);
        memset((void *)layer_addr[layer], 0, bsp_lcd_laysize);
    }
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

void screen_sync (int wait)
{
    if (wait) {
        lcd_wait_ready();
    } else {
        lcd_set_layer(screen_layer_current);
        screen_layer_current = layer_switch[screen_layer_current];
    }
}

void screen_get_invis_screen (screen_t *screen)
{
    screen->width = bsp_lcd_width;
    screen->height = bsp_lcd_height;
    screen->buf = (pix_t *)layer_addr[layer_switch[screen_layer_current]];
}

void screen_set_clut (pal_t *palette, uint32_t clut_num_entries)
{
    int layer;
    for (layer = 0; layer < (int)LCD_MAX_LAYER; layer++) {
        screen_load_clut (palette, clut_num_entries, layer);
    }
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

#define DST_NEXT_LINE(x, w) (((uint32_t)(x) + w * 2 * sizeof(pix_t)))
#define W_STEP (sizeof(scanline_t) / sizeof(pix_t))

void screen_update_2x2(screen_t *in)
{
    uint64_t *d_y0;
    uint64_t *d_y1;
    uint64_t pix;
    int s_y, i;
    scanline_t *scanline;
    scanline_u d_yt0, d_yt1;
    screen_t screen;

    screen_sync (0);
    SCB_CleanDCache();
    screen_get_invis_screen(&screen);

    d_y0 = (uint64_t *)screen.buf;
    d_y1 = (uint64_t *)DST_NEXT_LINE(d_y0, in->width);

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
            pix = (uint64_t)(((uint64_t)d_yt1.w << 32) | d_yt0.w);
#elif (GFX_COLOR_MODE == GFX_COLOR_MODE_RGB565)
            pix = d_yt0.w;
            *d_y0++     = pix;
            *d_y1++     = pix;

            pix = d_yt1.w;
#endif
            *d_y0++     = pix;
            *d_y1++     = pix;
        }
        d_y0 = d_y1;
        d_y1 = (uint64_t *)DST_NEXT_LINE(d_y0, in->width);
    }
}



/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

