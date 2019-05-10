#include "main.h"
#include "ff.h"
#include "ff_gen_drv.h"
#include "sd_diskio.h"
#include "dev_io.h"
#include "dev_conf.h"
#include "stdarg.h"
#include <misc_utils.h>
#include <string.h>
#include <ctype.h>
#include <debug.h>

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
    int type;
    int is_owned;
    char ptr[];
} fobjhdl_t;

typedef struct {
    int type;
    int is_owned;
    FIL file;
} fhandle_t;

typedef struct {
    int type;
    int is_owned;
    DIR dir;
    FILINFO fn;
} dirhandle_t;

#define DEVIO_PRINTENV_CMD "print"
#define DEVIO_REGISTER_CMD "register"
#define DEVIO_UNREGISTER_CMD "unreg"

const char *devio_deprecated_names[] =
{
    DEVIO_PRINTENV_CMD,
    DEVIO_REGISTER_CMD,
    DEVIO_UNREGISTER_CMD,
};

static int devio_con_clbk (const char *text, int len);
static int _d_dvar_reg (dvar_t *var, const char *name);

static int devio_print_env (void *p1, void *p2);
static int devio_dvar_register (void *p1, void *p2);
static int devio_dvar_unregister (void *p1, void *p2);

static int devio_dvar_deprecated (const char *name);

static fhandle_t __file_handles[MAX_HANDLES];
static dirhandle_t __dir_handles[MAX_HANDLES];

static fobjhdl_t *file_handles[MAX_HANDLES];
static fobjhdl_t *dir_handles[MAX_HANDLES];


#define alloc_file(nump) allochandle(file_handles, nump)
#define alloc_dir(nump) allochandle(dir_handles, nump)

#define getfile(num) gethandle(file_handles, num)
#define getdir(num) gethandle(dir_handles, num)

#define freefile(num) releasehandle(file_handles, num)
#define freedir(num) releasehandle(dir_handles, num)

static inline void *allochandle (fobjhdl_t **hdls, int *num)
{
    int i;

    for (i=0 ; i<MAX_HANDLES ; i++) {
        if (hdls[i]->is_owned == 0) {
            hdls[i]->is_owned = 1;
            *num = i;
            return hdls[i]->ptr;
        }
    }
    *num = -1;
    return NULL;
}

static inline void *gethandle (fobjhdl_t **hdls, int num)
{
    return hdls[num]->ptr;
}

static inline void releasehandle (fobjhdl_t **hdls, int handle)
{
    hdls[handle]->is_owned = 0;
}

int dev_io_init (void)
{
    dvar_t dvar;
    int i;
    if(FATFS_LinkDriver(&SD_Driver, SDPath)) {
        return -1;
    }

    if(f_mount(&SDFatFs, (TCHAR const*)SDPath, 0) != FR_OK) {
        return -1;
    }

    for (i = 0; i < MAX_HANDLES; i++) {
        file_handles[i] = (fobjhdl_t *)&__file_handles[i];
    }
    for (i = 0; i < MAX_HANDLES; i++) {
        dir_handles[i] = (fobjhdl_t *)&__dir_handles[i];
    }
    serial_rx_callback(devio_con_clbk);

    dvar.ptr = devio_print_env;
    dvar.ptrsize = sizeof(&devio_print_env);
    dvar.type = DVAR_FUNC;

    _d_dvar_reg(&dvar, DEVIO_PRINTENV_CMD);
    dvar.ptr = devio_dvar_register;
    _d_dvar_reg(&dvar, DEVIO_REGISTER_CMD);
    dvar.ptr = devio_dvar_unregister;
    _d_dvar_reg(&dvar, DEVIO_UNREGISTER_CMD);
    return 0;
}

int d_open (const char *path, int *hndl, char const * att)
{
    int ret = -1;
    BYTE mode = 0;
    FRESULT res;

    if (!att) {
        return ret;
    }

    alloc_file(hndl);
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
        res = f_open(getfile(*hndl), path, FA_READ);
        if (res == FR_OK) {
            f_close(getfile(*hndl));
            f_unlink(path);
        }
    }
    res = f_open(getfile(*hndl), path, mode);
    if (res != FR_OK) {
        freefile(*hndl);
        *hndl = -1;
        return -1;
    }
    return f_size((FIL *)getfile(*hndl));
}

int d_size (int hndl)
{
    return f_size((FIL *)getfile(hndl));
}

void d_close (int h)
{
    if (h < 0) {
        return;
    }
    f_close(getfile(h));
    freefile(h);
}

int d_unlink (const char *path)
{
    FRESULT res;
    res = f_unlink(path);
    return res == FR_OK ? 0 : -1;
}

