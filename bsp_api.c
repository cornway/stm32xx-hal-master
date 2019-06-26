#include <bsp_api.h>
#include <debug.h>
#include <audio_main.h>
#include <input_main.h>
#include <misc_utils.h>
#include <dev_io.h>
#include <lcd_main.h>
#include <heap.h>
#include <bsp_sys.h>
#include <gui.h>

bspapi_t *g_bspapi;

typedef struct {
    uint32_t size;
    bspapi_t api;
    struct bsp_io_api_s     io;
    struct bsp_video_api_s  vid;
    struct bsp_sfx_api_s    sfx;
    struct bsp_cd_api_s     cd;
    struct bsp_sytem_api_s  sys;
    struct bsp_debug_api_s  dbg;
    struct bsp_input_api_s  in;
    struct bsp_gui_api_s    gui;
} bsp_api_int_t;

#if !BSP_INDIR_API

void dev_init_stub (void)
{
    dprintf("%s()\n", __func__);
}

void dev_deinit_stub (void)
{
    dprintf("%s()\n", __func__);
}

void dev_conf_stub (const char *arg)
{
    dprintf("%s() : \'%s\'\n", __func__, arg);
}

const char *dev_info_stub (void)
{
    dprintf("%s()\n", __func__);
    return "stub\n";
}

int dev_priv_stub (int c, void *v)
{
    dprintf("%s()\n", __func__);
}

extern int dev_init (void (*userinit) (void));

#define API_SETUP(api, module) \
(api)->api.module = &(api)->module;

