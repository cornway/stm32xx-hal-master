#include <string.h>
#include <ctype.h>
#include "boot/int/boot_int.h"
#include "int/bsp_mod_int.h"
#include <bsp_cmd.h>
#include <debug.h>
#include <heap.h>
#include <misc_utils.h>
#include <dev_io.h>

#if defined(BSP_DRIVER)

#define CMD_MAX_NAME (16)
#define CMD_MAX_PATH (128)
#define CMD_MAX_BUF (256)
#define CMD_MAX_ARG (16)

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

typedef struct cmd_keyval_s {
    const char *key;
    void *val;
    const char fmt[3];
    uint8_t valid;
    void (*handle) (struct cmd_keyval_s *, const char *);
} cmd_keyval_t;

/**_S - means 'short', for '-x' cases*/
#define CMD_KVI32_S(c, v) \
    {c, v, "%i", 0, NULL}

#define CMD_KVSTR_S(c, v) \
    {c, v, "%s", 0, NULL}

static dvar_int_t *dvar_head = NULL;

static int _cmd_register_var (cmdvar_t *var, const char *name);
static int _cmd_rx_handler (int argc, const char **argv);
static int __cmd_handle_input (dvar_int_t *v, int argc, const char **argv);
static int __cmd_print_env (int argc, const char **argv);
static int __cmd_register_var (int argc, const char **argv);
static int __cmd_unregister_var (int argc, const char **argv);
static int __cmd_exec_priv (int argc, const char **argv);
static int __cmd_is_readonly (const char *name);
static int __cmd_fs_print_dir (int argc, const char **argv);
static int _cmd_export_all (int argc, const char **argv);
static int __cmd_fs_touch (int argc, const char **argv);
static int cmd_brd_reset (int argc, const char **argv);
static int __cmd_fs_mkdir (int argc, const char **argv);
static int cmd_install_exec (int argc, const char **argv);
static int __cmd_fs_mkdir (int argc, const char **argv);
static int cmd_start_exec (int argc, const char **argv);
static int cmd_mod_insert (int argc, const char **argv);
static int cmd_mod_rm (int argc, const char **argv);
static int cmd_mod_probe (int argc, const char **argv);
int cmd_stdin_handle (int argc, const char **argv);
static int cmd_stdin_handle (int argc, const char **argv);
static int cmd_serial_config (int argc, const char **argv);
static int __cmd_set_stdin_char (int num);
int cmd_cat (int argc, const char **argv);
static int cmd_kvpair_process_keys (cmd_keyval_t **begin, cmd_keyval_t **end, int argc,
                                        const char **argv_dst, const char **argv);

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

static cmd_keyval_t *
__cmd_collect_parm_short (cmd_keyval_t **begin, cmd_keyval_t **end,
                            const char *str)
{
    cmd_keyval_t *kv;
    const char *pstr = str;

    while (begin <= end) {
        if (begin[0]->key[0] == str[0]) {
            kv = begin[0];
            if (end[0]) {
                begin[0] = end[0];
                end[0] = NULL;
            }
            return kv;
        }
        begin++;
    }
    return NULL;
}

void cmd_keyval_parse_val (cmd_keyval_t *kv, const char *str)
{
    if (!sscanf(str, kv->fmt, kv->val)) {
        dprintf("%s() : fail\n", __func__);
    }
}

static int __cmd_kvpair_check (const char *str, int *iscombo)
{
    int dashes = 0;

    *iscombo = 0;
    while (*str == '-') {
        dashes++; str++;
    }
    if (str[0] && str[1]) {
        /*More than one key present : -xyz..*/
        *iscombo = 1;
    }
    return dashes;
}

static const char **
__cmd_kv_extend_combo (const char **argv_dst, const char *cmb)
{
    while (*cmb) {
        if (isalpha(*cmb)) {
            argv_dst[0] = cmb;
            argv_dst++;
        }
        cmb++;
    }
    return argv_dst;
}

