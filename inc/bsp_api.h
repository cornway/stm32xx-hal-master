#ifndef __BSP_API_H__
#define __BSP_API_H__

#include <arch.h>

#include <nvic.h>

#ifdef BOOT
#define BSP_INDIR_API 0

#else

#ifdef APPLICATION
#define BSP_INDIR_API 1
#else
#define BSP_INDIR_API 0
#endif

#endif

typedef struct bspapi_s {

    struct io_s {
        int (*io_init) (void);
        void (*io_deinit) (void);
        int (*open) (const char *, int *, char const * );
        int (*size) (int);
        int (*tell) (int);
        void (*close) (int);
        int (*unlink) (const char *);
        int (*seek) (int, int, uint32_t);
        int (*eof) (int);
        int (*read) (int, PACKED void *, int);
        char *(*gets) (int, PACKED char *, int);
        char (*getc) (int);
        int (*write) (int, PACKED const void *, int);
        int (*printf) (int, char *, ...);
        int (*mkdir) (const char *);
        int (*opendir) (const char *);
        int (*closedir) (int );
        int (*readdir) (int , void *);
        uint32_t (*time) (void);
        int (*dirlist) (const char *, void *);

        int (*var_reg) (void *, const char *);
        int (*var_int32) (int32_t *, const char *);
        int (*var_float) (float *, const char *);
        int (*var_str) (char *, int, const char *);

        int (*var_rm) (const char *);
    } io;

    struct vid_S {
        void (*init) (void);
        void (*deinit) (void);
        void (*get_wh) (void *);
        uint32_t (*mem_avail) (void);
        int (*win_cfg) (void *, void *, uint32_t, int);
        void (*set_clut) (void *, uint32_t);
        void (*update) (void *);
        void (*direct) (void *);
        void (*vsync) (void);
        void (*input_align) (int *, int *);
    } vid;

    struct snd_s {
        void (*init) (void);
        void (*deinit) (void);
        void (*mixer_ext) (void (*) (int, void *, int, void *));
        void *(*play) (void *, int);
        void *(*stop) (int);
        void (*pause) (int);
        void (*stop_all) (void);
        int (*is_play) (int);
        int (*check) (int);
        void (*set_pan) (int, int, int);
        void (*sample_volume) (void *, uint8_t);
        void (*tickle) (void);
        void (*irq_save) (irqmask_t *);
        void (*irq_restore) (irqmask_t);

        int (*wave_open) (const char *, int);
        int (*wave_size) (int);
        int (*wave_cache) (int, uint8_t *, int);
        void (*wave_close) (int);
    } snd;

    struct sndcd_s {
        void *(*play) (void *, const char *);
        int (*pause) (void *);
        int (*resume) (void *);
        int (*stop) (void *);
        int (*volume) (void *, uint8_t);
        uint8_t (*getvol) (void *);
        int (*playing) (void);
    } cd;

    struct sys_t {
        void (*fatal) (char *, ...);
        void (*alloc_init) (void);
        void *(*alloc_shared) (int *);
        void *(*alloc_vid) (int *);
        int (*avail) (void);
        void *(*malloc) (int);
        void *(*realloc) (void *, int32_t);
        void *(*calloc) (int32_t);
        void (*free) (void *);

        void (*prof_enter) (const char *, int);
        void (*prof_exit) (const char *, int);
        void (*prof_reset) (void);
        void (*prof_init) (void);
    } sys;

    struct dbg_s {
        void (*init) (void);
        void (*putc) (char c);
        char (*getc) (void);
        void (*send) (const void *, unsigned int);
        void (*flush) (void);
        void (*reg_clbk) (void *);
        void (*unreg_clbk) (void *);
        void (*tickle) (void);
        void (*dprintf) (const char *, ...);
    } dbg;

    struct in_s {
        void (*bsp_init) (void);
        void (*bsp_deinit) (void);
        void (*soft_init) (void *(*) (void *, void *), void *);
        void (*bind_extra) (int, int);
        void (*tickle) (void);
        void (*proc_keys) (void *);
        void *(*post_key) (void *, void *event);
        int (*touch_present) (void);
    } in;

} bspapi_t;

extern bspapi_t *g_bspapi;

#endif /*__BSP_API_H__*/