bspapi_t *bsp_api_attach (void)
{
    arch_word_t *ptr, size;
    bsp_api_int_t *api;

    arch_get_shared(&ptr, &size);
    assert(ptr);
    assert(size >= sizeof(*api));

    memset(ptr, 0, size);

    api = (bsp_api_int_t *)ptr;
    API_SETUP(api, io);
    API_SETUP(api, vid);
    API_SETUP(api, sfx);
    API_SETUP(api, cd);
    API_SETUP(api, sys);
    API_SETUP(api, dbg);
    API_SETUP(api, in);
    API_SETUP(api, gui);
    api->size = sizeof(*api);
    g_bspapi = &api->api;

    BSP_IO_API(dev.init)     = dev_io_init;
    BSP_IO_API(dev.deinit)   = dev_io_deinit;
    BSP_IO_API(dev.conf)    = dev_conf_stub;
    BSP_IO_API(dev.info)    = dev_info_stub;
    BSP_IO_API(dev.priv)    = dev_priv_stub;
    BSP_IO_API(open)        = d_open;
    BSP_IO_API(size)        = d_size;
    BSP_IO_API(tell)        = d_tell;
    BSP_IO_API(close)       = d_close;
    BSP_IO_API(unlink)      = d_unlink;
    BSP_IO_API(seek)        = d_seek;
    BSP_IO_API(eof)         = d_eof;
    BSP_IO_API(read)        = d_read;
    BSP_IO_API(gets)        = d_gets;
    BSP_IO_API(getc)        = d_getc;
    BSP_IO_API(write)       = d_write;
    BSP_IO_API(printf)      = d_printf;
    BSP_IO_API(mkdir)       = d_mkdir;
    BSP_IO_API(opendir)     = d_opendir;
    BSP_IO_API(closedir)    = d_closedir;
    BSP_IO_API(readdir)     = d_readdir;
    BSP_IO_API(time)        = d_time;
    BSP_IO_API(dirlist)     = d_dirlist;
    BSP_IO_API(var_reg)     = d_dvar_reg;
    BSP_IO_API(var_int32)   = d_dvar_int32;
    BSP_IO_API(var_float)   = d_dvar_float;
    BSP_IO_API(var_str)     = d_dvar_str;

    BSP_VID_API(dev.init)   = vid_init;
    BSP_VID_API(dev.deinit) = vid_deinit;
    BSP_VID_API(dev.conf)    = dev_conf_stub;
    BSP_VID_API(dev.info)    = dev_info_stub;
    BSP_VID_API(dev.priv)    = dev_priv_stub;
    BSP_VID_API(get_wh)     = vid_wh;
    BSP_VID_API(mem_avail)  = vid_mem_avail;
    BSP_VID_API(win_cfg)    = vid_config;
    BSP_VID_API(set_clut)   = vid_set_clut;
    BSP_VID_API(update)     = vid_upate;
    BSP_VID_API(direct)     = vid_direct;
    BSP_VID_API(vsync)      = vid_vsync;
    BSP_VID_API(input_align) = vid_ptr_align;

    BSP_SFX_API(dev.init)   = audio_init;
    BSP_SFX_API(dev.deinit) = audio_deinit;
    BSP_SFX_API(dev.conf)    = dev_conf_stub;
    BSP_SFX_API(dev.info)    = dev_info_stub;
    BSP_SFX_API(dev.priv)    = dev_priv_stub;
    BSP_SFX_API(mixer_ext)  = audio_mixer_ext;
    BSP_SFX_API(play)       = audio_play_channel;
    BSP_SFX_API(stop)       = audio_stop_channel;
    BSP_SFX_API(pause)      = audio_pause;
    BSP_SFX_API(stop_all)   = audio_stop_all;
    BSP_SFX_API(is_play)    = audio_is_playing;
    BSP_SFX_API(check)      = audio_chk_priority;
    BSP_SFX_API(set_pan)    = audio_set_pan;
    BSP_SFX_API(sample_volume) = audio_sample_vol;
    BSP_SFX_API(tickle)     = audio_update;
    BSP_SFX_API(irq_save)   = audio_irq_save;
    BSP_SFX_API(irq_restore) = audio_irq_restore;

    BSP_SFX_API(wave_open)  = audio_open_wave;
    BSP_SFX_API(wave_size)  = audio_wave_size;
    BSP_SFX_API(wave_cache) = audio_cache_wave;
    BSP_SFX_API(wave_close) = audio_wave_close;

    BSP_CD_API(dev.init)    = dev_init_stub;
    BSP_CD_API(dev.deinit)  = dev_deinit_stub;
    BSP_CD_API(dev.conf)    = dev_conf_stub;
    BSP_CD_API(dev.info)    = dev_info_stub;
    BSP_CD_API(dev.priv)    = dev_priv_stub;
    BSP_CD_API(play)        = cd_play_name;
    BSP_CD_API(pause)       = cd_pause;
    BSP_CD_API(resume)      = cd_resume;
    BSP_CD_API(stop)        = cd_stop;
    BSP_CD_API(volume)      = cd_volume;
    BSP_CD_API(getvol)      = cd_getvol;
    BSP_CD_API(playing)     = cd_playing;

    BSP_SYS_API(dev.init)   = dev_init;
    BSP_SYS_API(dev.deinit)  = dev_deinit_stub;
    BSP_SYS_API(dev.conf)    = dev_conf_stub;
    BSP_SYS_API(dev.info)    = dev_info_stub;
    BSP_SYS_API(dev.priv)    = dev_priv_stub;
    BSP_SYS_API(fatal)      = fatal_error;
    BSP_SYS_API(prof_enter) = _profiler_enter;
    BSP_SYS_API(prof_exit)  = _profiler_exit;
    BSP_SYS_API(prof_reset) = profiler_reset;
    BSP_SYS_API(prof_init)  = profiler_init;
    BSP_SYS_API(prof_deinit)  = profiler_deinit;

    BSP_DBG_API(dev.init)   = serial_init;
    BSP_DBG_API(dev.deinit)  = dev_deinit_stub;
    BSP_DBG_API(dev.conf)    = dev_conf_stub;
    BSP_DBG_API(dev.info)    = dev_info_stub;
    BSP_DBG_API(dev.priv)    = dev_priv_stub;
    BSP_DBG_API(putc)       = serial_putc;
    BSP_DBG_API(getc)       = serial_getc;
    BSP_DBG_API(send)       = serial_send_buf;
    BSP_DBG_API(flush)      = serial_flush;
    BSP_DBG_API(reg_clbk)   = debug_add_rx_handler;
    BSP_DBG_API(unreg_clbk) = debug_rm_rx_handler;
    BSP_DBG_API(tickle)     = serial_tickle;
    BSP_DBG_API(dprintf)    = dprintf;

    BSP_IN_API(dev.init)    = input_bsp_init;
    BSP_IN_API(dev.deinit)  = input_bsp_deinit;
    BSP_IN_API(dev.conf)    = dev_conf_stub;
    BSP_IN_API(dev.info)    = dev_info_stub;
    BSP_IN_API(dev.priv)    = dev_priv_stub;
    BSP_IN_API(soft_init)   = input_soft_init;
    BSP_IN_API(bind_extra)  = input_bind_extra;
    BSP_IN_API(tickle)      = input_tickle;
    BSP_IN_API(proc_keys)   = input_proc_keys;
    BSP_IN_API(post_key)    = NULL;
    BSP_IN_API(touch_present) = input_is_touch_avail;
}

#else /*BSP_INDIR_API*/

bspapi_t *bsp_api_attach (void)

{
    arch_word_t *ptr, size;
    bsp_api_int_t *api;

    arch_get_shared(&ptr, &size);
    assert(ptr);
    assert(size >= sizeof(*api));

    api = (bsp_api_int_t *)ptr;
    assert(api->size == sizeof(*api));
    return &api->api;
}

#endif /*BSP_INDIR_API*/

void dev_tickle (void)
{
    audio_update();
    input_tickle();
    serial_tickle();
    profiler_reset();
}

void dvprintf (const char *fmt, va_list argptr)
{
    char            string[1024];

    vsnprintf (string, sizeof(string), fmt, argptr);

    dprintf(string);
}



