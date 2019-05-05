#include <stdint.h>

#ifndef __DEVIO_H__
#define __DEVIO_H__

#include <arch.h>

typedef enum {
    FTYPE_FILE,
    FTYPE_DIR,
} ftype_t;

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

int dev_io_init (void);
int d_open (char *path, int *hndl, char const * att);
int d_size (int hndl);
void d_close (int h);
void d_unlink (char *path);
int d_seek (int handle, int position, uint32_t mode);
int d_eof (int handle);
int d_read (int handle, PACKED void *dst, int count);
char *d_gets (int handle, PACKED char *dst, int count);
int d_write (int handle, PACKED const void *src, int count);
int d_printf (int handle, char *fmt, ...);
int d_mkdir (char *path);
int d_opendir (char *path);
int d_closedir (int dir);
int d_readdir (int dir, fobj_t *fobj);
uint32_t d_time (void);
int d_dirlist (char *path, flist_t *flist);


#endif