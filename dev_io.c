#include "main.h"
#include "ff.h"
#include "ff_gen_drv.h"
#include "sd_diskio.h"
#include "dev_io.h"
#include "dev_conf.h"

#ifndef DEVIO_READONLY
#warning "DEVIO_READONLY is undefined, using TRUE"
#define DEVIO_READONLY 1
#endif

#ifndef MAX_HANDLES
#warning "MAX_HANDLES is undefined, using 3"
#define MAX_HANDLES    3
#endif

FATFS SDFatFs;  /* File system object for SD card logical drive */
char SDPath[4]; /* SD card logical drive path */

typedef struct {
    FIL file;
    int is_owned;
} fhandle_t;

fhandle_t sys_handles[MAX_HANDLES];

static inline FIL *allochandle (int *num)
{
    int i;

    for (i=0 ; i<MAX_HANDLES ; i++) {
        if (sys_handles[i].is_owned == 0) {
            sys_handles[i].is_owned = 1;
            *num = i;
            return &sys_handles[i].file;
        }
    }
    *num = -1;
    return NULL;
}

static inline FIL *gethandle (int num)
{
    return &sys_handles[num].file;
}

static inline void releasehandle (int handle)
{
    sys_handles[handle].is_owned = 0;
}

int dev_io_init (void)
{
    if(FATFS_LinkDriver(&SD_Driver, SDPath)) {
        return -1;
    }

    if(f_mount(&SDFatFs, (TCHAR const*)SDPath, 0) != FR_OK) {
        return -1;
    }
    return 0;
}

int d_open (char *path, int *hndl, char const * att)
{
    int ret = -1;
    BYTE mode = 0;
    FRESULT res;

    if (!att) {
        return ret;
    }

    allochandle(hndl);
    if (*hndl < 0) {
        return ret;
    }
    while (att)
    switch (*att) {
        case 'r':
            mode |= FA_READ | FA_OPEN_EXISTING;
            att = NULL;
        break;
#if !DEVIO_READONLY
        case 'w':
            mode |= FA_WRITE | FA_OPEN_EXISTING;
            att = NULL;
        break;
#endif
        case '+':
            att++;
            mode |= FA_CREATE_ALWAYS;
        break;
        default:
            att = NULL;
        break;
    }
    if (mode & FA_CREATE_ALWAYS) {
        res = f_open(gethandle(*hndl), path, FA_READ);
        if (res == FR_OK) {
            f_close(gethandle(*hndl));
            f_unlink(path);
        }
    }
    res = f_open(gethandle(*hndl), path, mode);
    if (res != FR_OK) {
        releasehandle(*hndl);
        *hndl = -1;
        return -1;
    }
    return f_size(gethandle(*hndl));
}

int d_size (int hndl)
{
    return f_size(gethandle(hndl));
}

void d_close (int h)
{
    if (h < 0) {
        return;
    }
    f_close(gethandle(h));
    releasehandle(h);
}

void d_unlink (char *path)
{

}

void d_seek (int handle, int position)
{
    if (handle < 0) {
        return;
    }
    f_lseek(gethandle(handle), position);
}

int d_eof (int handle)
{
    if (handle < 0) {
        return -1;
    }
    return f_eof(gethandle(handle));
}

int d_read (int handle, void *dst, int count)
{
    char *data;
    UINT done = 0;
    FRESULT res = FR_NOT_READY;

    if ( handle >= 0 ) {
        data = dst;
        res = f_read(gethandle(handle), data, count, &done);
    }
    if (res != FR_OK) {
        return -1;
    }
    return done;
}

char *d_gets (int handle, char *dst, int count)
{
    if (f_gets(dst, count, gethandle(handle)) == NULL) {
        return NULL;
    }
    return dst;
}

int d_write (int handle, void *src, int count)
{
#if !DEVIO_READONLY
    char *data;
    UINT done;
    FRESULT res = FR_NOT_READY;

    if ( handle >= 0 ) {
        data = src;
        res = f_write (gethandle(handle), data, count, &done);
    }
    if (res != FR_OK) {
        return -1;
    }
    return done;
#else
    return count;
#endif
}

int d_mkdir (char *path)
{
#if !DEVIO_READONLY
    FRESULT res = f_mkdir(path);
    if ((res != FR_OK) && (res != FR_EXIST)) {
        return -1;
    }
#endif
    return 0;
}

uint32_t d_time (void)
{
    return HAL_GetTick();
}

int d_dirlist (char *path, flist_t *flist)
{
    FRESULT res;
    DIR dir;
    FILINFO fno;
    ftype_t ftype;

    res = f_opendir(&dir, path);
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno); 
            if (res != FR_OK || fno.fname[0] == 0) {
                break;
            }

            if ((fno.fattrib & AM_DIR) == 0) {
                ftype = FTYPE_FILE;
            } else {
                ftype = FTYPE_DIR;
            }
            if (flist->clbk(fno.fname, ftype)) {
                break;
            }
        }
        f_closedir(&dir);
    } else {
        return -1;
    }
    return 0;
}

