#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if defined(USE_STM32H747I_DISCO)
#include <stm32h747i_discovery_lcd.h>
#elif defined(USE_STM32H745I_DISCO)
#include <stm32h745i_discovery_lcd.h>
#elif defined(USE_STM32F769I_DISCO)
#include <stm32f769i_discovery_lcd.h>
#else
#error
#endif

#include <config.h>

#include <arch.h>
#include <bsp_api.h>
#include <misc_utils.h>
#include <heap.h>
#include <gfx.h>
#include <gfx2d_mem.h>
#include <smp.h>
#include <lcd_main.h>
#include <lcd_int.h>
#include <nvic.h>
#include <bsp_cmd.h>
#include <debug.h>
#include <bsp_sys.h>

#define LCD_COLORMODE_INVALID (0xFFFFFFFF)

uint32_t lcd_x_size_var;
uint32_t lcd_y_size_var;

enum {
    V_STATE_IDLE,
    V_STATE_QCOPY,
    V_STATE_COPYFAST,
    V_STATE_MAX,
};

typedef struct screen_hal_ctxt_s {
    lcd_t *lcd;
    void *hal_dma;
    void *hal_cfg;
    void *hal_ltdc;
    void *hal_dsi;
    copybuf_t *bufq;
    uint8_t busy;
    uint8_t poll;
    uint8_t state;
    uint8_t waitreload;
} screen_hal_ctxt_t;

#define GET_VHAL_CTXT(lcd) ((screen_hal_ctxt_t *)((lcd_t *)(lcd))->hal_ctxt)
#define GET_VHAL_LTDC(lcd) ((LTDC_HandleTypeDef *)GET_VHAL_CTXT(lcd)->hal_ltdc)
#define GET_VHAL_DSI(lcd) ((DSI_HandleTypeDef *)GET_VHAL_CTXT(lcd)->hal_dsi)
#define GET_VHAL_DMA2D(lcd) ((DMA2D_HandleTypeDef *)GET_VHAL_CTXT(lcd)->hal_dma)
#define VHAL_PIX_FMT(lcd, num) (GET_VHAL_LTDC(lcd)->LayerCfg[num].PixelFormat)

#if defined(USE_STM32F769I_DISCO)

#define GET_VHAL_HALCFG(cfg) ((LCD_LayerCfgTypeDef *)GET_VHAL_CTXT(cfg)->hal_cfg)

#define GFX_TRANSPARENT				GFX_ARGB8888_A(LCD_COLOR_TRANSPARENT)
#define GFX_OPAQUE					GFX_ARGB8888_A(~LCD_COLOR_TRANSPARENT)

#elif defined(USE_STM32H745I_DISCO) || defined(USE_STM32H747I_DISCO)

#define GET_VHAL_HALCFG(cfg) ((LTDC_LayerCfgTypeDef *)GET_VHAL_CTXT(cfg)->hal_cfg)

#define GFX_TRANSPARENT				0x00
#define GFX_OPAQUE					0xff
#else
#error
#endif

static screen_hal_ctxt_t screen_hal_ctxt = {0};

static const uint32_t dma2d_color_mode2out_map[] =
{
    LCD_COLORMODE_INVALID,
    DMA2D_OUTPUT_ARGB8888,
    DMA2D_OUTPUT_RGB565,
    DMA2D_OUTPUT_ARGB8888,
};

static const uint32_t dma2d_color_mode2in_map[] =
{
    LCD_COLORMODE_INVALID,
    DMA2D_INPUT_L8,
    DMA2D_INPUT_RGB565,
    DMA2D_INPUT_ARGB8888,
};

static const uint32_t screen_mode2fmt_map[] =
{
    LCD_COLORMODE_INVALID,
    LTDC_PIXEL_FORMAT_L8,
    LTDC_PIXEL_FORMAT_RGB565,
    LTDC_PIXEL_FORMAT_ARGB8888,
};

void DMA2D_XferCpltCallback (struct __DMA2D_HandleTypeDef * hdma2d);

static void screen_dma2d_irq_hdlr (screen_hal_ctxt_t *ctxt);
static void screen_copybuf_split (screen_hal_ctxt_t *ctxt, copybuf_t *buf, int parts);
static int screen_hal_copy_next (screen_hal_ctxt_t *ctxt);

