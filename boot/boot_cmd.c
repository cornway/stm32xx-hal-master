#include <string.h>
#include <debug.h>
#include "../int/bsp_cmd_int.h"
#include "../int/term_int.h"
#include "int/boot_int.h"
#include <dev_io.h>


extern bsp_bin_t *boot_bin_head;

int g_boot_log_level = -1;

static int boot_print_bin_list (int argc, const char **argv)
{
    int i = 0;
    bsp_bin_t *bin = boot_bin_head;

    dprintf("%s() :\n", __func__);
    while (bin) {
        if (bin->filetype == BIN_FILE) {
            dprintf("[%i] %s, %s, <0x%p> %u bytes\n",
                i++, bin->name, bin->path, (void *)bin->progaddr, bin->size);
        } else if (bin->filetype == BIN_LINK) {
            dprintf("[%i] %s, %s, <link file>\n",
                i++, bin->name, bin->path);
        } else {
            dprintf("unable to handle : [%i] %s, %s, ???\n",
                i++, bin->name, bin->path);
            assert(0)
        }
        bin = bin->next;
    }
    return argc;
}

static void __bin_cmd_dump (arch_word_t addr, arch_word_t bytescnt, const char *path)
{
    int f, i, tmp;
    uint8_t *ptr = (uint8_t *)addr;

    dprintf("Bin dump:\n");
    d_open(path, &f, "+w");

    if (f < 0) {
        return;
    }
    dprintf("[ ");
    for (i = 0; i < bytescnt && tmp > 0;) {
        tmp = d_write(f, ptr, 1024 * 16);
        ptr += tmp;
        i += tmp;
        dprintf(">");
    }
    tmp = bytescnt - i;
    if (tmp > 0) {
        tmp = d_write(f, ptr, tmp);
        dprintf(">");
    }
    dprintf(" ]\n");
    d_close(f);
    dprintf("Done; <0x%p> : <0x%x> bytes)\n", (void *)addr, bytescnt);
}

static void __bin_cmd_hexdump_le_u32 (arch_word_t addr, arch_word_t bytescnt, const char *path)
{
    int f, i, tmp;
    uint8_t *ptr = (uint8_t *)addr;

    dprintf("Hex dump: <0x%p> : 0x%08x\n", (void *)addr, bytescnt);
    d_open(path, &f, "+w");

    if (f < 0) {
        return;
    }
    dprintf("[ ");

    for (i = 0; i < bytescnt && tmp > 0;) {
        tmp = __hexdump_le_u32(d_printf, f, (uint32_t *)ptr, 1024 * 16, 16);
        ptr += tmp;
        i += tmp;
        dprintf(">");
    }
    tmp = bytescnt - i;
    if (tmp > 0) {
        tmp = __hexdump_le_u32(d_printf, f, (uint32_t *)ptr, tmp, 16);
    }
    dprintf(" ]\n");
    d_close(f);
    dprintf("Done; <0x%x> bytes)\n", bytescnt);
}

const char *bin_cmd_dump_usage =
"usage : dump <address(hex)> <size>(hex) <path/to/file>\n"
"-h : use hexdump, no parameters, little endian, 32 bit\n";

static int bin_cmd_dump (int argc, const char **argv)
{
    arch_word_t addr, size;
    const char *argvbuf[16];

    cmd_keyval_t dohex = CMD_KVI32_S("h", NULL);
    cmd_keyval_t *kvlist[] ={&dohex};

    assert(arrlen(argvbuf) > argc);

    argc = cmd_parm_collect(&kvlist[0], &kvlist[arrlen(kvlist) - 1],
                                argc, argvbuf, argv);

    if (argc < 3) {
        dprintf("%s", bin_cmd_dump_usage);
        return - 1;
    }
    argc -= 3;
    if (!sscanf(argv[0], "%x", &addr)) {
        dprintf("fail to parse addr : \'%s\'", argv[0]);
        return -1;
    }
    if (!sscanf(argv[1], "%x", &size)) {
        dprintf("fail to parse size : \'%s\'", argv[1]);
        return -1;
    }
    if (dohex.valid) {
        __bin_cmd_hexdump_le_u32(addr, size, argv[2]);
    } else {
        __bin_cmd_dump(addr, size, argv[2]);
    }
    return argc;
}

static int bin_cmd_mem_set (int argc, const char **argv)
{
    arch_word_t addr, size, value = 0;
    if (argc < 2) {
        dprintf("usage : addres(hex) - size(hex) - val(hex)\n");
        return -1;
    }
    if (argc > 2) {
        if (!sscanf(argv[2], "%x", &addr)) {
            dprintf("fail to parse value : \'%s\'", argv[2]);
            dprintf("using default : [0]");
        }
        argc--;
    }
    argc -= 2;

    if (!sscanf(argv[0], "%x", &addr)) {
        dprintf("fail to parse addr : \'%s\'", argv[0]);
    }
    if (!sscanf(argv[1], "%x", &size)) {
        dprintf("fail to parse size : \'%s\'", argv[1]);
    }
    bhal_set_mem(NULL, (arch_word_t *)addr, size / sizeof(arch_word_t), value);
    return argc;
}

