
#include "main.h"
#include "ff.h"


#define MAX_HANDLES		3

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

int d_open (char *path, int *hndl, char const * att)
{
    int ret = -1;
    BYTE mode = 0;

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
        case 'w':
            mode |= FA_WRITE | FA_OPEN_EXISTING;
            att = NULL;
        break;
        case '+':
            att++;
            mode |= FA_CREATE_ALWAYS;
        break;
        default:
            att = NULL;
        break;
    }
    if (f_open(gethandle(*hndl), path, mode)) {
        releasehandle(*hndl);
        return -1;
    }
    return f_size(gethandle(*hndl));
}

void d_close (int h)
{
    if (h < 0) {
        return;
    }
    releasehandle(h);
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
}

int d_mkdir (char *path)
{
    static DIR dp;
    if (f_opendir(&dp, path)) {
        return -1;
    }
    return 0;
}

uint32_t d_time (void)
{
    return HAL_GetTick();
}