static int __cmd_kvpair_collect (int argc, const char **keys,
                                    const char **params, const char **src,
                                    uint8_t *flags) /*!! N(flags) == N(src)*/
{
    int dashes, iscombo;
    int paramcnt = 0;

    while (argc > 0) {

        dashes = __cmd_kvpair_check(src[0], &iscombo);

        if (dashes > 2) {

            dprintf("%s() : fail \'%s\'\n", __func__, src[0]);
        } else if (dashes == 2) {

            assert(!iscombo);
            /*TODO : support for "--abcd." sequences ?*/
            dprintf("%s() : not yet\n", __func__);
        } else if (dashes) {

            const char *key = src[0] += dashes;

            if (iscombo) {
                keys = __cmd_kv_extend_combo(keys, key);
            } else {
                keys[0] = key;
            }
            flags[0] = 'k'; /*mark key*/
            /*collect parameter : -x 'val'*/
            if (argc > 1) {
                if (__cmd_kvpair_check(src[1], &iscombo) == 0) {
                    flags[1] = 'v'; /*mark next as value*/
                    keys[1] = src[1];
                    keys++; flags++;
                    src++; argc--;
                } else {
                    /*Next key follows, nothing todo*/
                }
            }
            keys++; flags++;
        } else {
            params[0] = src[0];
            params++;
            paramcnt++;
        }
        src++; argc--;
    }
    keys[0] = NULL;
    return paramcnt;
}
static int
__cmd_kvpair_proc_keys
                (cmd_keyval_t **begin, cmd_keyval_t **end, 
                const char **params, const char **keys, /*Null terminated*/
                uint8_t *flags)
{
    int paramcnt = 0;
    cmd_keyval_t *kv = NULL;

    for (; begin[0] && keys[0]; ) {

        kv = __cmd_collect_parm_short(begin, end, keys[0]);
        if (!kv) {
            assert(*flags == 'k');
            dprintf("unknown param : \'%s\'", keys[0]);
            return -1;
        }
        assert(!end[0]);
        kv->valid = 1;
        end--; keys++; flags++;

        if (*flags == 'v') {
            if (!keys[0]) {
                dprintf("%s() : oops!\n", __func__);
                break;
            }
            if (!kv->val) {
                /*Does not require parameter*/
                params[0] = keys[0]; /*Push back to parameters*/
                params++;
                paramcnt++;
            } else if (kv->handle) {
                kv->handle(kv, keys[0]);
            } else {
                cmd_keyval_parse_val(kv, keys[0]);
            }
            flags++; keys++;
        }
    }
    return paramcnt;
}

static int
cmd_kvpair_process_keys (cmd_keyval_t **begin, cmd_keyval_t **end, int argc,
                                        const char **params, const char **src)
{
    const char *keys[CMD_MAX_ARG];
    uint8_t flags[CMD_MAX_ARG];
    int moreparams = 0;

    if (argc < 1) {
        /*at least one arg required*/
        return argc;
    }

    argc = __cmd_kvpair_collect(argc, keys, params, src, flags);

    moreparams = __cmd_kvpair_proc_keys(begin, end, params, keys, flags);
    if (moreparams < 0) {
        return -1;
    }

    return argc + moreparams;
}


const cmd_func_map_t cmd_func_tbl[] =
{
    {"print",       __cmd_print_env},
    {"register",    __cmd_register_var},
    {"unreg",       __cmd_unregister_var},
    {"bsp",         __cmd_exec_priv},
    {"list",        __cmd_fs_print_dir},
    {"cat",         cmd_cat},
};

cmd_func_map_t cmd_priv_func_tbl[] =
{
    {"export",  _cmd_export_all}, /*Must be first*/
    {"testfs",  __cmd_fs_touch},
    {"reset",   cmd_brd_reset},
    {"mkdir",   __cmd_fs_mkdir},
    {"load",    cmd_install_exec},
    {"boot",    cmd_start_exec},
    {"insmod",  cmd_mod_insert},
    {"rmmod",   cmd_mod_rm},
    {"modprobe",cmd_mod_probe},
    {"stdin",   cmd_stdin_handle},
    {"serial",  cmd_serial_config},
    
};

