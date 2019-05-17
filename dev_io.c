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
#include <heap.h>

#ifndef DEVIO_READONLY
#warning "DEVIO_READONLY is undefined, using TRUE"
#define DEVIO_READONLY 1
#endif

#ifndef MAX_HANDLES
#warning "MAX_HANDLES is undefined, using 3"
#define MAX_HANDLES    3
#endif

#define FLAG_EXPORT (1 << 0)

FATFS SDFatFs ALIGN(8);  /* File system object for SD card logical drive */
char SDPath[4] = {0}; /* SD card logical drive path */


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
#define DEVIO_CTRL_CMD "devio"

const char *devio_deprecated_names[] =
{
    DEVIO_PRINTENV_CMD,
    DEVIO_REGISTER_CMD,
    DEVIO_UNREGISTER_CMD,
    DEVIO_CTRL_CMD,
};

static int devio_con_clbk (const char *text, int len);
static int _d_dvar_reg (dvar_t *var, const char *name);

static int devio_print_env (void *p1, void *p2);
static int devio_dvar_register (void *p1, void *p2);
static int devio_dvar_unregister (void *p1, void *p2);
static int devio_ctrl (void *p1, void *p2);

static int devio_dvar_deprecated (const char *name);

static fhandle_t __file_handles[MAX_HANDLES] ALIGN(8);
static dirhandle_t __dir_handles[MAX_HANDLES] ALIGN(8);

static fobjhdl_t *file_handles[MAX_HANDLES] ALIGN(8);
static fobjhdl_t *dir_handles[MAX_HANDLES] ALIGN(8);


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

const char *_fres_to_string (FRESULT res)
{
#define caseres(res) case res: str = #res ; break
const char *str;
    switch (res) {
        caseres(FR_DISK_ERR);
        caseres(FR_INT_ERR);
        caseres(FR_NOT_READY);
        caseres(FR_NO_FILE);
        caseres(FR_NO_PATH);
        caseres(FR_INVALID_NAME);
        caseres(FR_DENIED);
        caseres(FR_EXIST);
        caseres(FR_INVALID_OBJECT);
        caseres(FR_WRITE_PROTECTED);
        caseres(FR_INVALID_DRIVE);
        caseres(FR_NOT_ENABLED);
        caseres(FR_NO_FILESYSTEM);
        caseres(FR_MKFS_ABORTED);
        caseres(FR_TIMEOUT);
        caseres(FR_LOCKED);
        caseres(FR_NOT_ENOUGH_CORE);
        caseres(FR_TOO_MANY_OPEN_FILES);
        caseres(FR_INVALID_PARAMETER);
    }
    return str;
#undef caseres
}

static int _devio_mount (void *p1, void *p2);

int dev_io_init (void)
{
    FRESULT res;
    dvar_t dvar;
    int i;

    serial_rx_callback(devio_con_clbk);

    dvar.ptr = devio_print_env;
    dvar.ptrsize = sizeof(&devio_print_env);
    dvar.type = DVAR_FUNC;

    _d_dvar_reg(&dvar, DEVIO_PRINTENV_CMD);
    dvar.ptr = devio_dvar_register;
    _d_dvar_reg(&dvar, DEVIO_REGISTER_CMD);
    dvar.ptr = devio_dvar_unregister;
    _d_dvar_reg(&dvar, DEVIO_UNREGISTER_CMD);
    dvar.ptr = devio_ctrl;
    _d_dvar_reg(&dvar, DEVIO_CTRL_CMD);

    _devio_mount(SDPath, NULL);
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
            res = f_unlink(path);
            if (res != FR_OK) {
                dbg_eval(DBG_ERR) dprintf("%s() : fail : \'%s\'\n", __func__, _fres_to_string(res));
            }
        }
    }
    res = f_open(getfile(*hndl), path, mode);
    if (res != FR_OK) {
        dbg_eval(DBG_WARN) dprintf("%s() : fail : \'%s\'\n", __func__, _fres_to_string(res));
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
    FRESULT res;
    if (h < 0) {
        return;
    }
    res = f_close(getfile(h));
    if (res != FR_OK) {
        dbg_eval(DBG_ERR) dprintf("%s() : fail : \'%s\'\n", __func__, _fres_to_string(res));
    }
    freefile(h);
}