static int bin_cmd_copy (int argc, const char **argv)
{
    int fdst, fsrc;
    int srclen, dstlen, tmp;
    uint8_t buf[256];

    if (argc < 2) {
        /*TODO  : usage*/
        return -1;
    }
    argc -= 2;

    d_open(argv[0], &fdst, "+w");
    if (!fdst) {
        dprintf("cannot open : \'%s\'\n", argv[0]);
        return -1;
    }
    srclen = d_open(argv[1], &fsrc, "r");
    dstlen = 0;
    if (fsrc < 0) {
        d_close(fdst);
        dprintf("cannot open : \'%s\'\n", argv[1]);
        return -1;
    }
    while (dstlen < srclen) {
        tmp = d_read(fsrc, buf, sizeof(buf));
        if (!tmp) {
            dprintf("unexpected eof\n");
            break;
        }
        d_write(fdst, buf, tmp);
        dstlen += tmp;
    }
    dprintf("Done : %u bytes copied\n", dstlen);
    d_close(fsrc);
    d_close(fdst);

    return argc;
}

static int bin_cmd_remove (int argc, const char **argv)
{
    if (argc < 1) {
        /*TODO  : usage*/
        return -1;
    }
    argc -= 1;

    if (d_unlink(argv[0])) {
        dprintf("%s() : fail\n", __func__);
        return -1;
    }
    return argc;
}

int bin_install (int argc, const char **argv)
{
    arch_word_t progaddr;

    if (argc < 2) {
        dprintf("usage : <boot address 0x0xxx..> </path/to/file>");
        return -CMDERR_NOARGS;
    }
    if (!sscanf(argv[0], "%x", &progaddr)) {
        return -CMDERR_INVPARM;
    }
    bsp_install_exec((arch_word_t *)progaddr, argv[1]);

    return argc - 2;
}

const char *cmd_start_exec_usage =
"-p - path to file to load from\n"
"-a - args will be passed to app, - [-a \"myname -path tosomewhere\"]\n"
"ex : boot 0x08000000 -p /exe.bin -a \"superapplication\"\n";

int bin_execute (int argc, const char **argv)
{
    int i, err = CMDERR_OK, doinstall = 0;
    const char *argvbuf[CMD_MAX_ARG];
    char apparg[CMD_MAX_BUF] = {0};
    char binpath[CMD_MAX_PATH] = {0};
    arch_word_t progaddr;

    cmd_keyval_t kvarr[] = 
    {
        CMD_KVSTR_S("p", &binpath[0]),
        CMD_KVSTR_S("a", &apparg[0]),
    };
    cmd_keyval_t *kvlist[arrlen(kvarr)];

    for (i = 0; i < arrlen(kvarr); i++) kvlist[i] = &kvarr[i];
    kvarr[1].handle = cmd_parm_load_str;

    if (argc < 1) {
        dprintf("%s", cmd_start_exec_usage);
        return -CMDERR_NOARGS;
    }

    argc = cmd_parm_collect(&kvlist[0], &kvlist[arrlen(kvlist) - 1],
                                argc, argvbuf, argv);


    if (binpath[0]) {
        doinstall = 1;
    }

    /*prog addr always first*/
    if (!sscanf(argv[0], "%x", &progaddr)) {
        dprintf("cannot parse addr : [%s]\n", argv[0]);
        return -CMDERR_INVPARM;
    }

    if (doinstall) {
        bsp_exec_file_type_t type;
        type = bsp_bin_file_compat(binpath);
        switch (type) {
            case BIN_FILE:
                err = bsp_install_exec((arch_word_t *)progaddr, binpath);
            break;
            case BIN_LINK:
                return bsp_exec_link(NULL, binpath);
            break;
            default:
                assert(0);
        }
    }

    if (err == CMDERR_OK) {
        argvbuf[0] = apparg;
        bsp_start_exec((arch_word_t *)progaddr, 1, &argvbuf[0]);
    }
    return err;
}

static int boot_intutil_log (int argc, const char **argv);

static const cmd_func_map_t boot_cmd_map [] =
{
    {"print", boot_print_bin_list},
    {"dump",  bin_cmd_dump},
    {"memset", bin_cmd_mem_set},
    {"copy",  bin_cmd_copy},
    {"rm",    bin_cmd_remove},
    {"write", bin_install},
    {"boot",  bin_execute},
    {"log",   boot_intutil_log},
};