/*TODO : handle mode arg*/
int d_seek (int handle, int position, uint32_t mode)
{
    if (handle < 0) {
        return -1;
    }
    assert(mode == DSEEK_SET);
    return f_lseek(getfile(handle), position) == FR_OK ? position : -1;
}

int d_eof (int handle)
{
    if (handle < 0) {
        return -1;
    }
    return f_eof((FIL *)getfile(handle));
}

int d_read (int handle, PACKED void *dst, int count)
{
    PACKED char *data;
    UINT done = 0;
    FRESULT res = FR_NOT_READY;

    if ( handle >= 0 ) {
        data = dst;
        res = f_read(getfile(handle), data, count, &done);
    }
    if (res != FR_OK) {
        return -1;
    }
    return done;
}

char *d_gets (int handle, PACKED char *dst, int count)
{
    if (f_gets(dst, count, getfile(handle)) == NULL) {
        return NULL;
    }
    return dst;
}

int d_write (int handle, PACKED const void *src, int count)
{
#if !DEVIO_READONLY
    PACKED const char *data;
    UINT done;
    FRESULT res = FR_NOT_READY;

    if ( handle >= 0 ) {
        data = src;
        res = f_write (getfile(handle), data, count, &done);
    }
    if (res != FR_OK) {
        return -1;
    }
    return done;
#else
    return count;
#endif
}

int d_printf (int handle, char *fmt, ...)
{
    va_list args;
    char buf[1024];
    int size;

    va_start(args, fmt);
    size = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    return d_write(handle, buf, size);
}


int d_mkdir (const char *path)
{
#if !DEVIO_READONLY
    FRESULT res = f_mkdir(path);
    if ((res != FR_OK) && (res != FR_EXIST)) {
        return -1;
    }
#endif
    return 0;
}

int d_opendir (const char *path)
{
    int h;
    alloc_dir(&h);
    if (h < 0) {
        return -1;
    }
    if (f_opendir(getdir(h), path) != FR_OK) {
        return -1;
    }
    return h;
}

int d_closedir (int dir)
{
    f_closedir(getdir(dir));
    freedir(dir);
    return 0;
}

int d_readdir (int dir, fobj_t *fobj)
{
    
    dirhandle_t *dh = (dirhandle_t *)getdir(dir);
    f_readdir(getdir(dir) , &dh->fn);
    if (dh->fn.fname[0] == 0) {
        return -1;
    }
    snprintf(fobj->name, sizeof(fobj->name), "%s", dh->fn.fname);
    if ((dh->fn.fattrib & AM_DIR) == 0) {
        fobj->type = FTYPE_FILE;
    } else {
        fobj->type = FTYPE_DIR;
    }
    return dir;
}

uint32_t d_time (void)
{
    return HAL_GetTick();
}

int d_dirlist (const char *path, flist_t *flist)
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


#define DEV_MAX_NAME (1 << 4)

typedef struct dvar_int_s {
    struct dvar_int_s *next;
    dvar_t dvar;
    char name[DEV_MAX_NAME];
    uint16_t namelen;
} dvar_int_t;

dvar_int_t *dvar_head = NULL;

static int devio_dvar_exist (PACKED const char *name, dvar_int_t **_prev, dvar_int_t **_dvar)
{
    dvar_int_t *v = dvar_head;
    dvar_int_t *prev = NULL;

    while (v) {

        if (strcmp(v->name, name) == 0) {
            *_prev = prev;
            *_dvar = v;
            return 1;
        }

        prev = v;
        v = v->next;
    }
    return 0;
}

static int _d_dvar_reg (dvar_t *var, const char *name)
{
    dvar_int_t *v;

    if (devio_dvar_exist(name, &v, &v)) {
        dprintf("Variable with name \'%s\' already exist !\n", name);
        return -1;
    }

    v = (dvar_int_t *)Sys_Malloc(sizeof(dvar_int_t));
    if (!v) {
        return -1;
    }

    v->namelen = snprintf(v->name, sizeof(v->name), "%s", name);
    memcpy(&v->dvar, var, sizeof(v->dvar));

    if (dvar_head == NULL) {
        dvar_head = v;
        v->next = NULL;
    } else {
        v->next = dvar_head;
        dvar_head = v;
    }
    return 0;
}

int d_dvar_reg (dvar_t *var, const char *name)
{
    if (devio_dvar_deprecated(name)) {
        dprintf("Deprecated name \'%s\'\n", name);
        return -1;
    }
    return _d_dvar_reg(var, name);
}


