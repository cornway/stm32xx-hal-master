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
static int __cmd_stdin_forward (int argc, const char **argv);
static int __cmd_stdin_handle (dvar_int_t *v, int argc, const char **argv);
static int __cmd_print_env (int argc, const char **argv);
static int __cmd_register_var (int argc, const char **argv);
static int __cmd_unregister_var (int argc, const char **argv);
static int __cmd_exec_internal (int argc, const char **argv);
static int __cmd_var_readonly (const char *name);
static int __cmd_fs_print_dir (int argc, const char **argv);
static int _cmd_export_all (int argc, const char **argv);
static int __cmd_fs_touch (int argc, const char **argv);
static int cmd_bsp_sys_reset (int argc, const char **argv);
static int __cmd_fs_mkdir (int argc, const char **argv);
static int cmd_install_executable (int argc, const char **argv);
static int __cmd_fs_mkdir (int argc, const char **argv);
static int cmd_start_executable (int argc, const char **argv);
static int cmd_mod_insert (int argc, const char **argv);
static int cmd_mod_rm (int argc, const char **argv);
static int cmd_mod_probe (int argc, const char **argv);
int cmd_stdin_ctrl (int argc, const char **argv);
static int cmd_stdin_ctrl (int argc, const char **argv);
static int cmd_util_serial (int argc, const char **argv);
int cmd_util_cat (int argc, const char **argv);
static int cmd_argv_to_parm (cmd_keyval_t **begin, cmd_keyval_t **end, int argc,
                                        const char **argv_dst, const char **argv);
static int cmd_stdin_to_path_write (int argc, const char **argv);


#define MAX_RECURSION 16

static int __dir_list (char *pathbuf, int recursion,
                        int maxrecursion, const char *path);

