#ifndef __LCD_INT_H__
#define __LCD_INT_H__

#ifdef __cplusplus
    extern "C" {
#endif

#define LCD_MAX_SCALE 3

typedef enum
{
    LCD_BACKGROUND,
    LCD_FOREGROUND,
    LCD_MAX_LAYER,
} lcd_layers_t;

typedef void (*screen_update_handler_t) (screen_t *in);

typedef struct copybuf_s {
    struct copybuf_s *next;
    screen_t dest, src;
} copybuf_t;

typedef struct {
    uint16_t w, h;
    uint16_t pend_idx, rd_idx;
    uint32_t bytes_total;
    uint32_t bytes_frame;
    void *base;
    void *frame[LCD_MAX_LAYER];
    void *buf;
    void *frame_ext;
} framebuf_t;

typedef struct {
    screen_conf_t config;
    framebuf_t fb;
    screen_update_handler_t scaler;
    void *hal_ctxt;
    void *blut;
    uint32_t bilinear: 1;
    int cvar_have_smp;
} lcd_t;

void vid_direct_copy (gfx_2d_buf_t *dest2d, gfx_2d_buf_t *src2d);
void _screen_hal_reload_layer (lcd_t *lcd);
int screen_hal_init (int init);
void screen_hal_attach (lcd_t *cfg);
void *screen_hal_set_config (lcd_t *lcd, int x, int y,
                                            int w, int h, uint8_t colormode);
void screen_hal_set_clut (lcd_t *lcd, void *_buf, int size, int layer);
int screen_hal_set_keying (lcd_t *lcd, uint32_t color, int layer);
void screen_hal_sync (lcd_t *lcd, int wait);
void screen_hal_post_sync (lcd_t *lcd);
int screen_hal_copy_m2m (lcd_t *lcd, copybuf_t *copybuf, uint8_t pix_bytes);
int screen_hal_scale_h8_2x2 (lcd_t *lcd, copybuf_t *copybuf, int interleave);
int screen_gfx8_copy_line (lcd_t *lcd, void *dest, void *src, int w);
int screen_gfx8888_copy (lcd_t *lcd, gfx_2d_buf_t *dest, gfx_2d_buf_t *src);
int screen_hal_post_config (lcd_t *lcd);


static inline void screen_hal_reload_layer (lcd_t *lcd)
{
    if (lcd->config.laynum < 2) {
        return;
    }
    _screen_hal_reload_layer(lcd);
}

void vid_line_event_callback (lcd_t *lcd);

d_bool screen_hal_ts_available (void);

extern uint32_t lcd_x_size_var;
extern uint32_t lcd_y_size_var;

extern uint32_t bsp_lcd_width;
extern uint32_t bsp_lcd_height;

extern const lcd_layers_t layer_switch[LCD_MAX_LAYER];
extern const uint32_t screen_mode2pixdeep[GFX_COLOR_MODE_MAX];
extern lcd_t *g_lcd_inst;

#define LCD() g_lcd_inst

#ifdef __cplusplus
    }
#endif

#endif /*__LCD_INT_H__*/

