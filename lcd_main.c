/* Includes ------------------------------------------------------------------*/

#if defined(BSP_DRIVER)

#include "int/lcd_int.h"
#include "gui/colors.h"

#include <lcd_main.h>
#include <misc_utils.h>
#include <debug.h>
#include <heap.h>
#include <mpu.h>
#include <bsp_sys.h>

static void screen_copy_1x1_SW (screen_t *in);
static void screen_copy_1x1_HW (screen_t *in);
static void screen_copy_2x2_HW (screen_t *in);
static void screen_copy_2x2_8bpp (screen_t *in);
static void screen_copy_3x3_8bpp (screen_t *in);

static screen_update_handler_t vid_scaler_handler = NULL;

int bsp_lcd_width = -1;
int bsp_lcd_height = -1;

static lcd_wincfg_t lcd_def_cfg;
lcd_wincfg_t *lcd_active_cfg = NULL;

const lcd_layers_t layer_switch[LCD_MAX_LAYER] =
{
    [LCD_BACKGROUND] = LCD_FOREGROUND,
    [LCD_FOREGROUND] = LCD_BACKGROUND,
};

static const char *screen_mode2txt_map[] =
{
    [GFX_COLOR_MODE_CLUT] = "*CLUT L8*",
    [GFX_COLOR_MODE_RGB565] = "*RGB565*",
    [GFX_COLOR_MODE_RGBA8888] = "*ARGB8888*",
};

const uint32_t screen_mode2pixdeep[GFX_COLOR_MODE_MAX] =
{
    [GFX_COLOR_MODE_CLUT]       = 1,
    [GFX_COLOR_MODE_RGB565]     = 2,
    [GFX_COLOR_MODE_RGBA8888]   = 4,
};

int vid_init (void)
{
    return screen_hal_init(1);
}

static void vid_mpu_release (lcd_wincfg_t *cfg)
{
    if (cfg->raw_mem) {
        mpu_unlock((arch_word_t)cfg->raw_mem, cfg->fb_size + cfg->extmem_size);
    }
}

static void vid_release (lcd_wincfg_t *cfg)
{
    if (NULL == cfg) {
        return;
    }
    if (cfg->raw_mem) {
        d_memset(cfg->raw_mem, 0, cfg->fb_size);
        heap_free(cfg->raw_mem);
    }
    if (cfg->extmem) {
        d_memset(cfg->extmem, 0, cfg->extmem_size);
        heap_free(cfg->extmem);
    }
    vid_mpu_release(cfg);
    d_memset(cfg, 0, sizeof(*cfg));
}

void vid_deinit (void)
{
    dprintf("%s() :\n", __func__);
    screen_hal_init(0);
    vid_release(lcd_active_cfg);
    lcd_active_cfg = NULL;
}

int vid_set_keying (uint32_t color, int layer)
{
    if (layer >= lcd_active_cfg->config.laynum || layer < 0) {
        return -1;
    }
    screen_hal_set_keying(lcd_active_cfg, color, (lcd_layers_t)layer);
    return 0;
}

void vid_wh (screen_t *s)
{
    assert(lcd_active_cfg && s);
    s->width = lcd_active_cfg->w;
    s->height = lcd_active_cfg->h;
}