void screen_hal_set_clut (lcd_t *lcd, void *_buf, int size)
{
    assert(LCD_COLORMODE_INVALID != screen_mode2fmt_map[GFX_COLOR_MODE_CLUT]);

    if (VHAL_PIX_FMT(lcd, 0) != screen_mode2fmt_map[GFX_COLOR_MODE_CLUT]) {
        return;
    }
    __HAL_DSI_WRAPPER_DISABLE(GET_VHAL_DSI(lcd));
    HAL_LTDC_ConfigCLUT(GET_VHAL_LTDC(lcd), (uint32_t *)_buf, size, 0);
    HAL_LTDC_EnableCLUT(GET_VHAL_LTDC(lcd), 0);
    __HAL_DSI_WRAPPER_ENABLE(GET_VHAL_DSI(lcd));
}

int screen_hal_set_keying (lcd_t *cfg, uint32_t color)
{
#if defined(USE_STM32F769I_DISCO)
    BSP_LCD_SetColorKeying(0, color);
#elif defined(USE_STM32H745I_DISCO) || defined(USE_STM32H747I_DISCO)
    BSP_LCD_SetColorKeying(0, 0, color);
#else
#error
#endif
    return 0;
}

int screen_hal_init (int init)
{
    uint32_t status;

#if defined(USE_STM32F769I_DISCO)
    if (init) {

        d_memset(&screen_hal_ctxt, 0, sizeof(screen_hal_ctxt));
        status = BSP_LCD_Init();
        assert(!status);

        bsp_lcd_width = BSP_LCD_GetXSize();
        bsp_lcd_height = BSP_LCD_GetYSize();

        BSP_LCD_SetBrightness(100);
    } else {
        BSP_LCD_SetBrightness(0);
        BSP_LCD_DeInitEx();
        d_memset(&screen_hal_ctxt, 0, sizeof(screen_hal_ctxt));
        HAL_Delay(10);
    }
#elif defined(USE_STM32H745I_DISCO) || defined(USE_STM32H747I_DISCO)
    if (init) {

        d_memset(&screen_hal_ctxt, 0, sizeof(screen_hal_ctxt));
        if (EXEC_REGION_DRIVER()) {
            status = BSP_LCD_Init(0, LCD_ORIENTATION_LANDSCAPE);
        } else {
            status = BSP_LCD_CMD_Init();
        }
        assert(!status);

        BSP_LCD_GetXSize(0, &bsp_lcd_width);
        BSP_LCD_GetYSize(0, &bsp_lcd_height);
        BSP_LCD_SetBrightness(0, 100);
    } else {
        BSP_LCD_SetBrightness(0, 0);
        BSP_LCD_DeInitEx();
        d_memset(&screen_hal_ctxt, 0, sizeof(screen_hal_ctxt));
        HAL_Delay(10);
    }
#else
#error
#endif
    return 0;
}


extern LTDC_HandleTypeDef hlcd_ltdc;
extern DSI_HandleTypeDef   hlcd_dsi;

static DMA2D_HandleTypeDef dma2d;
#if defined(USE_STM32F769I_DISCO)
static LCD_LayerCfgTypeDef hal_cfg;
#elif defined(USE_STM32H745I_DISCO) || defined(USE_STM32H747I_DISCO)
static LTDC_LayerCfgTypeDef hal_cfg;
#else
#error
#endif

void screen_hal_attach (lcd_t *lcd)
{
    lcd->hal_ctxt = &screen_hal_ctxt;
    GET_VHAL_CTXT(lcd)->hal_ltdc = &hlcd_ltdc;
    GET_VHAL_CTXT(lcd)->hal_dma = &dma2d;
    GET_VHAL_CTXT(lcd)->hal_cfg = &hal_cfg;
    GET_VHAL_CTXT(lcd)->hal_dsi = &hlcd_dsi;
    screen_hal_ctxt.lcd = lcd;
}

