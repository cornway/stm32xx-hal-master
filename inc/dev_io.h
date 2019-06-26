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

typedef struct bsp_io_api_s {
    bspdev_t dev;
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
} bsp_io_api_t;

#define BSP_IO_API(func) ((bsp_io_api_t *)(g_bspapi->io))->func

#if BSP_INDIR_API

#define dev_io_init     BSP_IO_API(dev.io_init)
#define dev_io_deinit   BSP_IO_API(dev.io_deinit)
#define dev_io_conf     BSP_IO_API(dev.conf)
#define dev_io_info     BSP_IO_API(dev.info)
#define dev_io_priv     BSP_IO_API(dev.priv)

#define d_open          BSP_IO_API(open)
#define d_size          BSP_IO_API(size)
#define d_tell          BSP_IO_API(tell)
#define d_close         BSP_IO_API(close)
#define d_unlink        BSP_IO_API(unlink)
#define d_seek          BSP_IO_API(seek)
#define d_eof           BSP_IO_API(eof)
#define d_read          BSP_IO_API(read)
#define d_gets          BSP_IO_API(gets)
#define d_getc          BSP_IO_API(getc)
#define d_write         BSP_IO_API(write)
#define d_printf        BSP_IO_API(printf)
#define d_mkdir         BSP_IO_API(mkdir)
#define d_opendir       BSP_IO_API(opendir)
#define d_closedir      BSP_IO_API(closedir)
#define d_readdir       BSP_IO_API(readdir)
#define d_time          BSP_IO_API(time)
#define d_dirlist       BSP_IO_API(dirlist)
#define d_dvar_reg      BSP_IO_API(var_reg)
#define d_dvar_int32    BSP_IO_API(var_int32)
#define d_dvar_float    BSP_IO_API(var_float)
#define d_dvar_str      BSP_IO_API(var_str)

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

void term_parse (const char *buf, int size);

#endif

#endif
