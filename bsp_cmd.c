#include <string.h>
#include <ctype.h>
#include "boot/int/boot_int.h"
#include <bsp_cmd.h>
#include <debug.h>
#include <heap.h>
#include <misc_utils.h>
#include <dev_io.h>

#if defined(BSP_DRIVER)

#define CMD_MAX_NAME (16)
#define CMD_MAX_PATH (128)
#define CMD_MAX_BUF (256)

#define FLAG_EXPORT (1 << 0)

typedef struct dvar_int_s {
    struct dvar_int_s *next;
    cmdvar_t dvar;
    char name[CMD_MAX_NAME];
    uint16_t namelen;
} dvar_int_t;

typedef struct {
    const char *name;
    cmd_func_t func;
} cmd_func_map_t;

static dvar_int_t *dvar_head = NULL;

static int _cmd_register_var (cmdvar_t *var, const char *name);
static int _cmd_rx_handler (int argc, char **argv);
static int __cmd_handle_input (dvar_int_t *v, int argc, char **argv);
static int __cmd_print_env (int argc, char **argv);
static int __cmd_register_var (int argc, char **argv);
static int __cmd_unregister_var (int argc, char **argv);
static int __cmd_exec_priv (int argc, char **argv);
static int __cmd_is_readonly (const char *name);
static int __cmd_fs_print_dir (int argc, char **argv);
static int _cmd_export_all (int argc, char **argv);
static int __cmd_test_fs (int argc, char **argv);
static int cmd_brd_reset (int argc, char **argv);
static int __cmd_fs_mkdir (int argc, char **argv);
static int cmd_install_exec (int argc, char **argv);
static int __cmd_fs_mkdir (int argc, char **argv);
static int cmd_start_exec (int argc, char **argv);

static int cmd_var_exist (PACKED const char *name, dvar_int_t **_prev, dvar_int_t **_dvar)
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