void *screen_hal_set_config (lcd_t *lcd, int x, int y, int w, int h, uint8_t colormode)
{
#if defined(USE_STM32F769I_DISCO)
    LCD_LayerCfgTypeDef *Layercfg;
#elif defined(USE_STM32H745I_DISCO) || defined(USE_STM32H747I_DISCO)
    LTDC_LayerCfgTypeDef *Layercfg;
#else
#error
#endif
    screen_hal_attach(lcd);
    assert(LCD_COLORMODE_INVALID != screen_mode2fmt_map[colormode]);

    Layercfg = GET_VHAL_HALCFG(lcd);
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

    Layercfg->FBStartAdress = (uint32_t)lcd->fb.frame;

    __HAL_DSI_WRAPPER_DISABLE(GET_VHAL_DSI(lcd));
    if (HAL_LTDC_ConfigLayer(GET_VHAL_LTDC(lcd), Layercfg, 0)) {
        dprintf("%s() Failed\n", __func__);
        Layercfg =  NULL;
    }
    __HAL_DSI_WRAPPER_ENABLE(GET_VHAL_DSI(lcd));
    return Layercfg;
}

void screen_hal_sync (lcd_t *lcd)
{
#if defined(USE_STM32H745I_DISCO) || defined(USE_STM32H747I_DISCO)
    GET_VHAL_CTXT(lcd)->waitreload = 1U;
    HAL_DSI_Refresh(GET_VHAL_DSI(lcd));
#endif
    while (GET_VHAL_CTXT(lcd)->waitreload ||
           GET_VHAL_CTXT(lcd)->state != V_STATE_IDLE) {}
}

void screen_hal_refresh_direct (lcd_t *lcd)
{
    HAL_DSI_Refresh(GET_VHAL_DSI(lcd));
}

int screen_update_direct (lcd_t *lcd, screen_t *psrc)
{
    copybuf_t buf = {NULL};
    screen_t *dest = &buf.dest, *src = &buf.src;

    if (!psrc) {
        psrc = src;
        src->buf = lcd->fb.frame;
        src->x = 0;
        src->y = 0;
        src->width = lcd->fb.w;
        src->height = lcd->fb.h;
        src->colormode = lcd->config.colormode;
    }
    dest->buf = lcd->fb.frame;
    dest->x = 0;
    dest->y = 0;
    dest->width = lcd->fb.w;
    dest->height = lcd->fb.h;
    dest->colormode = lcd->config.colormode;
    if (GET_VHAL_CTXT(lcd)->poll) {
        assert(screen_mode2pixdeep[dest->colormode]);
        return screen_hal_copy_m2m(lcd, &buf, screen_mode2pixdeep[dest->colormode]);
    } else {
        irqmask_t irq;
        int ret;

        irq_save(&irq);
        if (GET_VHAL_CTXT(lcd)->bufq) {
            return 0;
        }
        screen_copybuf_split(GET_VHAL_CTXT(lcd), &buf, 4);
        ret = screen_hal_copy_next(GET_VHAL_CTXT(lcd));
        irq_restore(irq);
        return ret;
    }
}

static DMA2D_HandleTypeDef *
__screen_hal_copy_setup_M2M
(
    screen_hal_ctxt_t *ctxt,
    uint8_t dst_colormode,
    uint8_t src_colormode,
    uint8_t src_alpha,
    int src_width,
    int dest_wtotal,
    int src_wtotal
)
{
    DMA2D_HandleTypeDef *hdma2d = GET_VHAL_DMA2D(ctxt->lcd);
    const int layid = 1;

    d_memzero(&hdma2d->Init, sizeof(hdma2d->Init));
    assert(LCD_COLORMODE_INVALID != dma2d_color_mode2out_map[dst_colormode]);
    assert(LCD_COLORMODE_INVALID != dma2d_color_mode2in_map[src_colormode]);

    hdma2d->Init.Mode         = DMA2D_M2M;
    hdma2d->Init.ColorMode    = dma2d_color_mode2out_map[dst_colormode];
    hdma2d->Init.OutputOffset = dest_wtotal - src_width;
    hdma2d->Init.AlphaInverted = DMA2D_REGULAR_ALPHA;
    hdma2d->Init.RedBlueSwap   = DMA2D_RB_REGULAR;

    hdma2d->XferCpltCallback = DMA2D_XferCpltCallback;

    hdma2d->LayerCfg[layid].AlphaMode = DMA2D_COMBINE_ALPHA;
    hdma2d->LayerCfg[layid].InputAlpha = src_alpha;
    hdma2d->LayerCfg[layid].InputColorMode = dma2d_color_mode2in_map[src_colormode];
    hdma2d->LayerCfg[layid].InputOffset = src_wtotal - src_width;
    hdma2d->LayerCfg[layid].RedBlueSwap = DMA2D_RB_REGULAR;
    hdma2d->LayerCfg[layid].AlphaInverted = DMA2D_REGULAR_ALPHA;

    hdma2d->Instance = DMA2D;

    if(HAL_DMA2D_Init(hdma2d) != HAL_OK) {
        return NULL;
    }
    if (HAL_DMA2D_ConfigLayer(hdma2d, layid) != HAL_OK) {
        return NULL;
    }

    return hdma2d;
}

