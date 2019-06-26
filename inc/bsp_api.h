#ifndef __BSP_API_H__
#define __BSP_API_H__

#include <arch.h>

#include <nvic.h>

#if defined(APPLICATION)
#define BSP_INDIR_API 1
#elif defined(BSP_DRIVER)
#define BSP_INDIR_API 0
#else
#define BSP_INDIR_API 0
#endif

typedef struct bspapi_s {
    void *io;
    void *vid;
    void *sfx;
    void *cd;
    void *sys;
    void *dbg;
    void *in;
    void *gui;
} bspapi_t;

typedef struct {
    int (*init) (void);
    void (*deinit) (void);
    int (*conf) (const char *);
    const char *(*info) (void);
    int (*priv) (int c, void *v);
} bspdev_t;

extern bspapi_t *g_bspapi;

#endif /*__BSP_API_H__*/