int d_dvar_rm (const char *name)
{
    dvar_int_t *v = NULL;
    dvar_int_t *prev = NULL;

    if (devio_dvar_deprecated(name)) {
        dprintf("Cannot be removed : \'%s\'\n", name);
        return -1;
    }

    if (!devio_dvar_exist(name, &prev, &v)) {
        dprintf("Variable with name \'%s\' missing!", name);
        return -1;
    }

    if (prev) {
        prev->next = v->next;
    } else {
        dvar_head = v->next;
    }
    return 0;
}

static const char *_next_token (const char *buf)
{
    while (*buf) {
        if (!isspace(*buf)) {
            return buf;
        }
        buf++;
    }
    return NULL;
}

static const char *_next_space (const char *buf)
{
    while (*buf) {
        if (isspace(*buf)) {
            return buf;
        }
        buf++;
    }
    return NULL;
}

/*Deffered routine, to register/unregister dvar's, etc..*/

static dvar_int_t devio_defered_dvar;
static dvar_int_t *devio_defered_ptr = NULL;

static int _devio_con_handle (dvar_int_t *v, const char *text, int len)
{
    int ret = 0;
    switch (v->dvar.type) {
        case DVAR_INT32:
        {
            const char *p;
            int32_t i;

            p = _next_token(text);
            if (!p) {
                return 0;
            }
            if (!sscanf(p, "%i", &i)) {
                return 0;
            }
            writeLong(v->dvar.ptr, i);
            p = _next_space(p);
            if (!p) {
                return len;
            }
            ret = (int)(p - text);
        }
        break;
        case DVAR_STR:
        {
            const char *p;

            p = _next_token(text);
            if (!p) {
                return 0;
            }
            p += snprintf(v->dvar.ptr, v->dvar.ptrsize, "%s", p);
            ret = (int)(p - text);
        }
        break;
        case DVAR_FUNC:
        {
            dvar_func_t func = (dvar_func_t)v->dvar.ptr;
            ret = func((void *)text, &len);
        }
        break;
    }

    return ret;
}

static int devio_con_clbk (const char *_text, int len)
{
    dvar_int_t *v = dvar_head;
    const char *text;
    int hret;

    text = _next_token(_text);
    hret = (int)(text - _text);

    while (v) {

        if (strncmp(text, v->name, v->namelen) == 0) {
            /*TODO : Add command/line terminator, e.g. - ';' or '&&'..*/
            hret += _devio_con_handle(v, text + v->namelen, len - v->namelen);
            if (devio_defered_ptr) {
                hret += len - (hret + v->namelen);
                break;
            }
            return hret + v->namelen;
        }
        v = v->next;
    }
    if (devio_defered_ptr) {
        dvar_func_t func;

        v = devio_defered_ptr;
        func = (dvar_func_t)v->dvar.ptr;

        func(v->name, &v->namelen);

        devio_defered_ptr = NULL;
        if (hret != 0) {
            dprintf("Warning! trying to execute \'%s\'\n", text + len);
            dprintf("after : \'%s\' was executed\n", v->name);
        }
        return len;
    }
    return 0;
}

static int devio_dvar_register (void *p1, void *p2)
{
    char *name = (char *)p1;

    dprintf("%s() : Not implemented yet, \'%s\' skipped\n", __func__, name);
    return strlen(name);
}

static int _devio_dvar_unregister (void *p1, void *p2)
{
    d_dvar_rm((char *)p1);

    return strlen((char *)p1);
}

static int devio_dvar_unregister (void *p1, void *p2)
{
    devio_defered_ptr = &devio_defered_dvar;
    /*TODO : remove explicit size : %16*/
    sscanf((const char *)p1, "%16s", devio_defered_ptr->name);
    devio_defered_ptr->namelen = strlen(devio_defered_ptr->name);
    devio_defered_ptr->dvar.ptr = _devio_dvar_unregister;
    devio_defered_ptr->dvar.ptrsize = sizeof(&_devio_dvar_unregister);
    devio_defered_ptr->dvar.type = DVAR_FUNC;

    return devio_defered_ptr->namelen;
}

static int devio_dvar_deprecated (const char *name)
{
    int i;

    for (i = 0; i < arrlen(devio_deprecated_names); i++) {
        if (strcmp(devio_deprecated_names[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int devio_print_env (void *p1, void *p2)
{
    dvar_int_t *v = dvar_head;
    const char *border = "========================================\n";
    const char *type_text[] = {"function", "integer", "string"};
    int i = 0;

    dprintf("%s", border);

    while (v) {
        dprintf("%3i : \'%s\'  \'%s\'  <0x%08x>  <0x%04x>\n",
                i, v->name, type_text[v->dvar.type], (unsigned)v->dvar.ptr, v->dvar.ptrsize);
        v = v->next;
    }

    dprintf("%s", border);

    return 0;
}