static void *
vid_create_framebuffer (screen_alloc_t *alloc, lcd_wincfg_t *cfg,
                       uint32_t w, uint32_t h, uint32_t pixel_deep, uint32_t layers_cnt)
{
    const uint32_t lay_mem_align = 64;
    const uint32_t lay_mem_size = (w * h * pixel_deep) + lay_mem_align;
    uint32_t fb_size, i;
    uint8_t *fb_mem;
    const char *mpu_conf = NULL;

    assert(layers_cnt <= LCD_MAX_LAYER);

    fb_size = lay_mem_size * layers_cnt;

    if (cfg->raw_mem) {
        assert(0);
    }

    fb_mem = alloc->malloc(fb_size);
    if (!fb_mem) {
        dprintf("%s() : failed to alloc %u bytes\n", __func__, fb_size);
        return NULL;
    }
    /*To prevent lcd panel 'in-burning', or 'ghosting' etc.. -
      lets fill fb with white color
    */
    d_memset(fb_mem, COLOR_WHITE, fb_size);

    cfg->raw_mem = fb_mem;
    cfg->fb_size = fb_size;
    cfg->config.laynum = layers_cnt;
    cfg->w = w;
    cfg->h = h;

    for (i = 0; i < layers_cnt; i++) {
        cfg->lay_mem[i] = (uint8_t *)(ROUND_UP((arch_word_t)fb_mem, lay_mem_align));
        fb_mem = fb_mem + lay_mem_align + (fb_size / layers_cnt);
    }

    switch (cfg->config.cachealgo) {
        case VID_CACHE_NONE:
            /*Non-cacheable*/
            mpu_conf = "-xscb";
        break;
        case VID_CACHE_WTWA:
            /*Write through, no write allocate*/
            mpu_conf = "-xsb";
        break;
        case VID_CACHE_WBNWA:
            /*Write-back, no write allocate*/
            mpu_conf = "-xs";
        break;
    }

    i = fb_size;
    if (mpu_conf && mpu_lock((arch_word_t)cfg->raw_mem, &fb_size, mpu_conf) < 0) {
        dprintf("%s() : MPU region config failed\n", __func__);
    } else if (i != fb_size) {

        assert(i < fb_size);
        i = fb_size - i;
        dprintf("%s() : MPU region requires extra space(padding) +[0x%08x] bytes, allocating\n",
                __func__, i);
        cfg->extmem = alloc->malloc(i);
        cfg->extmem_size = i;
        assert(cfg->extmem);
    }
    return cfg->raw_mem;
}

void vid_ptr_align (int *x, int *y)
{
}

static screen_update_handler_t vid_get_scaler (int scale, uint8_t colormode)
{
    screen_update_handler_t h = NULL;

    if (colormode == GFX_COLOR_MODE_CLUT) {
        switch (scale) {
            case 1:
                if (lcd_active_cfg->config.hwaccel) {
                    h = screen_copy_1x1_HW;
                } else {
                    h = screen_copy_1x1_SW;
                }
            case 2:
                if (lcd_active_cfg->config.hwaccel) {
                    h = screen_copy_2x2_HW;
                } else {
                    h = screen_copy_2x2_8bpp;
                }
            break;
            case 3:
                h = screen_copy_3x3_8bpp;
            break;

            default:
                fatal_error("%s() : Scale not supported yet!\n", __func__);
            break;
        }
    } else {
        switch (scale) {
            default:
                h = screen_copy_1x1_SW;
            break;
        }
    }
    return h;
}

static lcd_wincfg_t *vid_new_winconfig (lcd_wincfg_t *cfg)
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

    cfg = vid_new_winconfig(lcd_active_cfg);
    if ((lcd_wincfg_t *)cfg == lcd_active_cfg) {
        return 0;
    }
    lcd_active_cfg = cfg;
    lcd_active_cfg->config = *conf;
    scale = vid_set_win_size(conf->res_x, conf->res_y, bsp_lcd_width, bsp_lcd_height, &x, &y, &w, &h);

    vid_scaler_handler = vid_get_scaler(scale, conf->colormode);

    lcd_x_size_var = w;
    lcd_y_size_var = h;

    if (!vid_create_framebuffer(&conf->alloc, cfg, w, h, screen_mode2pixdeep[conf->colormode], conf->laynum)) {
        return -1;
    }

    if (screen_hal_set_config(cfg, x, y, w, h, conf->colormode)) {
        return 0;
    }
    return -1;
}

uint32_t vid_mem_avail (void)
{
    assert(lcd_active_cfg);
    return ((lcd_active_cfg->fb_size) / 1024);
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

void vid_get_screen (screen_t *screen, int laynum)
{
    screen->width = lcd_active_cfg->w;
    screen->height = lcd_active_cfg->h;
    screen->buf = (void *)lcd_active_cfg->lay_mem[laynum];
    screen->x = 0;
    screen->y = 0;
    screen->colormode = lcd_active_cfg->config.colormode;
    screen->alpha = 0xff;
}

void vid_get_ready_screen (screen_t *screen)
{
    vid_get_screen(screen, lcd_active_cfg->ready_lay_idx);
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
        vid_scaler_handler(in);
    }
}

