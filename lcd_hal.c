
#include <stm32f7xx_it.h>
#include "stm32f769i_discovery_lcd.h"

#include "int/lcd_int.h"

#include <misc_utils.h>

enum {
    V_STATE_IDLE,
    V_STATE_QCOPY,
    V_STATE_COPYFAST,
    V_STATE_MAX,
};

typedef struct screen_hal_ctxt_s {
    lcd_wincfg_t *lcd_cfg;
    void *hal_dma;
    void *hal_cfg;
    void *hal_ltdc;
    copybuf_t *bufq;
    uint8_t busy;
    uint8_t poll;
    uint8_t prescaler;
    uint8_t state;
} screen_hal_ctxt_t;

#define GET_VHAL_CTXT(cfg) ((screen_hal_ctxt_t *)((lcd_wincfg_t *)(cfg))->hal_ctxt)
#define GET_VHAL_LTDC(cfg) ((LTDC_HandleTypeDef *)GET_VHAL_CTXT(cfg)->hal_ltdc)
#define GET_VHAL_DMA2D(cfg) ((DMA2D_HandleTypeDef *)GET_VHAL_CTXT(cfg)->hal_dma)
#define GET_VHAL_HALCFG(cfg) ((LCD_LayerCfgTypeDef *)GET_VHAL_CTXT(cfg)->hal_cfg)
#define VHAL_PIX_FMT(cfg, num) (GET_VHAL_LTDC(cfg)->LayerCfg[num].PixelFormat)


lcd_layers_t screen_hal_set_layer (lcd_wincfg_t *cfg)
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

static screen_hal_ctxt_t screen_hal_ctxt = {{0}};

static const uint32_t dma2d_color_mode2out_map[] =
{
    [GFX_COLOR_MODE_CLUT] = DMA2D_OUTPUT_ARGB8888,
    [GFX_COLOR_MODE_RGB565] = DMA2D_OUTPUT_RGB565,
    [GFX_COLOR_MODE_RGBA8888] = DMA2D_OUTPUT_ARGB8888,
};

