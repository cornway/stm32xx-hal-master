#include <bsp_api.h>
#include <debug.h>
#include <audio_main.h>
#include <input_main.h>
#include <misc_utils.h>

bspapi_t bspapi;
bspapi_t *g_bspapi;

#if !BSP_INDIR_API

void bsp_api_attach (bspapi_t *api)
{
    arch_word_t *ptr, size;

    arch_get_shared(&ptr, &size);
    assert(ptr);
    assert(size >= sizeof(*api));

    memset(api, 0, sizeof(*api));

    g_bspapi = api;

    api->io.io_init = dev_io_init;
    api->io.io_deinit = dev_io_deinit;
    api->io.open = d_open;
    api->io.size = d_size;
    api->io.tell = d_tell;
    api->io.close = d_close;
    api->io.unlink = d_unlink;
    api->io.seek = d_seek;
    api->io.eof = d_eof;
    api->io.read = d_read;
    api->io.gets = d_gets;
    api->io.getc = d_getc;
    api->io.write = d_write;
    api->io.printf = d_printf;
    api->io.mkdir = d_mkdir;
    api->io.opendir = d_opendir;
    api->io.closedir = d_closedir;
    api->io.readdir = d_readdir;
    api->io.time = d_time;
    api->io.dirlist = d_dirlist;
    api->io.var_reg = d_dvar_reg;
    api->io.var_int32 = d_dvar_int32;
    api->io.var_float = d_dvar_float;
    api->io.var_str = d_dvar_str;

    api->vid.init = screen_init;
    api->vid.deinit = screen_deinit;
    api->vid.get_wh = screen_get_wh;
    api->vid.mem_avail = screen_total_mem_avail_kb;
    api->vid.win_cfg = screen_win_cfg;
    api->vid.set_clut = screen_set_clut;
    api->vid.update = screen_update;
    api->vid.direct = screen_direct;
    api->vid.vsync = screen_vsync;
    api->vid.input_align = screen_ts_align;

    api->snd.init = audio_init;
    api->snd.deinit = audio_deinit;
    api->snd.mixer_ext = audio_mixer_ext;
    api->snd.play = audio_play_channel;
    api->snd.stop = audio_stop_channel;
    api->snd.pause = audio_pause;
    api->snd.stop_all = audio_stop_all;
    api->snd.is_play = audio_is_playing;
    api->snd.check = audio_chk_priority;
    api->snd.set_pan = audio_set_pan;
    api->snd.sample_volume = audio_change_sample_volume;
    api->snd.tickle = audio_update;
    api->snd.irq_save = audio_irq_save;
    api->snd.irq_restore = audio_irq_restore;

    api->snd.wave_open = audio_open_wave;
    api->snd.wave_size = audio_wave_size;
    api->snd.wave_cache = audio_cache_wave;
    api->snd.wave_close = audio_wave_close;

    api->cd.play = cd_play_name;
    api->cd.pause = cd_pause;
    api->cd.resume = cd_resume;
    api->cd.stop = cd_stop;
    api->cd.volume = cd_volume;
    api->cd.getvol = cd_getvol;
    api->cd.playing = cd_playing;

    api->sys.fatal = fatal_error;
    api->sys.alloc_init = Sys_AllocInit;
    api->sys.alloc_shared = Sys_AllocShared;
    api->sys.alloc_vid = Sys_AllocVideo;
    api->sys.avail = Sys_AllocBytesLeft;
    api->sys.malloc = Sys_Malloc;
    api->sys.realloc = Sys_Realloc;
    api->sys.calloc = Sys_Calloc;
    api->sys.free = Sys_Free;
    api->sys.prof_enter = _profiler_enter;
    api->sys.prof_exit = _profiler_exit;
    api->sys.prof_reset = profiler_reset;
    api->sys.prof_init = profiler_init;

    api->dbg.init = serial_init;
    api->dbg.putc = serial_putc;
    api->dbg.getc = serial_getc;
    api->dbg.send = serial_send_buf;
    api->dbg.flush = serial_flush;
    api->dbg.reg_clbk = term_register_handler;
    api->dbg.unreg_clbk = term_unregister_handler;
    api->dbg.tickle = serial_tickle;
    api->dbg->dprintf = dprintf;

    api->dbg.bsp_init = input_bsp_init;
    api->dbg.bsp_deinit = input_bsp_deinit;
    api->dbg.soft_init = input_soft_init;
    api->dbg.bind_extra input_bind_extra;
    api->dbg.tickle = input_tickle;
    api->dbg.proc_keys = input_proc_keys;
    api->dbg.post_key = NULL;
    api->dbg.touch_present = input_is_touch_present;

    d_memcpy(ptr, api, sizeof(*api));
}

#else

void bsp_api_attach (bspapi_t *api)
{
    arch_word_t *ptr, size;

    arch_get_shared(&ptr, &size);
    assert(ptr);
    assert(size >= sizeof(*api));

    d_memcpy(api, ptr, sizeof(*api));
    g_bspapi = api;
}

#endif /*BSP_INDIR_API*/


void bug (void)
{
    for (;;) {}
}

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



