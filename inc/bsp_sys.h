#include <bsp_api.h>

typedef struct bsp_sytem_api_s {
    bspdev_t dev;
    void (*fatal) (char *, ...);
    void (*prof_enter) (const char *, int);
    void (*prof_exit) (const char *, int);
    void (*prof_reset) (void);
    void (*prof_init) (void);
    void (*prof_deinit) (void);
} bsp_sytem_api_t;

#define BSP_SYS_API(func) ((bsp_sytem_api_t *)(g_bspapi->sys))->func

#if BSP_INDIR_API

#define sys_init          BSP_SYS_API(dev.init)
#define sys_deinit        BSP_SYS_API(dev.deinit)
#define sys_conf          BSP_SYS_API(dev.conf)
#define sys_info          BSP_SYS_API(dev.info)
#define sys_priv          BSP_SYS_API(dev.priv)

#define fatal_error         BSP_SYS_API(fatal)
#define _profiler_enter     BSP_SYS_API(prof_enter)
#define _profiler_exit      BSP_SYS_API(prof_exit)
#define profiler_reset      BSP_SYS_API(prof_reset)
#define profiler_init       BSP_SYS_API(prof_init)
#define profiler_deinit       BSP_SYS_API(prof_deinit)

#else /*BSP_INDIR_API*/

void fatal_error (char *message, ...);
void _profiler_enter (const char *func, int line);
void _profiler_exit (const char *func, int line);
void profiler_reset (void);
void profiler_init (void);
void profiler_deinit (void);

#endif /*BSP_INDIR_API*/

#define profiler_enter() _profiler_enter(__func__, __LINE__)
#define profiler_exit() _profiler_exit(__func__, __LINE__)