static const uint32_t dma2d_color_mode2in_map[] =
{
    [GFX_COLOR_MODE_CLUT] = DMA2D_INPUT_L8,
    [GFX_COLOR_MODE_RGB565] = DMA2D_INPUT_RGB565,
    [GFX_COLOR_MODE_RGBA8888] = DMA2D_INPUT_ARGB8888,
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


void screen_hal_set_clut (lcd_wincfg_t *cfg, void *_buf, int size, int layer)
{
    if (VHAL_PIX_FMT(cfg, layer) != screen_mode2fmt_map[GFX_COLOR_MODE_CLUT]) {
        return;
    }
    HAL_LTDC_ConfigCLUT(GET_VHAL_LTDC(cfg), (uint32_t *)_buf, size, layer);
    HAL_LTDC_EnableCLUT(GET_VHAL_LTDC(cfg), layer);
}

int screen_hal_init (int init, uint8_t clockpresc)
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

void screen_hal_attach (lcd_wincfg_t *cfg)
{
extern LTDC_HandleTypeDef hltdc_discovery;
    static DMA2D_HandleTypeDef dma2d;
    static LCD_LayerCfgTypeDef hal_cfg;

    cfg->hal_ctxt = &screen_hal_ctxt;
    GET_VHAL_CTXT(cfg)->hal_ltdc = &hltdc_discovery;
    GET_VHAL_CTXT(cfg)->hal_dma = &dma2d;
    GET_VHAL_CTXT(cfg)->hal_cfg = &hal_cfg;
    screen_hal_ctxt.lcd_cfg = cfg;
}

void *
screen_hal_set_config (lcd_wincfg_t *cfg, int x, int y, int w, int h, uint8_t colormode)
{
    int layer;
    LCD_LayerCfgTypeDef *Layercfg;

    screen_hal_attach(cfg);

    Layercfg = GET_VHAL_HALCFG(cfg);
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

    for (layer = 0; layer < cfg->config.laynum; layer++) {
        Layercfg->FBStartAdress = (uint32_t)cfg->lay_mem[layer];
        HAL_LTDC_ConfigLayer(GET_VHAL_LTDC(cfg), Layercfg, layer);
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

void screen_hal_sync (lcd_wincfg_t *cfg, int wait)
{
    while (GET_VHAL_CTXT(cfg)->state != V_STATE_IDLE) {
        HAL_Delay(1);
    }
    if (cfg->config.laynum > 1) {
        cfg->waitreload = wait & cfg->layreload;
        cfg->ready_lay_idx = screen_hal_set_layer(cfg);
    }
    if (wait) {
        __screen_hal_vsync(cfg);
    }
}

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
        src->colormode = cfg->config.colormode;
    }
    dest->buf = cfg->lay_mem[cfg->ready_lay_idx];
    dest->x = 0;
    dest->y = 0;
    dest->width = cfg->w;
    dest->height = cfg->h;
    dest->colormode = cfg->config.colormode;
    if (GET_VHAL_CTXT(cfg)->poll) {
        return screen_hal_copy(cfg, &buf);
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

void DMA2D_XferCpltCallback (struct __DMA2D_HandleTypeDef * hdma2d);

static DMA2D_HandleTypeDef *
__screen_hal_copy_setup
    (screen_hal_ctxt_t *ctxt, screen_t *dest, screen_t *src,
     int dest_leg, int src_leg, uint32_t mode, uint32_t pixel_size)
{
    DMA2D_HandleTypeDef *hdma2d = GET_VHAL_DMA2D(ctxt->lcd_cfg);

    hdma2d->Init.Mode         = DMA2D_M2M;
    hdma2d->Init.ColorMode    = dma2d_color_mode2in_map[dest->colormode];
    hdma2d->Init.OutputOffset = dest->width - src->width;
    hdma2d->Init.AlphaInverted = DMA2D_REGULAR_ALPHA;
    hdma2d->Init.RedBlueSwap   = DMA2D_RB_REGULAR;

    hdma2d->XferCpltCallback  = DMA2D_XferCpltCallback;

    hdma2d->LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
    hdma2d->LayerCfg[1].InputAlpha = 0xFF;
    hdma2d->LayerCfg[1].InputColorMode = dma2d_color_mode2out_map[src->colormode];
    hdma2d->LayerCfg[1].InputOffset = 0;
    hdma2d->LayerCfg[1].RedBlueSwap = DMA2D_RB_REGULAR;
    hdma2d->LayerCfg[1].AlphaInverted = DMA2D_REGULAR_ALPHA;

    hdma2d->Instance          = DMA2D;
    return hdma2d;
}

static int screen_hal_copy_start
            (lcd_wincfg_t *cfg, DMA2D_HandleTypeDef *hdma2d, screen_t *dest, screen_t *src,
            void *dptr, void *sptr)
{
    HAL_StatusTypeDef status = HAL_OK;

    if(HAL_DMA2D_Init(hdma2d) != HAL_OK) {
        return -1;
    }
    if (HAL_DMA2D_ConfigLayer(hdma2d, 1) != HAL_OK) {
        return -1;
    }
    if (GET_VHAL_CTXT(cfg)->poll) {
        if (HAL_DMA2D_Start(hdma2d, (uint32_t)dptr, (uint32_t)sptr, src->width, src->height) != HAL_OK) {
            return -1;
        }
        status = HAL_DMA2D_PollForTransfer(hdma2d, GET_VHAL_CTXT(cfg)->poll);
        GET_VHAL_CTXT(cfg)->state = V_STATE_IDLE;
    } else {
        if (HAL_DMA2D_Start_IT(hdma2d, (uint32_t)dptr, (uint32_t)sptr, src->width, src->height) != HAL_OK) {
            return -1;
        }
    }
    if (status != HAL_OK) {
        return -1;
    }
    return 0;
}

int screen_hal_copy (lcd_wincfg_t *cfg, copybuf_t *copybuf)
{
    HAL_StatusTypeDef status = HAL_OK;
    screen_t *dest = &copybuf->dest;
    screen_t *src = &copybuf->src;
    const int pix_size = 4;
    DMA2D_HandleTypeDef *hdma2d;

    void *destination = (void *)((uint32_t)dest->buf + (dest->y * dest->width + dest->x) * pix_size);
    void *source      = (void *)((uint32_t)src->buf + (src->y * src->width + src->x) * pix_size);

    GET_VHAL_CTXT(cfg)->state = V_STATE_QCOPY;

    hdma2d = __screen_hal_copy_setup(GET_VHAL_CTXT(cfg), dest, src,
                                                          dest->width, src->width, DMA2D_M2M, pix_size);

    return screen_hal_copy_start(cfg, hdma2d, dest, src, destination, source);
}

typedef struct {
    copybuf_t copybuf;
    uint16_t dest_leg;
    uint16_t src_leg;
} fastline_t;

static fastline_t fastline_h8;

int screen_hal_copy_h8 (lcd_wincfg_t *cfg, copybuf_t *copybuf)
{
    screen_t *dest = &copybuf->dest;
    screen_t *src = &copybuf->src;
    DMA2D_HandleTypeDef *hdma2d;
    int dw = dest->width, sw = src->width;
    const int pix_size = 1;

    void *dptr;
    void *sptr;

    fastline_h8.copybuf = *copybuf;
    copybuf = &fastline_h8.copybuf;
    copybuf->next = NULL;
    fastline_h8.dest_leg = dest->width;
    fastline_h8.src_leg = dest->width;
    dest->width = 1;
    src->width = 1;
    dest->x = dest->width - 1;
    src->x = src->width - 1;

    dptr = (void *)((uint32_t)dest->buf + (dest->y * fastline_h8.dest_leg + dest->x) * pix_size);
    sptr      = (void *)((uint32_t)src->buf + (src->y * fastline_h8.src_leg + src->x) * pix_size);

    GET_VHAL_CTXT(cfg)->state = V_STATE_COPYFAST;

    hdma2d = __screen_hal_copy_setup(GET_VHAL_CTXT(cfg), dest, src, dw, sw, DMA2D_M2M, pix_size);
    return screen_hal_copy_start(cfg, hdma2d, dest, src, dptr, sptr);
}

static int
screen_hal_copy_h8_next (screen_hal_ctxt_t *ctxt)
{
    void *dptr;
    void *sptr;
    const int pix_size = 1;
    screen_t *dest = &fastline_h8.copybuf.dest;
    screen_t *src = &fastline_h8.copybuf.src;

    if (dest->x == 0 || src->x == 0) {
        ctxt->state = V_STATE_IDLE;
        return 0;
    }

    dest->x--;
    if (dest->x & 0x1) {
        dest->y = 1;
    } else {
        dest->y = 0;
        src->x--;
    }

    dptr = (void *)((uint32_t)dest->buf + (dest->y * fastline_h8.dest_leg + dest->x) * pix_size);
    sptr      = (void *)((uint32_t)src->buf + (src->y * fastline_h8.src_leg + src->x) * pix_size);

    return screen_hal_copy_start(ctxt->lcd_cfg, GET_VHAL_DMA2D(ctxt->lcd_cfg), dest, src, dptr, sptr);
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
    ret = screen_hal_copy(ctxt->lcd_cfg, buf);
    ctxt->lcd_cfg->config.alloc.free(buf);
    ctxt->bufq = bufq;
    return ret;
}

static copybuf_t *
screen_hal_copybuf_alloc (screen_hal_ctxt_t *ctxt, screen_t *dest, screen_t *src)
{
    copybuf_t *buf = ctxt->lcd_cfg->config.alloc.malloc(sizeof(*buf));

    d_memcpy(&buf->dest, dest, sizeof(buf->dest));
    d_memcpy(&buf->src, src, sizeof(buf->src));

    buf->next = ctxt->bufq;
    ctxt->bufq = buf;
    return buf;
}

static void screen_dma2d_irq_hdlr (screen_hal_ctxt_t *ctxt)
{
    switch (ctxt->state) {

        case V_STATE_QCOPY:
                if (ctxt->bufq == NULL) {
                    return;
                }
                screen_hal_copy_next(ctxt);
        break;
        case V_STATE_COPYFAST:
                screen_hal_copy_h8_next(ctxt);
        break;
        default:
        break;
    }
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




void DMA2D_IRQHandler(void)
{
    HAL_DMA2D_IRQHandler(GET_VHAL_DMA2D(lcd_active_cfg));
    GET_VHAL_CTXT(lcd_active_cfg)->busy = 0;
}

void DMA2D_XferCpltCallback (struct __DMA2D_HandleTypeDef * hdma2d)
{
    if (GET_VHAL_DMA2D(lcd_active_cfg) == hdma2d) {
        screen_dma2d_irq_hdlr(GET_VHAL_CTXT(lcd_active_cfg));
    }
}

void BSP_LCD_LTDC_IRQHandler (void)
{
    HAL_LTDC_IRQHandler(GET_VHAL_LTDC(lcd_active_cfg));
}

void HAL_LTDC_ReloadEventCallback(LTDC_HandleTypeDef *hltdc)
{
    lcd_active_cfg->waitreload = 0;
}