static inline void
__DMA2D_SetConfig_M2M (DMA2D_HandleTypeDef *hdma2d, uint32_t pdata,
                               uint32_t DstAddress, uint32_t Width, uint32_t Height)
{
    MODIFY_REG(hdma2d->Instance->NLR, (DMA2D_NLR_NL|DMA2D_NLR_PL), (Height| (Width << DMA2D_NLR_PL_Pos))); 
    WRITE_REG(hdma2d->Instance->OMAR, DstAddress);
    WRITE_REG(hdma2d->Instance->FGMAR, pdata);
}

static inline HAL_StatusTypeDef
__HAL_DMA2D_Start_IT (DMA2D_HandleTypeDef *hdma2d, uint32_t pdata,
                             uint32_t DstAddress, uint32_t Width,  uint32_t Height)
{
  __HAL_LOCK(hdma2d);

  hdma2d->State = HAL_DMA2D_STATE_BUSY;

  __DMA2D_SetConfig_M2M(hdma2d, pdata, DstAddress, Width, Height);

  /* Enable the transfer complete, transfer error and configuration error interrupts */
  __HAL_DMA2D_ENABLE_IT(hdma2d, DMA2D_IT_TC|DMA2D_IT_TE|DMA2D_IT_CE);

  /* Enable the Peripheral */
  __HAL_DMA2D_ENABLE(hdma2d);

  return HAL_OK;
}

static inline int 
screen_hal_copy_start
(
    lcd_t *cfg,
    uint32_t width,
    uint32_t height,
    void *dptr,
    void *sptr
)
{
    DMA2D_HandleTypeDef *hdma2d = GET_VHAL_DMA2D(cfg);
    HAL_StatusTypeDef status = HAL_OK;

    if (GET_VHAL_CTXT(cfg)->poll) {
        if (HAL_DMA2D_Start(hdma2d, (uint32_t)sptr, (uint32_t)dptr, width, height) != HAL_OK) {
            return -1;
        }
        status = HAL_DMA2D_PollForTransfer(hdma2d, GET_VHAL_CTXT(cfg)->poll);
        GET_VHAL_CTXT(cfg)->state = V_STATE_IDLE;
    } else {
        if (__HAL_DMA2D_Start_IT(hdma2d, (uint32_t)sptr, (uint32_t)dptr, width, height) != HAL_OK) {
            return -1;
        }
    }
    if (status != HAL_OK) {
        return -1;
    }
    return 0;
}

static inline void *
__screen_2_ptr (screen_t *s, uint8_t pixbytes)
{
    return (void *)((uint32_t)s->buf + (s->y * s->width + s->x) * pixbytes);
}

static inline void *
__gfx_2_ptr (gfx_2d_buf_t *buf, uint8_t pixbytes)
{
    return (void *)((uint32_t)buf->buf + (buf->y * buf->wtotal + buf->x) * pixbytes);
}

static inline void
__screen_check (lcd_t *cfg, screen_t *s)
{
    if (s->colormode == GFX_COLOR_MODE_AUTO) {
        s->colormode = cfg->config.colormode;
    }
}

