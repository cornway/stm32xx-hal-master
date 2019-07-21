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
    LCD_DIRECT,
    LCD_MAX_LAYER,
} lcd_layers_t;

typedef struct {
    void *buf;
    int x, y;
    int width, height;
    uint8_t colormode;
} screen_t;

typedef void *(*lcd_mem_malloc_t) (uint32_t size);

typedef struct bsp_video_api_s {
    bspdev_t dev;
    void (*get_wh) (screen_t *);
    uint32_t (*mem_avail) (void);
    int (*win_cfg) (lcd_mem_malloc_t, void *, screen_t *, uint32_t, int);
    void (*set_clut) (void *, uint32_t);
    void (*update) (screen_t *);
    void (*direct) (screen_t *);
    void (*vsync) (void);
    void (*input_align) (int *, int *);
} bsp_video_api_t;

#define BSP_VID_API(func) ((bsp_video_api_t *)(g_bspapi->vid))->func

#if BSP_INDIR_API

#define vid_init            BSP_VID_API(dev.init)
#define vid_deinit          BSP_VID_API(dev.deinit)
#define vid_config          BSP_VID_API(dev.conf)
#define vid_info            BSP_VID_API(dev.info)
#define vid_priv            BSP_VID_API(dev.priv)
#define vid_wh              BSP_VID_API(get_wh)
#define vid_mem_avail       BSP_VID_API(mem_avail)
#define vid_config          BSP_VID_API(win_cfg)
#define vid_set_clut        BSP_VID_API(set_clut)
#define vid_upate           BSP_VID_API(update)
#define vid_direct          BSP_VID_API(direct)
#define vid_vsync           BSP_VID_API(vsync)
#define vid_ptr_align       BSP_VID_API(input_align)

#else
int vid_init (void);
void vid_deinit (void);
void vid_wh (screen_t *s);
uint32_t vid_mem_avail (void);
int vid_config (lcd_mem_malloc_t __malloc, void *cfg,
                    screen_t *screen, uint32_t colormode, int layers_cnt);
void vid_set_clut (void *palette, uint32_t clut_num_entries);
void vid_upate (screen_t *in);
void vid_direct (screen_t *s);
void vid_vsync (void);
void vid_ptr_align (int *x, int *y);
#endif

#endif /*_LCD_MAIN_H*/