typedef enum {
    STDIN_CHAR,
    STDIN_PATH,
    STDIN_NULL,
} BSP_STDIN;

static int stdin_eof_recv = 0;
int g_stdin_eof_timeout_var = 0;
int g_stdin_eof_timeout = 20000;
static int stdin_to_path_bytes_limit = -1;
static int stdin_to_path_bytes_cnt = 0;
BSP_STDIN bsp_stdin_type = STDIN_CHAR;


int cmd_init (void)
{
    debug_add_rx_handler(_cmd_rx_handler);

    int i;

    for (i = 0; i < arrlen(cmd_func_tbl); i++) {
        _cmd_register_func(cmd_func_tbl[i].func, cmd_func_tbl[i].name);
    }
    cmd_register_i32(&g_stdin_eof_timeout, "stdinwait");
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
    if (g_stdin_eof_timeout_var && g_stdin_eof_timeout_var < d_time()) {
        dprintf("wait for stdin : timeout\n");
        dprintf("flushing..\n");
        __cmd_set_stdin_char(0);
    }
}

int cmd_cat (int argc, const char **argv)
{
    int f, fseek = 0;
    int a = 0, b = 0, c = -1, h = 0;
    char str[256], *pstr;
    const char *argvbuf[CMD_MAX_ARG];

    cmd_keyval_t kvarr[] = 
    {
        CMD_KVI32_S("n", &c),
        CMD_KVI32_S("a", &a),
        CMD_KVI32_S("b", &b),
        CMD_KVI32_S("h", &h),
    };
    cmd_keyval_t *kvlist[arrlen(kvarr)];

    for (a = 0; a < arrlen(kvarr); a++) kvlist[a] = &kvarr[a];
    a = 0;

    argc = cmd_kvpair_process_keys(&kvlist[0], &kvlist[arrlen(kvlist) - 1],
                        argc, argvbuf, argv);

    if (argc < 1) {
        dprintf("%s() : unexpected arguments\n", __func__);
        return -1;
    }
    d_open(argv[0], &f, "r");
    if (f < 0) {
        dprintf("path does not exist : %s\n", argv[0]);
        return -1;
    }
    if (c && a && b) {
        fseek = c - a;
    } else if (!a) {
        b = 0;
    } else {
        fseek = a;
    }
    if (fseek) {
        if (fseek < 0) {
            fseek = d_size(f) + fseek;
            assert(fseek > 0);
        }
        d_seek(f, fseek, DSEEK_SET);
    }
    pstr = str;
    if (h > 0 && h < 4) {
        h = 4;
    }
    dprintf("******************************\n");
    while (!d_eof(f) && c--) {
        if (h) {
            int len = sizeof(str) / 2;

            len = d_read(f, pstr, ROUND_DOWN(len, h));
            if (!len) {
                break;
            }
            hexdump(str, len, h);
        } else {
            pstr = d_gets(f, pstr, sizeof(str));
            if (!pstr) {
                break;
            }
            dprintf("%s\n", str);
        }
    }
    d_close(f);
    dprintf("\n");
    return 0;
}

static int cmd_serial_config (int argc, const char **argv)
{
    return argc;
}

static cmd_func_t saved_stdin_hdlr = NULL;
static int stdin_file = -1;

char *cmd_get_eof (const char *data, int len)
{
    char *p;

    p = strstr(data, "\n\n\n\n\n\n\n\n");
    return p;
}

static int cmd_stdin_path_close (void);