int screen_hal_copy_m2m (lcd_t *cfg, copybuf_t *copybuf, uint8_t pix_bytes)
{
    screen_t *dest = &copybuf->dest;
    screen_t *src = &copybuf->src;

    void *dptr = __screen_2_ptr(dest, pix_bytes);
    void *sptr = __screen_2_ptr(src,  pix_bytes);

    __screen_check(cfg, dest);
    __screen_check(cfg, src);

    __screen_hal_copy_setup_M2M(GET_VHAL_CTXT(cfg), dest->colormode, src->colormode,
                                    src->alpha, src->width, dest->width, src->width);

    GET_VHAL_CTXT(cfg)->state = V_STATE_QCOPY;

    return screen_hal_copy_start(cfg, src->width, src->height, dptr, sptr);
}

int screen_gfx8888_copy (lcd_t *cfg, gfx_2d_buf_t *dest, gfx_2d_buf_t *src)
{
    void *dptr = __gfx_2_ptr(dest, 4);
    void *sptr = __gfx_2_ptr(src, 4);

    __screen_hal_copy_setup_M2M(GET_VHAL_CTXT(cfg), GFX_COLOR_MODE_ARGB8888, GFX_COLOR_MODE_ARGB8888,
                                    0xff, src->w, dest->wtotal, src->wtotal);

    GET_VHAL_CTXT(cfg)->state = V_STATE_QCOPY;

    return screen_hal_copy_start(cfg, src->w, src->h, dptr, sptr);
}

int screen_gfx8_copy_line (lcd_t *cfg, void *dest, void *src, int w)
{
    __screen_hal_copy_setup_M2M(GET_VHAL_CTXT(cfg), cfg->config.colormode, cfg->config.colormode,
                                    0xff, w, w, w);

    GET_VHAL_CTXT(cfg)->state = V_STATE_QCOPY;

    return screen_hal_copy_start(cfg, w, 1, dest, src);
}

typedef struct {
    copybuf_t copybuf;
    void *dptr, *sptr;
    uint16_t dest_leg;
    uint16_t src_leg;
    uint16_t copycnt;
    uint16_t copydone;
    uint8_t interleave;
    uint8_t mode;
} fastline_t;

#define M_INTERLEAVE 0x1

static fastline_t fastline_h8;

int screen_hal_scale_h8_2x2 (lcd_t *cfg, copybuf_t *copybuf, int interleave)
{
    screen_t *dest = &copybuf->dest;
    screen_t *src = &copybuf->src;
    int dw = dest->width, sw = src->width;
    const int pix_size = 1;

    void *dptr;
    void *sptr;

    copybuf->next = NULL;
    dest->width = 1;
    src->width = 1;
    src->y = 0;
    dest->y = 0;
    fastline_h8.copybuf = *copybuf;
    fastline_h8.dest_leg = dw;
    fastline_h8.src_leg = sw;
    fastline_h8.copydone = 0;
    if (interleave) {
        fastline_h8.mode |= M_INTERLEAVE;
        fastline_h8.copycnt = dw;
        fastline_h8.interleave = 1 - fastline_h8.interleave;
    } else {
        fastline_h8.copycnt = dw * 2;
    }

    dptr = __screen_2_ptr(dest, pix_size);
    sptr = __screen_2_ptr(src, pix_size);

    fastline_h8.dptr = dptr;
    fastline_h8.sptr = sptr;

    __screen_hal_copy_setup_M2M(GET_VHAL_CTXT(cfg), GFX_COLOR_MODE_CLUT, GFX_COLOR_MODE_CLUT,
                            0xff, src->width, dw * 2, sw);

    GET_VHAL_CTXT(cfg)->state = V_STATE_COPYFAST;

    return screen_hal_copy_start(cfg, src->width, src->height, dptr, sptr);
}