void vid_direct (int x, int y, screen_t *s, int laynum)
{
    screen_t screen;

    vid_vsync(1);
    if (lcd_active_cfg->config.laynum <= laynum) {
        
    }
    if (laynum < 0) {
        vid_get_ready_screen(&screen);
    } else {
        vid_get_screen(&screen, laynum);
    }
    screen.x = x;
    screen.y = y;
    screen.alpha = 0xff;
    screen.colormode = s->colormode;
    vid_copy(&screen, s);
}

void vid_print_info (void)
{
    assert(lcd_active_cfg);

    dprintf("Video :\n");
    dprintf("width=%4.3u height=%4.3u\n", lcd_active_cfg->w, lcd_active_cfg->h);
    dprintf("layers = %u, color mode = %s \n",
             lcd_active_cfg->config.laynum, screen_mode2txt_map[lcd_active_cfg->config.colormode]);
    dprintf("framebuffer = <0x%p> 0x%08x bytes\n", lcd_active_cfg->raw_mem, lcd_active_cfg->fb_size);
}

static void
vid_copy_SW (screen_t *dest, screen_t *src)
{
    d_memcpy(dest->buf, src->buf, src->width * src->height);
}

static int
vid_copy_HW (screen_t *dest, screen_t *src)
{
    copybuf_t copybuf = {NULL, *dest, *src};
    uint8_t colormode = lcd_active_cfg->config.colormode;

    vid_vsync(0);
    return screen_hal_copy(lcd_active_cfg, &copybuf, screen_mode2pixdeep[colormode]);
}

int vid_copy (screen_t *dest, screen_t *src)
{
    if (lcd_active_cfg->config.hwaccel) {
        return vid_copy_HW(dest, src);
    }
    vid_copy_SW(dest, src);
    return 0;
}

int vid_gfx2d_direct (int x, int y, gfx_2d_buf_t *src, int laynum)
{
    gfx_2d_buf_t dest;
    screen_t screen;

    vid_vsync(1);
    vid_get_ready_screen(&screen);

    screen.x = x;
    screen.y = y;
    __screen_to_gfx2d(&dest, &screen);
    return screen_gfx8888_copy(lcd_active_cfg, &dest, src);
}

static void
screen_copy_1x1_SW (screen_t *in)
{
    screen_t screen;

    vid_vsync(1);
    vid_get_ready_screen(&screen);
    vid_copy(&screen, in);
}

static void
screen_copy_1x1_HW (screen_t *in)
{
    screen_t screen;
    copybuf_t copybuf;

    vid_vsync(1);
    vid_get_ready_screen(&screen);
    copybuf.dest = screen;
    copybuf.src = *in;

    screen_hal_copy(lcd_active_cfg, &copybuf, 1);
}

static void
screen_copy_2x2_HW (screen_t *in)
{
    screen_t screen;
    copybuf_t copybuf;

    vid_vsync(1);
    vid_get_ready_screen(&screen);
    copybuf.dest = screen;
    copybuf.src = *in;

    copybuf.dest.colormode = lcd_active_cfg->config.colormode;
    copybuf.src.colormode = lcd_active_cfg->config.colormode;
    screen_hal_scale_h8_2x2(lcd_active_cfg, &copybuf, lcd_active_cfg->config.hwaccel > 1);
}

static void
screen_copy_2x2_8bpp (screen_t *in)
{
    screen_t screen;
    gfx_2d_buf_t dest, src;

    vid_vsync(1);
    vid_get_ready_screen(&screen);

    __screen_to_gfx2d(&dest, &screen);
    __screen_to_gfx2d(&src, in);
    gfx2d_scale2x2_8bpp(&dest, &src);
}

static void
screen_copy_3x3_8bpp(screen_t *in)
{
    screen_t screen;
    gfx_2d_buf_t dest, src;

    vid_vsync(1);
    vid_get_ready_screen(&screen);

    __screen_to_gfx2d(&dest, &screen);
    __screen_to_gfx2d(&src, in);
    gfx2d_scale3x3_8bpp(&dest, &src);
}

#endif

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

