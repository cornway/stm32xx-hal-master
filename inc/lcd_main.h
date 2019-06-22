#ifndef _LCD_MAIN_H
#define _LCD_MAIN_H

#include "gfx.h"
#include <bsp_api.h>

/*---------------------------------------------------------------------*
 *  additional includes                                                *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  global definitions                                                 *
 *---------------------------------------------------------------------*/

/*---------------------------------------------------------------------*
 *  type declarations                                                  *
 *---------------------------------------------------------------------*/

typedef enum
{
    LCD_BACKGROUND,
    LCD_FOREGROUND,
    LCD_MAX_LAYER,
} lcd_layers_t;

typedef struct {
    pix_t *buf;
    int width, height;
} screen_t;

typedef struct {
    void *fb_mem;
    void *lay_mem[LCD_MAX_LAYER];
    uint32_t fb_size;
    uint32_t lay_size;
    void *lay_halcfg;
    lcd_layers_t active_lay_idx;
    uint16_t w, h;
    uint8_t lay_cnt;
} lcd_wincfg_t;

typedef void *(*lcd_mem_malloc_t) (uint32_t size);


/*---------------------------------------------------------------------*
 *  function prototypes                                                *
 *---------------------------------------------------------------------*/
#if BSP_INDIR_API

#define screen_init   g_bspapi->vid.init
#define screen_deinit   g_bspapi->vid.deinit
#define screen_get_wh   g_bspapi->vid.get_wh
#define screen_total_mem_avail_kb   g_bspapi->vid.mem_avail
#define screen_win_cfg   g_bspapi->vid.win_cfg
#define screen_set_clut   g_bspapi->vid.set_clut
#define screen_update   g_bspapi->vid.update
#define screen_direct   g_bspapi->vid.direct
#define screen_vsync   g_bspapi->vid.vsync
#define screen_ts_align   g_bspapi->vid.input_align

#else
void screen_init (void);
void screen_deinit (void);
void screen_get_wh (screen_t *s);
uint32_t screen_total_mem_avail_kb (void);
int screen_win_cfg (lcd_mem_malloc_t __malloc, lcd_wincfg_t *cfg, screen_t *screen, uint32_t colormode, int layers_cnt);
void screen_set_clut (pal_t *palette, uint32_t clut_num_entries);
void screen_update (screen_t *in);
void screen_direct (screen_t *s);
void screen_vsync (void);
void screen_ts_align (int *x, int *y);
#endif

#endif /*_LCD_MAIN_H*/