static int _cmd_register_var (cmdvar_t *var, const char *name)
{
    dvar_int_t *v;

    if (cmd_var_exist(name, &v, &v)) {
        dprintf("var \'%s\' already exist\n", name);
        return -1;
    }

    v = (dvar_int_t *)heap_malloc(sizeof(dvar_int_t));
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

int cmd_register_var (cmdvar_t *var, const char *name)
{
    if (__cmd_is_readonly(name)) {
        dprintf("read-only : \'%s\'\n", name);
        return -1;
    }
    return _cmd_register_var(var, name);
}

int cmd_register_i32 (int32_t *var, const char *name)
{
    cmdvar_t v;
    v.ptr = var;
    v.ptrsize = sizeof(int32_t);
    v.type = DVAR_INT32;
    return cmd_register_var(&v, name);
}

int cmd_register_float (float *var, const char *name)
{
    cmdvar_t v;
    v.ptr = var;
    v.ptrsize = sizeof(float);
    v.type = DVAR_FLOAT;
    return cmd_register_var(&v, name);
}

int cmd_register_str (char *str, int len, const char *name)
{
    cmdvar_t v;
    v.ptr = str;
    v.ptrsize = len;
    v.type = DVAR_STR;
    return cmd_register_var(&v, name);
}

int cmd_register_func (cmd_func_t func, const char *name)
{
    cmdvar_t v;
    v.ptr = func;
    v.ptrsize = sizeof(func);
    v.type = DVAR_FUNC;
    return cmd_register_var(&v, name);
}

static int _cmd_register_func (cmd_func_t func, const char *name)
{
    cmdvar_t v;
    v.ptr = func;
    v.ptrsize = sizeof(func);
    v.type = DVAR_FUNC;
    return _cmd_register_var(&v, name);
}

int cmd_unregister (const char *name)
{
    dvar_int_t *v = NULL;
    dvar_int_t *prev = NULL;

    if (__cmd_is_readonly(name)) {
        dprintf("read-only : \'%s\'\n", name);
        return -1;
    }

    if (!cmd_var_exist(name, &prev, &v)) {
        dprintf("unknown : \'%s\'", name);
        return -1;
    }

    if (prev) {
        prev->next = v->next;
    } else {
        dvar_head = v->next;
    }
    return 0;
}

static void cmd_unregister_all (void)
{
    dvar_int_t *next = NULL;
    dvar_int_t *v = dvar_head;
    while (v) {
        next = v->next;
        heap_free(v);
        v = next;
    }
    dvar_head = NULL;
}

const cmd_func_map_t cmd_func_tbl[] =
{
    {"print",       __cmd_print_env},
    {"register",    __cmd_register_var},
    {"unreg",       __cmd_unregister_var},
    {"devio",       __cmd_exec_priv},
    {"list",        __cmd_fs_print_dir},
};

cmd_func_map_t cmd_priv_func_tbl[] =
{
    {"export",  _cmd_export_all}, /*Must be first*/
    {"test",    __cmd_test_fs},
    {"reset",   cmd_brd_reset},
    {"mkdir",   __cmd_fs_mkdir},
    {"load",    cmd_install_exec},
    {"boot",    cmd_start_exec},
};

int cmd_init (void)
{
    debug_add_rx_handler(_cmd_rx_handler);

    int i;

    for (i = 0; i < arrlen(cmd_func_tbl); i++) {
        _cmd_register_func(cmd_func_tbl[i].func, cmd_func_tbl[i].name);
    }
    return 0;
}

void cmd_deinit (void)
{
    debug_rm_rx_handler(_cmd_rx_handler);
    cmd_unregister_all();
}

void cmd_tickle (void)
{
    cmd_exec_queue(cmd_execute);
}

static int __cmd_is_readonly (const char *name)
{
    int i;

    for (i = 0; i < arrlen(cmd_func_tbl); i++) {
        if (strcmp(cmd_func_tbl[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int __cmd_export_all (cmd_func_map_t *hdlrs, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        dbg_eval(DBG_INFO) {
            dprintf("export : \'%s\'\n", hdlrs[i].name);
        }
        cmd_register_func(hdlrs[i].func, hdlrs[i].name);
    }
    return 0;
}

static int _cmd_unregister_var (void *p1, void *p2)
{
    cmd_unregister((char *)p1);

    return strlen((char *)p1);
}

static int __cmd_register_var (int argc, char **argv)
{
    dprintf("%s() : Not yet\n", __func__);
    return 0;
}

static int __cmd_unregister_var (int argc, char **argv)
{
    if (argc < 1) {
        return -1;
    }
    dprintf("not yet\n");

    return argc - 1;
}

static int _cmd_rx_handler (int argc, char **argv)
{
    dvar_int_t *v = dvar_head;
    int tmp;

    while (v && argc > 0) {

        if (strncmp(argv[0], v->name, v->namelen) == 0) {
            /*TODO : Add command/line terminator, e.g. - ';' or '&&'..*/
            tmp = __cmd_handle_input(v, argc, argv);
            argv = &argv[argc - tmp];
            argc = tmp;
            return argc;
        }
        v = v->next;
    }
    return 0;
}

static int __cmd_handle_input (dvar_int_t *v, int argc, char **argv)
{
    if (argc < 1) {
        return -1;
    }
    argc--;
    switch (v->dvar.type) {
        case DVAR_INT32:
        {
            int32_t val;

            if (!sscanf(argv[0], "%i", &val)) {
                dprintf("%s = %i\n", v->name, readLong(v->dvar.ptr));
                return argc;
            }
            writeLong(v->dvar.ptr, val);
            dprintf("%s = %i\n", v->name, val);
            return argc;
        }
        case DVAR_FLOAT:
        {
            float val;

            if (!sscanf(argv[0], "%f", &val)) {
                dprintf("%s = %f\n", v->name, readLong(v->dvar.ptr));
                return argc;
            }
            writeLong(v->dvar.ptr, val);
            dprintf("%s = %f\n", v->name, val);
            return argc;
        }
        break;
        case DVAR_STR:
        {
            snprintf(v->dvar.ptr, v->dvar.ptrsize, "%s", argv[0]);
            return argc;
        }
        break;
        case DVAR_FUNC:
        {
            cmd_func_t func = (cmd_func_t)v->dvar.ptr;
            return func(argc, &argv[0]);
        }
        break;
        default:
            assert(0);
        break;
    }
    if (v->dvar.flags & FLAG_EXPORT) {
        /*Exported function completes all*/
        return argc;
    }
    return argc;
}


static int __cmd_print_env (int argc, char **argv)
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

/*=======================================================================================*/

#define CMD_EXEC_MAX 6
cmdexec_t cmd_exec_pool[CMD_EXEC_MAX];
cmdexec_t *boot_cmd_top = &cmd_exec_pool[0];

void cmd_push (const char *cmd, const char *text, void *user1, void *user2)
{
    int i;
    char buf[CMD_MAX_BUF];

    if (boot_cmd_top == &cmd_exec_pool[arrlen(cmd_exec_pool)]) {
        return;
    }
    i = snprintf(buf, sizeof(buf), "%s %s", cmd, text);
    boot_cmd_top->text = heap_malloc(i + 1);
    assert(boot_cmd_top->text);
    strcpy(boot_cmd_top->text, buf);
    boot_cmd_top->user1 = user1;
    boot_cmd_top->user2 = user2;
    boot_cmd_top->len = i;
    boot_cmd_top++;
    dprintf("%s() : \'%s\' [%s]\n", __func__, cmd, text);
}

static cmdexec_t *__cmd_pop (char *text, cmdexec_t *cmd)
{
    if (boot_cmd_top == &cmd_exec_pool[0]) {
        return NULL;
    }
    boot_cmd_top--;
    strcpy(text, boot_cmd_top->text);
    heap_free(boot_cmd_top->text);
    boot_cmd_top->text = NULL;
    cmd->user1 = boot_cmd_top->user1;
    cmd->user2 = boot_cmd_top->user2;
    cmd->len = boot_cmd_top->len;
    return cmd;
}

void cmd_exec_queue (cmd_handler_t hdlr)
{
     cmdexec_t cmd;
     char buf[256];
     cmdexec_t *cmdptr = __cmd_pop(buf, &cmd);

     while (cmdptr) {
        hdlr(buf, cmdptr->len);
        cmdptr = __cmd_pop(buf, cmdptr);
     }
}

static int __cmd_test_fs (int argc, char **argv)
{
    const char *fname = "test.txt";
    char buf[128], *namep = (char *)fname;
    int f, errors = 0;

    if (argc > 1) {
        namep = argv[0];
    }
    dprintf("file name : %s\n", namep);

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

static int __cmd_fs_mkdir (int argc, char **argv)
{
    if (argc < 1) {
        return -1;
    }
    if (d_mkdir(argv[0]) < 0) {
        dprintf("%s() : fail\n", __func__);
    }
    return --argc;
}

static int cmd_brd_reset (int argc, char **argv)
{
extern void SystemSoftReset (void);
    dprintf("Resetting...\n");
    serial_flush();
    SystemSoftReset();
    assert(0);
    return -1;
}

static int __dir_list (char *pathbuf, int recursion, const char *path);

static int __cmd_fs_print_dir (int argc, char **argv)
{
    char *ppath;
    char pathbuf[CMD_MAX_PATH];

    if (argc > 1) {
        ppath = argv[0];
    } else {
        ppath = "";
    }
    return __dir_list(pathbuf, 0, ppath);
}

static int __dir_list (char *pathbuf, int recursion, const char *path)
{
    int h;
    fobj_t obj;

    h = d_opendir(path);
    if (h < 0) {
        dprintf("cannot open path : %s\n", path);
        return -1;
    }
    while(d_readdir(h, &obj) >= 0) {

        dprintf("%*s|%s %s \n",
            recursion * 2, "-", obj.name, obj.type == FTYPE_FILE ? "f" : "d");
        if (obj.type == FTYPE_DIR) {
            sprintf(pathbuf, "%s/%s", path, obj.name);
            __dir_list(pathbuf, recursion + 1, pathbuf);
        }
    }
    d_closedir(h);
    return 0;
}

static int _cmd_export_all (int argc, char **argv)
{
    return __cmd_export_all(cmd_priv_func_tbl + 1, arrlen(cmd_priv_func_tbl) - 1);
}

static int __cmd_exec_priv (int argc, char **argv)
{
    int  i;

    for (i = 0; i < arrlen(cmd_priv_func_tbl) && argc > 0; i++) {
        if (strcmp(argv[0], cmd_priv_func_tbl[i].name) == 0) {
            cmd_priv_func_tbl[i].func(--argc, &argv[1]);
            break;
        }
    }
    return argc;
}

void cmd_execute (const char *cmd, int len)
{
    char buf[256];
    len = snprintf(buf, sizeof(buf), "%s", cmd);
    term_proc_text(buf, len);
}

int cmd_install_exec (int argc, char **argv)
{
    arch_word_t progaddr;

    if (argc < 2) {
        dprintf("usage : /path/to/file <boot address 0x0xxx..>");
        return -1;
    }
    if (!sscanf(argv[1], "%x", &progaddr)) {
        return -1;
    }
    bsp_install_exec((arch_word_t *)progaddr, argv[0], 1, argv[1]);
    

    return argc - 2;
}

int cmd_start_exec (int argc, char **argv)
{
    arch_word_t progaddr;

    if (argc < 2) {
        dprintf("usage : /path/to/file <boot address 0x0xxx..>");
        return -1;
    }
    if (!sscanf(argv[1], "%x", &progaddr)) {
        return -1;
    }

    bsp_start_exec((arch_word_t *)progaddr, argv[0], 1, argv[1]);

    return argc - 2;
}

#endif /*BSP_DRIVER*/
