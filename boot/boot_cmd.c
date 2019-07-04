#include <string.h>
#include "../int/bsp_cmd_int.h"
#include "int/boot_int.h"
#include <dev_io.h>
#include <debug.h>

extern bsp_bin_t *boot_bin_head;

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

static void __bin_cmd_dump (arch_word_t addr, arch_word_t size, const char *path)
{
    int f, i, tmp;
    uint8_t *ptr = (uint8_t *)addr;

    dprintf("Bin dump:\n");
    d_open(path, &f, "+w");

    if (f < 0) {
        return;
    }
    dprintf("[ ");
    size = size * sizeof(arch_word_t);
    for (i = 0; i < size && tmp > 0;) {
        tmp = d_write(f, ptr, 1024 * 16);
        ptr += tmp;
        i += tmp;
        dprintf(">");
    }
    tmp = size - i;
    if (tmp > 0) {
        for (i = 0; i < tmp && tmp > 0;) {
            tmp = d_write(f, ptr, 1024);
            dprintf(">");
        }
    }
    dprintf(" ]\n");
    d_close(f);
    dprintf("Done; <0x%p> : <0x%x> bytes)\n", (void *)addr, size);
}


/*addres(hex) - size(hex) - path/to/file*/
static int bin_cmd_dump (int argc, const char **argv)
{
    arch_word_t addr, size;
    if (argc < 3) {
        dprintf("usage : addres(hex) - size(hex) - path/to/file(optional)\n");
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
    __bin_cmd_dump(addr, size / sizeof(arch_word_t), argv[2]);
    return argc;
}

/*addres(hex) - size(hex) - val(hex)*/
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

static const cmd_func_map_t boot_cmd_map [] =
{
    {"print", boot_print_bin_list},
    {"dump",  bin_cmd_dump},
    {"memset", bin_cmd_mem_set},
    {"copy",  bin_cmd_copy},
    {"rm",    bin_cmd_remove},
};

int boot_cmd_handle (int argc, const char **argv)
{
    int i = 0;
    if (argc < 1) {
        return -1;
    }
    for (i = 0; i < arrlen(boot_cmd_map); i++) {
        if (strcmp(argv[0], boot_cmd_map[i].name) == 0) {
            dprintf("executing : \'%s\'\n", argv[0]);
            return boot_cmd_map[i].func(argc - 1, &argv[1]);
        }

    }
    dprintf("unknown cmd : \'%s\'", argv[0]);
    return -1;
}


