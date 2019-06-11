#ifndef _LCD_MAIN_H
#define _LCD_MAIN_H

#include "gfx.h"
#include "stm32f769i_discovery_lcd.h"

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
    uint32_t width, height;
} screen_t;

typedef struct {
    void *fb_mem;
    void *lay_mem[LCD_MAX_LAYER];
    uint32_t fb_size;
    uint32_t lay_size;
    LCD_LayerCfgTypeDef lay_halcfg;
    lcd_layers_t active_lay_idx;
    uint8_t lay_cnt;
} lcd_wincfg_t;


/*---------------------------------------------------------------------*
 *  function prototypes                                                *
 *---------------------------------------------------------------------*/
void screen_init (void);
uint32_t screen_total_mem_avail_kb (void);
int screen_win_cfg (lcd_wincfg_t *cfg, screen_t *screen, uint32_t colormode, int layers_cnt);
void screen_set_clut (pal_t *palette, uint32_t clut_num_entries);
void screen_update (screen_t *in);
void screen_direct (screen_t *s);
void screen_deinit (void);
void screen_vsync (void);
void screen_ts_align (int *x, int *y);


#endif /*_LCD_MAIN_H*/
