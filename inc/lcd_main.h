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

typedef struct {
    void *buf;
    int x, y;
    int width, height;
    uint8_t colormode;
} screen_t;

typedef struct {
    void *(*malloc) (uint32_t);
    void (*free) (void *);
} screen_alloc_t;

typedef struct {
    screen_alloc_t alloc;
    int16_t res_x;
    int16_t res_y;
    uint8_t laynum;
    uint8_t colormode;
    uint8_t clockpresc;
    uint8_t hwaccel: 1,
            reserved: 7;
} screen_conf_t;

typedef struct bsp_video_api_s {
    bspdev_t dev;
    void (*get_wh) (screen_t *);
    uint32_t (*mem_avail) (void);
    int (*win_cfg) (screen_conf_t *);
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
#define vid_priv_ctl        BSP_VID_API(dev.priv)
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
int vid_config (screen_conf_t *);
void vid_set_clut (void *palette, uint32_t clut_num_entries);
void vid_upate (screen_t *in);
void vid_direct (screen_t *s);
void vid_vsync (void);
void vid_ptr_align (int *x, int *y);
int vid_priv_ctl (int c, void *v);
#endif

enum {
    VCTL_NOP,
    VCTL_VRAM_ALLOC,
    VCTL_VRAM_COPY,
    VCTL_MAX,
};

#endif /*_LCD_MAIN_H*/
