#include <stdint.h>

#ifndef __DEVIO_H__
#define __DEVIO_H__

#include <arch.h>
#include <bsp_api.h>

typedef enum {
    FTYPE_FILE,
    FTYPE_DIR,
} ftype_t;

typedef enum {
    DVAR_FUNC,
    DVAR_INT32,
    DVAR_FLOAT,
    DVAR_STR,
} dvar_obj_t;

typedef int (*dvar_func_t) (void *, void *);

typedef struct {
    void *ptr;
    uint16_t ptrsize;
    uint16_t size;
    dvar_obj_t type;
    uint32_t flags;
} dvar_t;

#define DSEEK_SET 0
#define DSEEK_CUR 1
#define DSEEK_END 2

typedef struct {
    ftype_t type;
    int h;
    char name[128];
    void *ptr;
} fobj_t;

typedef int (*list_clbk_t)(char *name, ftype_t type);

typedef struct {
    list_clbk_t clbk;
    void *user;
} flist_t;

#if BSP_INDIR_API

#define dev_io_init    g_bspapi->io.io_init
#define dev_io_deinit    g_bspapi->io.io_deinit
#define d_open    g_bspapi->io.open
#define d_size    g_bspapi->io.size
#define d_tell    g_bspapi->io.tell
#define d_close    g_bspapi->io.close
#define d_unlink    g_bspapi->io.unlink
#define d_seek    g_bspapi->io.seek
#define d_eof    g_bspapi->io.eof
#define d_read    g_bspapi->io.read
#define d_gets    g_bspapi->io.gets
#define d_getc    g_bspapi->io.getc
#define d_write    g_bspapi->io.write
#define d_printf    g_bspapi->io.printf
#define d_mkdir    g_bspapi->io.mkdir
#define d_opendir    g_bspapi->io.opendir
#define d_closedir    g_bspapi->io.closedir
#define d_readdir    g_bspapi->io.readdir
#define d_time    g_bspapi->io.time
#define d_dirlist    g_bspapi->io.dirlist
#define d_dvar_reg    g_bspapi->io.var_reg
#define d_dvar_int32    g_bspapi->io.var_int32
#define d_dvar_float    g_bspapi->io.var_float
#define d_dvar_str    g_bspapi->io.var_str

#else

int dev_io_init (void);
void dev_io_deinit (void);
int d_open (const char *path, int *hndl, char const * att);
int d_size (int hndl);
int d_tell (int h);
void d_close (int h);
int d_unlink (const char *path);
int d_seek (int handle, int position, uint32_t mode);
int d_eof (int handle);
int d_read (int handle, PACKED void *dst, int count);
char *d_gets (int handle, PACKED char *dst, int count);
char d_getc (int h);
int d_write (int handle, PACKED const void *src, int count);
int d_printf (int handle, char *fmt, ...);
int d_mkdir (const char *path);
int d_opendir (const char *path);
int d_closedir (int dir);
int d_readdir (int dir, fobj_t *fobj);
uint32_t d_time (void);
int d_dirlist (const char *path, flist_t *flist);

int d_dvar_reg (dvar_t *var, const char *name);
int d_dvar_int32 (int32_t *var, const char *name);
int d_dvar_float (float *var, const char *name);
int d_dvar_str (char *str, int len, const char *name);

int d_dvar_rm (const char *name);

#endif

#endif