int cmd_stdin_to_path_write (int argc, const char **argv)
{
    int len = argc;
    void *data = (void *)*argv;
    char *eof;

    if (stdin_eof_recv) {
        return len;
    }

    g_stdin_eof_timeout_var = d_time() + g_stdin_eof_timeout;
    eof = cmd_get_eof(data, len);
    if (eof) {
        len = eof - (char *)data;
    }
    len = min(len, stdin_to_path_bytes_limit);
    len = d_write(stdin_file, data, len);
    stdin_to_path_bytes_cnt += len;
    stdin_to_path_bytes_limit -= len;
    assert(stdin_to_path_bytes_limit >= 0);

    if (!stdin_to_path_bytes_limit) {
        dprintf("done, received : %u bytes\n", stdin_to_path_bytes_cnt);
        __cmd_set_stdin_char(0);
    }
    if (eof) {
        stdin_eof_recv = 1;
        g_stdin_eof_timeout_var = d_time() + g_stdin_eof_timeout;
    }
    return 0;
}

static int __cmd_set_stdin_char (int num)
{
    if (bsp_stdin_type == STDIN_CHAR) {
        return -1;
    }
    switch (num) {
        default :
            dprintf("stdin > %i\n", num);
            cmd_stdin_path_close();
            bsp_stdin_pop(saved_stdin_hdlr);
            g_serial_rx_eof = '\n';
            bsp_stdin_type = STDIN_CHAR;
        break;
    }
    return 0;
}

static int __cmd_set_stdin_file (const char *path, const char *attr)
{
    int f;
    if (bsp_stdin_type == STDIN_PATH) {
        return -1;
    }
    d_open(path, &f, attr);
    if (f >= 0) {
        dprintf("stdin > \'%s\'\n", path);
        g_serial_rx_eof = 0;
        saved_stdin_hdlr = bsp_stdin_push(cmd_stdin_to_path_write);
        g_stdin_eof_timeout_var = d_time() + g_stdin_eof_timeout;
        stdin_to_path_bytes_cnt = 0;
        bsp_stdin_type = STDIN_PATH;
    } else {
        dprintf("unable to create : \'%s\'\n", path);
    }
    return f;
}

static int cmd_stdin_path_close (void)
{
    dprintf("closing path\n");
    d_close(stdin_file);
    stdin_file = -1;
    stdin_eof_recv = 0;
    g_stdin_eof_timeout_var = 0;
    return 0;
}

static int __cmd_stdin_redirect (int argc, const char **argv)
{
    int num = -1;
    char attr[3] = "+w";
    int bytes_limit;
    char payload[256];
    const char *argvbuf[CMD_MAX_ARG];

    cmd_keyval_t kvpload =
        CMD_KVSTR_S("p", &payload[0]);
    cmd_keyval_t kvarr[] = 
    {
        CMD_KVI32_S("n", &bytes_limit),
        CMD_KVSTR_S("a", &attr[0]),
    };
    cmd_keyval_t *kvlist[] =
        {&kvarr[0], &kvarr[1], &kvpload};

    /*argv[0] must be a path*/
    argc = cmd_kvpair_process_keys(&kvlist[0], &kvlist[arrlen(kvlist) - 1],
                        argc - 1, argvbuf, &argv[1]);

    if (argc < 0) {
        dprintf("%s() : unexpected arguments\n", __func__);
        return -1;
    }

    stdin_to_path_bytes_limit = bytes_limit;

    dprintf("dst=[%s] att=[%s] bytes=<%u>\n",
            argv[0], attr, stdin_to_path_bytes_limit);
    if (sscanf(argv[0], "%i", &num)) {
        __cmd_set_stdin_char(num);
    } else {
        stdin_file = __cmd_set_stdin_file(argv[0], attr);
        if (stdin_file < 0) {
            return 0;
        }
        if (kvpload.valid) {
            /*Local payload, that will be written to the file*/
            if (stdin_to_path_bytes_limit > 0) {
                stdin_to_path_bytes_limit = min(stdin_to_path_bytes_limit, strlen(payload));
            }
            cmd_push("", payload, NULL, NULL);
        }
    }
    return argc;
}