static inline int
screen_hal_copy_h8_next (screen_hal_ctxt_t *ctxt)
{
    uint32_t dptr_off = 0;
    screen_t *dest = &fastline_h8.copybuf.dest;
    screen_t *src = &fastline_h8.copybuf.src;

    if (fastline_h8.copycnt == 0) {
        ctxt->state = V_STATE_IDLE;
        return 0;
    }

    fastline_h8.copycnt--;
    fastline_h8.copydone++;

    if (fastline_h8.mode & M_INTERLEAVE) {
        if (fastline_h8.interleave) {
            dptr_off = fastline_h8.dest_leg;
        }
        if ((fastline_h8.copydone & 0x1) == 0) {
            src->x++;
            fastline_h8.sptr = (void *)((uint32_t)fastline_h8.sptr + 1);
        }
        fastline_h8.dptr = (void *)((uint32_t)fastline_h8.dptr + 1);
    } else switch (fastline_h8.copydone & 0x3) {
        case 0x3:
        case 0x1:
            dptr_off = fastline_h8.dest_leg;
        break;
        case 0x2:
            src->x++;
            fastline_h8.sptr = (void *)((uint32_t)fastline_h8.sptr + 1);
        case 0x0:
            dest->y = 0;
            dest->x++;
            fastline_h8.dptr = (void *)((uint32_t)fastline_h8.dptr + 1);
        break;
    }

    return screen_hal_copy_start(ctxt->lcd, src->width, src->height,
                                (void *)((uint32_t)fastline_h8.dptr + dptr_off),
                                fastline_h8.sptr);
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
    assert(screen_mode2pixdeep[ctxt->lcd->config.colormode]);
    ret = screen_hal_copy_m2m(ctxt->lcd, buf, screen_mode2pixdeep[ctxt->lcd->config.colormode]);
    ctxt->lcd->config.alloc.free(buf);
    ctxt->bufq = bufq;
    return ret;
}

static copybuf_t *
screen_hal_copybuf_alloc (screen_hal_ctxt_t *ctxt, screen_t *dest, screen_t *src)
{
    copybuf_t *buf = (copybuf_t *)ctxt->lcd->config.alloc.malloc(sizeof(*buf));

    d_memcpy(&buf->dest, dest, sizeof(buf->dest));
    d_memcpy(&buf->src, src, sizeof(buf->src));

    buf->next = ctxt->bufq;
    ctxt->bufq = buf;
    return buf;
}

static inline void screen_dma2d_irq_hdlr (screen_hal_ctxt_t *ctxt)
{
    switch (ctxt->state) {

        case V_STATE_QCOPY:
                if (ctxt->bufq) {
                    screen_hal_copy_next(ctxt);
                    break;
                }
                GET_VHAL_CTXT(ctxt->lcd)->state = V_STATE_IDLE;
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

d_bool screen_hal_ts_available (void)
{
    if (BSP_LCD_UseHDMI()) {
        return d_false;
    }
    return d_true;
}

int screen_hal_smp_avail (void)
{
    return 1;
}

hal_smp_task_t *screen_hal_sched_task (void (*func) (void *), gfx_2d_buf_t *dest, gfx_2d_buf_t *src)
{
    gfx_2d_buf_pair_t buf = {*dest, *src};

    return hal_smp_sched_task(func, &buf, sizeof(buf));
}

void DMA2D_IRQHandler(void)
{
    lcd_t *lcd = LCD();
    HAL_DMA2D_IRQHandler(GET_VHAL_DMA2D(lcd));
    GET_VHAL_CTXT(lcd)->busy = 0;
}

void DMA2D_XferCpltCallback (struct __DMA2D_HandleTypeDef * hdma2d)
{
    lcd_t *lcd = LCD();
    if (GET_VHAL_DMA2D(lcd) == hdma2d) {
        screen_dma2d_irq_hdlr(GET_VHAL_CTXT(lcd));
    }
}

void LTDC_IRQHandler (void)
{
    lcd_t *lcd = LCD();
    HAL_LTDC_IRQHandler(GET_VHAL_LTDC(lcd));
}

void DSI_IRQHandler(void)
{
    lcd_t *lcd = LCD();
    HAL_DSI_IRQHandler(GET_VHAL_DSI(lcd));
}

void HAL_LTDC_ReloadEventCallback(LTDC_HandleTypeDef *hltdc)
{
    lcd_t *lcd = LCD();
    GET_VHAL_CTXT(lcd)->waitreload = 0;
}

void HAL_DSI_EndOfRefreshCallback(DSI_HandleTypeDef *hdsi)
{
    lcd_t *lcd = LCD();
    GET_VHAL_CTXT(lcd)->waitreload = 0;
}