int d_unlink (const char *path)
{
    FRESULT res;
    res = f_unlink(path);
    if (res != FR_OK) {
        dbg_eval(DBG_ERR) dprintf("%s() : fail : \'%s\'\n", __func__, _fres_to_string(res));
        return -1;
    }
    return 0;
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
        dbg_eval(DBG_WARN) dprintf("%s() : fail : \'%s\'\n", __func__, _fres_to_string(res));
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

char d_getc (int h)
{
    char c;
    UINT btr;
    FRESULT res;
    res = f_read(getfile(h), &c, 1, &btr);
    if (res != FR_OK) {
        dbg_eval(DBG_WARN) dprintf("%s() : fail : \'%s\'\n", __func__, _fres_to_string(res));
    }
    return c;
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
        dbg_eval(DBG_ERR) dprintf("%s() : fail : \'%s\'\n", __func__, _fres_to_string(res));
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
    FRESULT res;
    alloc_dir(&h);
    if (h < 0) {
        return -1;
    }
    res = f_opendir(getdir(h), path);
    if (res != FR_OK) {
        dprintf("%s() : fail : \'%s\'\n", __func__, _fres_to_string(res));
        freedir(h);
        return -1;
    }
    return h;
}

int d_closedir (int dir)
{
    FRESULT res;
    res = f_closedir(getdir(dir));
    if (res != FR_OK) {
        dprintf("%s() : fail : \'%s\'\n", __func__, _fres_to_string(res));
    }
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

int d_dvar_int32 (int32_t *var, const char *name)
{
    dvar_t v;
    v.ptr = var;
    v.ptrsize = sizeof(int32_t);
    v.type = DVAR_INT32;
    return d_dvar_reg(&v, name);
}

int d_dvar_float (float *var, const char *name)
{
    dvar_t v;
    v.ptr = var;
    v.ptrsize = sizeof(float);
    v.type = DVAR_FLOAT;
    return d_dvar_reg(&v, name);
}

int d_dvar_str (char *str, int len, const char *name)
{
    dvar_t v;
    v.ptr = str;
    v.ptrsize = len;
    v.type = DVAR_STR;
    return d_dvar_reg(&v, name);
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

static const char *_next_arg (const char *text, const char *fmt, void *buf)
{
    const char *p = _next_token(text);
    if (!p) return NULL;
    if (!sscanf(p, fmt, buf)) return NULL;
    return _next_space(p);
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
            if (!p || !sscanf(p, "%i", &i)) {
                /*nothing ?, print current*/
                dprintf("%s = %i\n", v->name, readLong(v->dvar.ptr));
                return 0;
            }
            writeLong(v->dvar.ptr, i);
            p = _next_space(p);
            if (!p) {
                return len;
            }
            ret = (int)(p - text);
        }
        case DVAR_FLOAT:
        {
            const char *p;
            float i;

            p = _next_token(text);
            if (!p || !sscanf(p, "%f", &i)) {
                i = (float)readLong(v->dvar.ptr);
                dprintf("%s = %10f\n", v->name, i);
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
                dprintf("%s = %s\n", v->name, (char *)(v->dvar.ptr));
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
    if (v->dvar.flags & FLAG_EXPORT) {
        /*Exported function completes all*/
        return len;
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

#define DEVIO_MAX_PATH 64

typedef struct {
    int (*h) (void *p1, void *p2);
    const char *name;
} devio_hdlr_t;

static int __devio_export (devio_hdlr_t *hdlrs, int len)
{
    int i;
    dvar_t dvar;

    dvar.ptrsize = sizeof(hdlrs[0].h);
    dvar.type = DVAR_FUNC;
    dvar.flags = FLAG_EXPORT;

    for (i = 0; i < len; i++) {
        dvar.ptr = hdlrs[i].h;
        dbg_eval(DBG_INFO) {
            dprintf("export : \'%s\'\n", hdlrs[i].name);
        }
        d_dvar_reg(&dvar, hdlrs[i].name);
    }
    return 0;
}

static int _devio_mount (void *p1, void *p2)
{
    int i;
    FRESULT res;

    memset(&SDFatFs, 0, sizeof(SDFatFs));
    FATFS_UnLinkDriver((char *)p1);

    if(FATFS_LinkDriver(&SD_Driver, (char *)p1)) {
        return -1;
    }

    dbg_eval(DBG_WARN) dprintf("mount fs : %s\n", (char *)p1);
    res = f_mount(&SDFatFs, (TCHAR const*)p1, 1);
    if(res != FR_OK) {
        dbg_eval(DBG_ERR) dprintf("%s() : fail : \'%s\'\n", __func__, _fres_to_string(res));
        return -1;
    }

    /*TODO : Add own size to each pool*/
    for (i = 0; i < MAX_HANDLES; i++) {
        file_handles[i] = (fobjhdl_t *)&__file_handles[i];
        __file_handles[i].is_owned = 0;
    }
    for (i = 0; i < MAX_HANDLES; i++) {
        dir_handles[i] = (fobjhdl_t *)&__dir_handles[i];
        __dir_handles[i].is_owned = 0;
    }

    return 0;
}

static int _devio_export (void *p1, void *p2);

static int _devio_test (void *p1, void *p2)
{
    const char *fname = "test.txt";

    char buf[128], name[DEVIO_MAX_PATH] = {0}, *namep = (char *)fname;
    const char *str = (const char *)p1;
    int f, errors = 0;

    _next_arg(str, "%16s", name);
    if (name[0]) {
        namep = name;
    }
    dprintf("file name : %s\n", name);

    d_open(namep, &f, "+w");
    if (f < 0) {
        goto failopen;
    }
    dprintf("file write :\n");
    dprintf("%s()\n", __func__);

    if (!d_printf(f, "%s()\n", __func__)) {
        errors++;
    }
    d_close(f);

    d_open(namep, &f, "r");
    if (f < 0) {
        goto failopen;
    }
    dprintf("file read :\n");
    while (!d_eof(f)) {
        if (!d_gets(f, buf, sizeof(buf))) {
            errors++;
        }
        dprintf("\'%s\'\n", buf);
    }
    d_close(f);
    d_unlink(namep);
    dprintf("errors : %d\n", errors);
    return 0;
failopen:
    dprintf("%s() : failed to open \'%s\'\n", __func__, namep);
    return 0;
}

static int _devio_mkdir (void *p1, void *p2)
{
    char *str = p1, *p;
    char path[DEVIO_MAX_PATH] = {0}, *ppath = ".";

    p = (char *)_next_arg(str, "%s", path);
    if (path[0]) {
        ppath = path;
    }
    if (d_mkdir(ppath) < 0) {
        dprintf("%s() : fail\n", __func__);
    }
    return 0;
}

static int _devio_reset (void *p1, void *p2)
{
extern void SystemSoftReset (void);
    dprintf("Resetting...\n");
    serial_flush();
    SystemSoftReset();
    exit(0);
}

static int __dir_list (char *pathbuf, int recursion, const char *path);

static int _devio_list (void *p1, void *p2)
{
    char path[DEVIO_MAX_PATH] = {0}, *ppath;
    char pathbuf[128];
    char *p;
    fobj_t fobj;
    int dh;

    ppath = ".";
    _next_arg((char *)p1, "%16s", path);
    if (path[0]) {
        ppath = path;
    }
    return __dir_list(pathbuf, 0, ppath);
}

const char *treedeep[] =
{
"--",
"----",
"------",
"--------"
};

static int __dir_list (char *pathbuf, int recursion, const char *path)
{
    int h;
    fobj_t obj;

    if (recursion > arrlen(treedeep)) {
        dprintf("Too deep recursion : %i\n", recursion);
    }
    h = d_opendir(path);
    if (h < 0) {
        dprintf("cannot open path : %s\n", path);
        return -1;
    }
    while(d_readdir(h, &obj) >= 0) {

        dprintf("%s|%s %s \n",
            treedeep[recursion], obj.name, obj.type == FTYPE_FILE ? "f" : "d");
        if (obj.type == FTYPE_DIR) {
            sprintf(pathbuf, "%s/%s", path, obj.name);
            __dir_list(pathbuf, recursion + 1, pathbuf);
        }
    }
    d_closedir(h);
    return 0;
}


devio_hdlr_t devio_hdlrs[] =
{
    {_devio_export, "export"}, /*Must be first*/
    {_devio_test, "test"},
    {_devio_reset, "reset"},
    {_devio_list, "list"},
    {_devio_mkdir, "mkdir"},
    {_devio_mount, "mount"},
};

static int _devio_export (void *p1, void *p2)
{
    __devio_export(devio_hdlrs + 1, arrlen(devio_hdlrs) - 1);
}

static int devio_ctrl (void *p1, void *p2)
{
    const char *str = p1, *p;
    int len = (int)p2, i, cnt;
    char tok[16] = {0};

    p = _next_arg(str, "%16s", tok);
    for (i = 0; i < arrlen(devio_hdlrs); i++) {
        if (strcmp(tok, devio_hdlrs[i].name) == 0) {
            cnt = devio_hdlrs[i].h((void *)p, NULL);
        }
    }
    return len;
bad:
    dprintf("%s() : unknown arg : \'%s\'\n", __func__, str);
    return len;
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
    const char *type_text[] = {"function", "integer", "float", "string"};
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