static cmd_func_t saved_stdin_hdlr = NULL;
static int stdin_file = -1;
static char __eof_sequence[16] = "...............";

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
    if (__cmd_var_readonly(name)) {
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

    if (__cmd_var_readonly(name)) {
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
__cmd_parm_short_match (cmd_keyval_t **begin, cmd_keyval_t **end,
                            const char *str)
{
    cmd_keyval_t *kv;

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

void cmd_parm_load_val (cmd_keyval_t *kv, const char *str)
{
    if (!sscanf(str, kv->fmt, kv->val)) {
        dprintf("%s() : fail\n", __func__);
    }
}
void cmd_parm_load_str (cmd_keyval_t *kv, const char *str)
{
    strcpy(kv->val, str);
}

static int __cmd_parm_check (const char *str, int *iscombo)
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
__cmd_parm_split_combo (const char **argv_dst, const char *cmb)
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

static int __cmd_parm_arrange (int argc, const char **keys,
                                    const char **params, const char **src,
                                    uint8_t *flags) /*!! N(flags) == N(src)*/
{
    int dashes, iscombo;
    int paramcnt = 0;

    while (argc > 0) {

        dashes = __cmd_parm_check(src[0], &iscombo);

        if (dashes > 2) {

            dprintf("%s() : fail \'%s\'\n", __func__, src[0]);
        } else if (dashes == 2) {

            assert(!iscombo);
            /*TODO : support for "--abcd." sequences ?*/
            dprintf("%s() : not yet\n", __func__);
        } else if (dashes) {

            const char *key = src[0] += dashes;

            if (iscombo) {
                keys = __cmd_parm_split_combo(keys, key);
            } else {
                keys[0] = key;
            }
            flags[0] = 'k'; /*mark key*/
            /*collect parameter : -x 'val'*/
            if (argc > 1) {
                if (__cmd_parm_check(src[1], &iscombo) == 0) {
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
__cmd_parm_proc_keys
                (cmd_keyval_t **begin, cmd_keyval_t **end, 
                const char **params, const char **keys, /*Null terminated*/
                uint8_t *flags)
{
    int paramcnt = 0;
    cmd_keyval_t *kv = NULL;

    for (; begin[0] && keys[0]; ) {

        kv = __cmd_parm_short_match(begin, end, keys[0]);
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
                cmd_parm_load_val(kv, keys[0]);
            }
            flags++; keys++;
        }
    }
    return paramcnt;
}

static int
cmd_argv_to_parm (cmd_keyval_t **begin, cmd_keyval_t **end, int argc,
                                        const char **params, const char **src)
{
    const char *keys[CMD_MAX_ARG];
    uint8_t flags[CMD_MAX_ARG] = {0};
    int moreparams = 0;

    if (argc < 1) {
        /*at least one arg required*/
        return argc;
    }

    argc = __cmd_parm_arrange(argc, keys, params, src, flags);

    moreparams = __cmd_parm_proc_keys(begin, end, params, keys, flags);
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
    {"bsp",         __cmd_exec_internal},
    {"list",        __cmd_fs_print_dir},
    {"cat",         cmd_util_cat},
};

cmd_func_map_t cmd_priv_func_tbl[] =
{
    {"export",  _cmd_export_all}, /*Must be first*/
    {"testfs",  __cmd_fs_touch},
    {"reset",   cmd_bsp_sys_reset},
    {"mkdir",   __cmd_fs_mkdir},
    {"load",    cmd_install_executable},
    {"boot",    cmd_start_executable},
    {"insmod",  cmd_mod_insert},
    {"rmmod",   cmd_mod_rm},
    {"modprobe",cmd_mod_probe},
    {"stdin",   cmd_stdin_ctrl},
    {"serial",  cmd_util_serial},
    
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

static char __cmd_tofile_buf[256] = {0};
static uint8_t __cmd_tofile_full = 0;
static uint8_t __cmd_tofile_tresh = 128;
static uint32_t __cmd_tofile_usage_tsf = 0;

static int cmd_stdin_path_close (void);

int cmd_init (void)
{
    bsp_stdin_register_if(__cmd_stdin_forward);

    int i;

    for (i = 0; i < arrlen(cmd_func_tbl); i++) {
        _cmd_register_func(cmd_func_tbl[i].func, cmd_func_tbl[i].name);
    }
    cmd_register_i32(&g_stdin_eof_timeout, "stdinwait");
    return 0;
}

void cmd_deinit (void)
{
    bsp_stdin_unreg_if(__cmd_stdin_forward);
    cmd_unregister_all();
}

char *cmd_get_eof (const char *data, int len)
{
    char *p;

    p = strstr(data, __eof_sequence);
    return p;
}

static void __cmd_filebuf_flush (void)
{
    int len = 0;
    __cmd_tofile_usage_tsf = 0;
    if (!__cmd_tofile_full) {
        return;
    }
    len = d_write(stdin_file, &__cmd_tofile_buf[0], __cmd_tofile_full);
    __cmd_tofile_full = 0;
}

static int __cmd_filebuf_write (void *data, int len)
{
    __cmd_tofile_usage_tsf = d_time() + 200;
    if (__cmd_tofile_full > __cmd_tofile_tresh) {
        len = __cmd_tofile_full;
        len = d_write(stdin_file, &__cmd_tofile_buf[0], len);
        __cmd_tofile_full = 0;
    }
    len = min(len, stdin_to_path_bytes_limit);
    memcpy(&__cmd_tofile_buf[__cmd_tofile_full], data, len);
    __cmd_tofile_full += len;
    return len;
}

static int __cmd_set_stdin_char (int argc, const char **argv)
{
    int num;

    if (bsp_stdin_type == STDIN_CHAR) {
        return -1;
    }
    num = argv[0][0] - '0';
    assert(num >= 0);
    switch (num) {
        default :
            dprintf("stdin > %i\n", num);
            cmd_stdin_path_close();
            bsp_stdin_unstash(saved_stdin_hdlr);
            g_serial_rx_eof = '\n';
            bsp_stdin_type = STDIN_CHAR;
        break;
    }
    return 0;
}

static int __cmd_set_stdin_file (int argc, const char **argv)
{
    int f;
    const char *payload = NULL;

    if (bsp_stdin_type == STDIN_PATH) {
        return -1;
    }
    assert(argc > 1);
    if (argc > 2) {
        payload = argv[2];
    }

    dprintf("stdin: path=[%s] att=[%s] ", argv[0], argv[1]);
    if (stdin_to_path_bytes_limit > 0) {
        dprintf("rx bytes=[%u]", stdin_to_path_bytes_limit);
    }
    dprintf("\n");

    d_open(argv[0], &f, argv[1]);
    stdin_file = f;

    if (f < 0) {
        dprintf("unable to create : \'%s\'\n", argv[0]);
        return -1;
    }
    dprintf("stdin > \'%s\'\n", argv[0]);
    g_serial_rx_eof = 0;
    saved_stdin_hdlr = bsp_stdin_stash(cmd_stdin_to_path_write);
    g_stdin_eof_timeout_var = d_time() + g_stdin_eof_timeout;
    stdin_to_path_bytes_cnt = 0;
    bsp_stdin_type = STDIN_PATH;
    if (payload) {
        cmd_exec_dsr("", payload, NULL, NULL);
    }

    return f;
}

static int cmd_stdin_path_close (void)
{
    if (__cmd_tofile_full) {
        dprintf("file closing before flush\n");
        assert(0);
    }
    dprintf("closing path\n");
    d_close(stdin_file);
    stdin_file = -1;
    stdin_eof_recv = 0;
    g_stdin_eof_timeout_var = 0;
    return 0;
}

static int cmd_stdin_to_path_write (int argc, const char **argv)
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

    len = __cmd_filebuf_write(data, len);

    stdin_to_path_bytes_cnt += len;
    stdin_to_path_bytes_limit -= len;
    assert(stdin_to_path_bytes_limit >= 0);

    if (!stdin_to_path_bytes_limit) {
        dprintf("done, received : %u bytes\n", stdin_to_path_bytes_cnt);
    }
    if (eof || !stdin_to_path_bytes_limit) {
        stdin_eof_recv = 1;
        g_stdin_eof_timeout_var = d_time() + g_stdin_eof_timeout;
        /*[stdin > 0 -g] : means- set stdin as character stream
           and flush everything.*/
        cmd_exec_dsr("bsp", "stdin > 0 -g", NULL, NULL);
    }
    return 0;
}

static void __cmd_stdin_force_flush (void)
{
    dprintf("stdin: flushed\n");
    __cmd_filebuf_flush();
}

const char *__cmd_stdin_usage =
"\n-p - write text message, - -p <message>\n"
"-f - mark eof sequence, if size undefined\n"
"     - -f <pattern>,"
      "file will be closed after <pattern> match\n"
"-n - total bytes to receive\n"
"-a - file open attr, - [+w],- create/replace,\n"
"     [w],- open only, file will be extended\n"
"-s - packet size, how much bytes i want per burst\n"
"-t - timeout, how long to wait before flush"
"-g - stdin flush";

static int __cmd_stdin_redirect (int argc, const char **argv)
{
    int num = -1, pktsize = -1;
    char attr[3] = "+w";
    int bytes_limit;
    char payload[256];
    const char *argvbuf[CMD_MAX_ARG];

    cmd_keyval_t kvpload =   CMD_KVSTR_S("p", &payload[0]);
    cmd_keyval_t kv_eofseq = CMD_KVSTR_S("f", &__eof_sequence[0]);
    cmd_keyval_t kv_fflush = CMD_KVI32_S("g", NULL);

    cmd_keyval_t kvarr[] = 
    {
        CMD_KVI32_S("n", &bytes_limit),
        CMD_KVSTR_S("a", &attr[0]),
        CMD_KVI32_S("s", &pktsize),
        CMD_KVI32_S("t", &g_stdin_eof_timeout),
    };
    cmd_keyval_t *kvlist[] =
        {&kvarr[0], &kvarr[1], &kvarr[2], &kvarr[3],
         &kvpload, &kv_eofseq, &kv_fflush};

    if (argc < 1) {
        dprintf("usage : %s\n", __cmd_stdin_usage);
        return argc;
    }

    /*path must be always as argv[0]*/
    argc = cmd_argv_to_parm(&kvlist[0], &kvlist[arrlen(kvlist) - 1],
                        argc - 1, argvbuf, &argv[1]);

    if (argc < 0) {
        dprintf("%s() : unexpected arguments\n", __func__);
        return -1;
    }

    stdin_to_path_bytes_limit = bytes_limit;

    if (kv_fflush.valid) {
        dprintf("stdin : force flush\n");
        __cmd_stdin_force_flush();
    }

    if (sscanf(argv[0], "%i", &num)) {

        const char *__argv[] = {"0"};
        int __argc = 1;

        if (__cmd_set_stdin_char(__argc, __argv) < 0) {
            return 0;
        }
    } else {

        const char *__argv[] = {argv[0], attr, NULL};
        int __argc = 2;

        if (kvpload.valid) {
            __argv[2] = payload;
            __argc++;
        }
        if (__cmd_set_stdin_file(__argc, __argv) < 0) {
            return 0;
        }
    }
    return argc;
}

static int cmd_stdin_ctrl (int argc, const char **argv)
{
    if (argc < 1) {
        dprintf("usage : %s\n", __cmd_stdin_usage);
        return argc;
    }

    switch (argv[0][0]) {
        case '>':
            argc = __cmd_stdin_redirect(argc--, &argv[1]);
        break;
        default :
            dprintf("%s() : unexpected sym : [%c]\n", __func__, argv[0][0]);
            argc = 0;
        break;
    }
    return argc;
}

void cmd_tickle (void)
{
    cmd_exec_pending(cmd_txt_exec);
    if (g_stdin_eof_timeout_var && g_stdin_eof_timeout_var < d_time()) {
        dprintf("wait for stdin : timeout\n");
        dprintf("flushing..\n");
        cmd_exec_dsr("bsp", "stdin > 0 -g", NULL, NULL);
    } else if (__cmd_tofile_usage_tsf && __cmd_tofile_usage_tsf < d_time()) {
        dprintf("stdin: flushed\n");
        __cmd_filebuf_flush();
    }
}

static int __cmd_var_readonly (const char *name)
{
    int i;

    for (i = 0; i < arrlen(cmd_func_tbl); i++) {
        if (strcmp(cmd_func_tbl[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int __cmd_bsp_export_cmd (cmd_func_map_t *hdlrs, int len)
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

static int __cmd_stdin_forward (int argc, const char **argv)
{
    dvar_int_t *v = dvar_head;

    if (argc < 1) {
        return -1;
    }

    while (v) {
        if (strncmp(argv[0], v->name, v->namelen) == 0) {
            argc = __cmd_stdin_handle(v, --argc, ++argv);
            return argc;
        }
        v = v->next;
    }
    return argc;
}

static int __cmd_stdin_handle (dvar_int_t *v, int argc, const char **argv)
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

void cmd_exec_dsr (const char *cmd, const char *text, void *user1, void *user2)
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

static cmdexec_t *__cmd_iter_next (char *text, cmdexec_t *cmd)
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

void cmd_exec_pending (cmd_handler_t hdlr)
{
     cmdexec_t cmd;
     char buf[256];
     cmdexec_t *cmdptr = __cmd_iter_next(buf, &cmd);

     while (cmdptr) {
        hdlr(buf, cmdptr->len);
        cmdptr = __cmd_iter_next(buf, cmdptr);
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

static int __cmd_fs_print_dir (int argc, const char **argv)
{
    const char *ppath;
    char pathbuf[CMD_MAX_PATH];
    const char *argvbuf[16];
    uint32_t recursion = MAX_RECURSION;

    cmd_keyval_t kva = CMD_KVI32_S("n", &recursion);
    cmd_keyval_t *kvlist[] ={&kva};

    assert(arrlen(argvbuf) > argc);

    argc = cmd_argv_to_parm(&kvlist[0], &kvlist[arrlen(kvlist) - 1],
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

static int cmd_bsp_sys_reset (int argc, const char **argv)
{
extern void SystemSoftReset (void);
    dprintf("board reset...\n");
    serial_flush();
    SystemSoftReset();
    assert(0);
    return -1;
}

static int _cmd_export_all (int argc, const char **argv)
{
    return __cmd_bsp_export_cmd(cmd_priv_func_tbl + 1, arrlen(cmd_priv_func_tbl) - 1);
}

static int __cmd_exec_internal (int argc, const char **argv)
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

int cmd_txt_exec (const char *cmd, int len)
{
    char buf[256];
    len = snprintf(buf, sizeof(buf), "%s", cmd);
    bsp_inout_forward(buf, len, '<');
    /*TODO : cmd errno ?*/
    return 0;
}

int cmd_install_executable (int argc, const char **argv)
{
    arch_word_t progaddr;

    if (argc < 2) {
        dprintf("usage : /path/to/file <boot address 0x0xxx..>");
        return -1;
    }
    if (!sscanf(argv[1], "%x", &progaddr)) {
        return -1;
    }
    bsp_install_exec((arch_word_t *)progaddr, argv[0]);

    return argc - 2;
}

const char *cmd_start_exec_usage =
"-p - path to file to load from\n"
"-a - args will be passed to app, - [-a \"myname -path tosomewhere\"] "
"ex : boot 0x08000000 -p /exe.bin -a \"superapplication\"";

int cmd_start_executable (int argc, const char **argv)
{
    const char *argvbuf[CMD_MAX_ARG];
    arch_word_t progaddr;
    const char **moreargs = NULL;
    int xcnt = 0, doinstall = 0;
    int i;
    char apparg[CMD_MAX_BUF] = {0};
    char binpath[CMD_MAX_PATH] = {0};

    cmd_keyval_t kvarr[] = 
    {
        CMD_KVSTR_S("p", &binpath[0]),
        CMD_KVSTR_S("a", &apparg[0]),
    };
    cmd_keyval_t *kvlist[arrlen(kvarr)];

    for (i = 0; i < arrlen(kvarr); i++) kvlist[i] = &kvarr[i];
    kvarr[1].handle = cmd_parm_load_str;

    if (argc < 1) {
        dprintf(cmd_start_exec_usage);
        return -1;
    }

    argc = cmd_argv_to_parm(&kvlist[0], &kvlist[arrlen(kvlist) - 1],
                                argc, argvbuf, argv);


    if (binpath[0]) {
        doinstall = 1;
    }

    /*prog addr always first*/
    if (!sscanf(argv[0], "%x", &progaddr)) {
        dprintf("cannot parse addr : [%s]\n", argv[0]);
        return -1;
    }

    if (doinstall) {
        bsp_exec_file_type_t type;
        type = bsp_bin_file_compat(binpath);
        switch (type) {
            case BIN_FILE:
                if (bsp_install_exec((arch_word_t *)progaddr, binpath) < 0) {
                    return 0;
                }
            break;
            case BIN_LINK:
                return bsp_exec_link(NULL, binpath);
                
            break;
            default:
                assert(0);
        }
    }

    argvbuf[0] = apparg;
    bsp_start_exec((arch_word_t *)progaddr, 1, &argvbuf[0]);

    return 0;
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

int cmd_util_cat (int argc, const char **argv)
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

    argc = cmd_argv_to_parm(&kvlist[0], &kvlist[arrlen(kvlist) - 1],
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
            hexdump((const uint8_t *)str, len, h);
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

static int cmd_util_serial (int argc, const char **argv)
{
    return argc;
}



#endif /*BSP_DRIVER*/