int boot_cmd_handle (int argc, const char **argv)
{
    int i = 0;
    if (argc < 1) {
        PRINT_CMD_MAP(boot_cmd_map);
        return -1;
    }
    for (i = 0; i < arrlen(boot_cmd_map); i++) {
        if (strcmp(argv[0], boot_cmd_map[i].name) == 0) {
            dprintf("executing : \'%s\'\n", argv[0]);
            return boot_cmd_map[i].func(argc - 1, &argv[1]);
        }

    }
    dprintf("unknown cmd : \'%s\'\n", argv[0]);
    return -1;
}

static int boot_log_stream = -1;
static int boot_log_use_console = 0;

int boot_log (int dummy, const char *fmt, ...)
{
    va_list         argptr;
    char buf[512];
    int size;

    assert(boot_log_use_console || boot_log_stream >= 0);

    va_start (argptr, fmt);
    size = vsnprintf(buf, sizeof(buf), fmt, argptr);
    if (boot_log_use_console) {
        size = dprintf(buf);
    } else {
        size = d_printf(boot_log_stream, buf);
    }
    va_end (argptr);
    return size;
}

#define _BITWIDTH(v) (sizeof(v) * 8)

int boot_log_hex (const void *data, int len)
{
    __hexdump(boot_log, boot_log_stream, (uint32_t *)data, _BITWIDTH(data), len, 16);
    return len;
}

static int
__boot_log_comp_hex_u32 (const void *a, const void *b, int size)
{
    const int linesize = 16;
    int maxlines = linesize;
    int line, i;
    char cmpbuf[linesize];
    uint32_t *ptra = (uint32_t *)a, *ptrb = (uint32_t *)b;

    size = size / sizeof(uint32_t);
    maxlines = size / linesize;

    for (line = 0; line < maxlines; line++) {
        for (i = 0; i < linesize; i++) {
            if (ptra[i] == ptrb[i]) {
                cmpbuf[i] = 'o';
            } else {
                cmpbuf[i] = 'X';
            }
        }
        boot_log(-1, "[0x%p : 0x%p] :""[ %s ]\n", ptra, ptrb, cmpbuf);
        ptra += linesize;
        ptrb += linesize;
    }
    return size * sizeof(uint32_t);
}

int
boot_log_comp_hex_u32 (const void *a, const void *b, int size)
{
    return __boot_log_comp_hex_u32(a, b, size);
}

static const char *boot_cmd_log_usage =
"usage : log <control> </path/to/file>\n"
"            <control> - [begin, end]; start or stop logging\n"
"-l      log verbosity; 0 - errors, 4 - everything\n"
"-a      append option; extend existing file\n";

static int boot_intutil_log (int argc, const char **argv)
{
    const char *argvbuf[CMD_MAX_ARG];
    const char *binpath;
    int loglevel = -1, append = 0, i;
    int err = CMDERR_OK;

    cmd_keyval_t kvarr[] = 
    {
        CMD_KVI32_S("l", &loglevel),
        CMD_KVI32_S("a", &append),
    };
    cmd_keyval_t *kvlist[arrlen(kvarr)];

    for (i = 0; i < arrlen(kvarr); i++) kvlist[i] = &kvarr[i];

    if (argc < 1) {
        dprintf("%s", boot_cmd_log_usage);
        return -CMDERR_NOARGS;
    }

    binpath = argv[1];

    argc = cmd_parm_collect(&kvlist[0], &kvlist[arrlen(kvlist) - 1],
                                argc, argvbuf, argv);

    if (strcmp(argv[0], "begin") == 0) {
        boot_log_use_console = 1;
        if (binpath[0]) {
            int f = -1;
            if (append) {
                d_open(binpath, &f, "w");
                if (f >= 0) {
                    err = d_seek(f, -1, DSEEK_END);
                }
            }
            if (f < 0) {
                d_open(binpath, &f, "+w");
            }
            if (f < 0) {
                dprintf("Path doesn't exist: [%s]", binpath);
                err = -CMDERR_NOPATH;
            } else {
                boot_log_use_console = 0;
                boot_log_stream = f;
            }
        }
        if (err == CMDERR_OK) {
            dprintf("BOOT: begin logging\n");
        }
    } else if (strcmp(argv[0], "end") == 0) {
        if (boot_log_use_console) {
            boot_log_use_console = 0;
            assert(boot_log_stream < 0);
        } else if (boot_log_stream >= 0) {
            assert(!boot_log_use_console);
            d_close(boot_log_stream);
            boot_log_stream = -1;
        }
        g_boot_log_level = BOOT_LOG_NONE;
        dprintf("BOOT: end logging\n");
    } else {
        dprintf("Unknown parameter: [%s]", argv[0]);
        err =-CMDERR_INVPARM;
    }
    if (err == CMDERR_OK) {
        if (0 == BOOT_LOG_CHECK(BOOT_LOG_ERR)) {
            g_boot_log_level = loglevel;
        } else {
            dprintf("loglevel undefined, using : 0(errors)");
        }
    }
    return err;
}

