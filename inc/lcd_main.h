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

/*---------------------------------------------------------------------*
 *  function prototypes                                                *
 *---------------------------------------------------------------------*/
void screen_init (void);
uint32_t screen_total_mem_avail_kb (void);
void screen_win_cfg (screen_t *screen);
void screen_set_clut (pal_t *palette, uint32_t clut_num_entries);
void screen_update (screen_t *in);

#endif /*_LCD_MAIN_H*/