static int cmd_stdin_handle (int argc, const char **argv)
{
    dprintf("%s() :\n", __func__);

    if (argc < 2) {
        return -1;
    }

    switch (argv[0][0]) {
        case '>':
            argc = __cmd_stdin_redirect(argc--, &argv[1]);
        break;
        default :
            dprintf("%s() : unknown\n", __func__);
            argc = 0;
        break;
    }
    return argc;
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

static int __cmd_register_var (int argc, const char **argv)
{
    dprintf("%s() : Not yet\n", __func__);
    return 0;
}

static int __cmd_unregister_var (int argc, const char **argv)
{
    if (argc < 1) {
        return -1;
    }
    dprintf("not yet\n");

    return argc - 1;
}

static int _cmd_rx_handler (int argc, const char **argv)
{
    dvar_int_t *v = dvar_head;

    if (argc < 1) {
        return -1;
    }

    while (v) {
        if (strncmp(argv[0], v->name, v->namelen) == 0) {
            argc = __cmd_handle_input(v, --argc, ++argv);
            return argc;
        }
        v = v->next;
    }
    return argc;
}

static int __cmd_handle_input (dvar_int_t *v, int argc, const char **argv)
{
    switch (v->dvar.type) {
        case DVAR_INT32:
        {
            int32_t val;

            if (argc <= 0 || !sscanf(argv[0], "%i", &val)) {
                dprintf("%s = %lu\n", v->name, readLong(v->dvar.ptr));
                argc = 0;
                break;
            }
            writeLong(v->dvar.ptr, val);
            dprintf("%s = %i\n", v->name, val);
            argc = 0;
        }
        break;
        case DVAR_FLOAT:
        {
            float val;

            if (argc <= 0 || !sscanf(argv[0], "%f", &val)) {
                dprintf("%s = %f\n", v->name, (float)readLong(v->dvar.ptr));
                argc = 0;
                break;
            }
            writeLong(v->dvar.ptr, val);
            dprintf("%s = %f\n", v->name, val);
            argc = 0;
        }
        break;
        case DVAR_STR:
        {
            if (argc <= 0) {
                dprintf("%s = %s\n", v->name, (char *)v->dvar.ptr);
                return argc;
            }
            snprintf(v->dvar.ptr, v->dvar.ptrsize, "%s", argv[0]);
            argc = 0;
        }
        break;
        case DVAR_FUNC:
        {
            cmd_func_t func = (cmd_func_t)v->dvar.ptr;
            argc = func(argc, &argv[0]);
        }
        break;
        default:
            assert(0);
        break;
    }
    if (v->dvar.flags & FLAG_EXPORT) {
        /*Exported function completes all*/
        return 0;
    }
    return argc;
}


static int __cmd_print_env (int argc, const char **argv)
{
    dvar_int_t *v = dvar_head;

    dprintf("print env :\n");
    while (v) {
        dprintf("\'%3.16s\' \'%3.16s\'\n",
                v->name, v->dvar.type == DVAR_FUNC ? "func" : "var");
        v = v->next;
    }
    dprintf("\n");

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

static int __cmd_fs_touch (int argc, const char **argv)
{
    const char *attr = "r";
    const char *path;
    int f;
    int fcreate = 0;

    if (argc < 1) {
        dprintf("usage: touch <path> +w/w/r/+r");
        return 0;
    }
    if (argc > 1) {
        attr = argv[1];
        argc--;
    }
    if (attr[0] == '+') {
        fcreate = 1;
    }
    argc--;
    path = argv[0];
    d_open(path, &f, attr);
    if (f < 0) {
        if (!fcreate) {
            dprintf("touch: \'%s\' existing, use [+w] to overwrite :\n", path);
            dprintf("[ touch <path> +w ]\n");
        }
        return 0;
    }
    d_close(f);
    return argc;
}

static int __cmd_fs_mkdir (int argc, const char **argv)
{
    if (argc < 1) {
        return -1;
    }
    if (d_mkdir(argv[0]) < 0) {
        dprintf("%s() : fail\n", __func__);
    }
    return --argc;
}

static int cmd_brd_reset (int argc, const char **argv)
{
extern void SystemSoftReset (void);
    dprintf("board reset...\n");
    serial_flush();
    SystemSoftReset();
    assert(0);
    return -1;
}

#define MAX_RECURSION 16

static int __dir_list (char *pathbuf, int recursion,
                        int maxrecursion, const char *path);


static int __cmd_fs_print_dir (int argc, const char **argv)
{
    const char *ppath;
    char pathbuf[CMD_MAX_PATH];
    const char *argvbuf[16];
    uint32_t recursion = MAX_RECURSION;

    cmd_keyval_t kva = CMD_KVI32_S("n", &recursion);
    cmd_keyval_t *kvlist[] ={&kva};

    assert(arrlen(argvbuf) > argc);

    argc = cmd_kvpair_process_keys(&kvlist[0], &kvlist[arrlen(kvlist) - 1],
                                argc, argvbuf, argv);

    if (argc < 0) {
        return -1;
    }

    if (argc >= 1) {
        ppath = argvbuf[0];
    } else {
        ppath = "";
    }
    return __dir_list(pathbuf, 0, recursion, ppath);
}

static int __dir_list (char *pathbuf, int recursion,
                        int maxrecursion, const char *path)
{
    int h;
    fobj_t obj;

    if (recursion >= maxrecursion) {
        return 0;
    }

    h = d_opendir(path);
    if (h < 0) {
        dprintf("cannot open path : %s\n", path);
        return 0;
    }
    while(d_readdir(h, &obj) >= 0) {

        dprintf("%*s|%s %s \n",
            recursion * 2, "-", obj.name, obj.type == FTYPE_FILE ? "f" : "d");
        if (obj.type == FTYPE_DIR) {
            sprintf(pathbuf, "%s/%s", path, obj.name);
            if (__dir_list(pathbuf, recursion + 1, maxrecursion, pathbuf) < 0) {
                break;
            }
        }
    }
    d_closedir(h);
    return 0;
}

static int _cmd_export_all (int argc, const char **argv)
{
    return __cmd_export_all(cmd_priv_func_tbl + 1, arrlen(cmd_priv_func_tbl) - 1);
}

static int __cmd_exec_priv (int argc, const char **argv)
{
    int  i;

    for (i = 0; i < arrlen(cmd_priv_func_tbl) && argc > 0; i++) {
        if (strcmp(argv[0], cmd_priv_func_tbl[i].name) == 0) {
            argc = cmd_priv_func_tbl[i].func(--argc, &argv[1]);
            break;
        }
    }
    return argc;
}

int cmd_execute (const char *cmd, int len)
{
    char buf[256];
    len = snprintf(buf, sizeof(buf), "%s", cmd);
    bsp_in_handle_cmd(buf, len);
    /*TODO : cmd errno ?*/
    return 0;
}

int cmd_install_exec (int argc, const char **argv)
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

int cmd_start_exec (int argc, const char **argv)
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

static int cmd_mod_insert (int argc, const char **argv)
{
    arch_word_t progaddr;
    const bsp_heap_api_t heap =
    {
        heap_alloc_shared,
        heap_free,
    };

    if (argc < 2) {
        dprintf("usage : /path/to/file <load address 0x0xxx..>");
        return -1;
    }
    if (!sscanf(argv[1], "%x", &progaddr)) {
        return -1;
    }

    bspmod_insert(&heap, argv[0], argv[1]);

    return argc - 2;
}

static int cmd_mod_probe (int argc, const char **argv)
{
    if (argc < 1) {
        dprintf("usage : /path/to/file <load address 0x0xxx..>");
        return -1;
    }

    bspmod_probe(argv[0]);

    return argc - 1;
}

static int cmd_mod_rm (int argc, const char **argv)
{
    if (argc < 1) {
        dprintf("usage : <mod name>");
        return -1;
    }

    bspmod_remove(argv[0]);

    return argc - 1;
}




#endif /*BSP_DRIVER*/
