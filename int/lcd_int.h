#ifndef __LCD_INT_H__
#define __LCD_INT_H__

#include <lcd_main.h>

#define LCD_MAX_SCALE 3

typedef enum
{
    LCD_BACKGROUND,
    LCD_FOREGROUND,
    LCD_MAX_LAYER,
} lcd_layers_t;

typedef struct copybuf_s {
    struct copybuf_s *next;
    screen_t dest, src;
} copybuf_t;

typedef struct {
    void *hal_ctxt;
    screen_conf_t config;
    uint16_t w, h;
    void *usermem;
    void *fb_mem;
    void *lay_mem[LCD_MAX_LAYER];
    uint32_t fb_size;
    uint32_t lay_size;
    lcd_layers_t ready_lay_idx;
    __IO uint8_t waitreload;
    uint8_t poll;
} lcd_wincfg_t;

typedef void (*screen_update_handler_t) (screen_t *in);

lcd_layers_t screen_hal_set_layer (lcd_wincfg_t *cfg);
int screen_hal_init (int init, uint8_t clockpresc);
void screen_hal_attach (lcd_wincfg_t *cfg);
void *screen_hal_set_config (lcd_wincfg_t *cfg, int x, int y,
                                            int w, int h, uint8_t colormode);
void screen_hal_set_clut (lcd_wincfg_t *cfg, void *_buf, int size, int layer);
void screen_hal_sync (lcd_wincfg_t *cfg, int wait);
int screen_hal_copy (lcd_wincfg_t *cfg, copybuf_t *copybuf, uint8_t pix_bytes);
int screen_hal_copy_h8 (lcd_wincfg_t *cfg, copybuf_t *copybuf, int interleave);

static inline void screen_hal_layreload (lcd_wincfg_t *cfg)
{
    if (cfg->config.laynum < 2) {
        return;
    }
    cfg->ready_lay_idx = screen_hal_set_layer(cfg);
}

extern uint32_t lcd_x_size_var;
extern uint32_t lcd_y_size_var;

extern int bsp_lcd_width;
extern int bsp_lcd_height;

extern const lcd_layers_t layer_switch[LCD_MAX_LAYER];
extern const uint32_t screen_mode2pixdeep[GFX_COLOR_MODE_MAX];
extern lcd_wincfg_t *lcd_active_cfg;

int vid_copy (screen_t *dest, screen_t *src);

#endif /*__LCD_INT_H__*/

