#if defined(BSP_DRIVER)

#include <string.h>
#include "int/term_int.h"
#include "int/bsp_cmd_int.h"
#include <bsp_cmd.h>
#include <misc_utils.h>
#include <debug.h>


#define TERM_MAX_CMD_BUF 256

#define INOUT_MAX_FUNC 4
#define MAX_TOKENS 16

typedef int (*tknmatch_t) (char c, int *state);

typedef enum {
    INVALID,
    SOLID,
    SQUASH,
} tkntype_t;

static cmd_func_t serial_rx_clbk[INOUT_MAX_FUNC] = {NULL};
static cmd_func_t *last_rx_clbk = &serial_rx_clbk[0];

static cmd_func_t serial_tx_clbk[INOUT_MAX_FUNC] = {NULL};
static cmd_func_t *last_tx_clbk = &serial_tx_clbk[0];

inout_clbk_t inout_early_clbk = NULL;

static inline char str_char_printable (char c)
{
    if (c < 0x20 || c >= 0x7f) {
        return 0;
    }
    return c;
}

void str_filter_printable (char *str)
{
    while (*str) {
        *str = str_char_printable(*str);
        str++;
    }
}

int str_parse_tok (const char *str, const char *tok, uint32_t *val)
{
    int len = strlen(tok), ret = 0;
    tok = strstr(str, tok);
    if (!tok) {
        return ret;
    }
    str = tok + len;
    if (*str != '=') {
        ret = -1;
        goto done;
    }
    str++;
    if (!sscanf(str, "%u", val)) {
        ret = -1;
    }
    ret = 1;
done:
    if (ret < 0) {
        dprintf("invalid value : \'%s\'\n", tok);
    }
    return ret;
}

/*Split 'quoted' text with other*/
static int
str_tkn_split (const char **argv, tknmatch_t match, int *argc, char *str)
{
    int matchcnt = 0, totalcnt = 0;
    int tknstart = 1, dummy = 0;
    int maxargs = *argc;

    argv[totalcnt++] = str;
    while (*str && totalcnt < maxargs) {

        if (match(*str, &dummy)) {
            *str = 0;
            if (!tknstart) {
                argv[totalcnt++] = str + 1;
            } else {
                argv[totalcnt++] = str + 1;
                matchcnt++;
            }
            tknstart = 1 - tknstart;
        }
        str++;
    }
    *argc = totalcnt;
    return matchcnt;
}

static int
str_tkn_continue (const char **dest, const char **src, tknmatch_t tkncmp,
                     tkntype_t *flags, int argc, int maxargc)
{
    int i;
    int tmp, total = 0;

    for (i = 0; i < argc; i++) {
        tmp = 0;
        if (flags[i] == SQUASH) {
            /*Split into*/
            maxargc = str_tokenize(dest, maxargc, (char *)src[i]);
            tmp = maxargc;
        } else {
            dest[0] = src[i];
            tmp = 1;
        }
        dest += tmp;
        total += tmp;
    }
    return total;
}


/*brief : remove empty strings - ""*/
static int str_tkn_clean (const char **dest, const char **src, tkntype_t *flags, int argc)
{
    int i = 0, maxargc = argc;

    for (i = 0; i < maxargc; i++) {
        if (src[i][0]) {
            dest[0] = src[i];
            /*each even - squashable, odd - solid, e.g - " ", ' ', { }, ..*/
            if (i & 0x1) {
                *flags = SOLID;
            } else {
                *flags = SQUASH;
            }
            flags++;
            dest++;
        } else {
            argc--;
        }
    }
    return argc;
}

int quotematch (char c, int *state)
{
    if (c == '\'' || c == '\"') {
        return 1;
    }
    return 0;
}

/*brief : convert " 1 '2 3 4' 5 6 " -> {"1", "2 3 4", "5 6"}*/
/*argc - in argc*/
/*argv - output buffer*/
/*ret - result argc*/
static int str_tokenize_string (char *buf, int argc, const char **argv)
{
    const char *tempbuf[MAX_TOKENS], **tempptr = &tempbuf[0];
    tkntype_t flags[MAX_TOKENS];

    int totalcnt;

    if (argc < 2) {
        return -1;
    }

    totalcnt = MAX_TOKENS;
    str_tkn_split(tempptr, quotematch, &totalcnt, buf);

    totalcnt = str_tkn_clean(argv, tempptr, flags, totalcnt);

    totalcnt = str_tkn_continue(tempptr, argv, quotematch, flags, totalcnt, MAX_TOKENS);

    totalcnt = str_tkn_clean(argv, tempptr, flags, totalcnt);
    return totalcnt;
}

static inline int str_tokenize_parms (char *buf, int argc, const char **argv)
{
    return str_tokenize_string(buf, argc, argv);
}

int str_tokenize (const char **tok, int tokcnt, char *str)
{
    char *p = str;
    int toktotal = tokcnt;
    *tok = p;
    p = strtok(str, " ");
    while (p && tokcnt > 0) {
        str_filter_printable(p);
        p = strtok(NULL, " ");
        tok++;
        tokcnt--;
        *tok = p;
    }
    return toktotal - tokcnt;
}

void bsp_stdin_register_if (cmd_func_t clbk)
{
    if (last_rx_clbk == &serial_rx_clbk[INOUT_MAX_FUNC]) {
        return;
    }
    *last_rx_clbk++ = clbk;
}

void bsp_stdout_register_if (cmd_func_t clbk)
{
    if (last_tx_clbk == &serial_tx_clbk[INOUT_MAX_FUNC]) {
        return;
    }
    *last_tx_clbk++ = clbk;
}

static cmd_func_t *__bsp_inout_unreg_if (cmd_func_t *begin, cmd_func_t *end,
                                    cmd_func_t *last, cmd_func_t h)
{
    cmd_func_t *first = &begin[0];
    if (!last) {
        return NULL;
    }
    while (first != &end[0]) {
        if (*first == h) {
            *first = *(--last);
            *last = NULL;
            return NULL;
        }
        first++;
    }
    return last;
}

void bsp_stdin_unreg_if (cmd_func_t clbk)
{
    last_rx_clbk = __bsp_inout_unreg_if(&serial_rx_clbk[0], &serial_rx_clbk[INOUT_MAX_FUNC],
                          last_rx_clbk, clbk);
}

void bsp_stout_unreg_if (cmd_func_t clbk)
{
    last_tx_clbk = __bsp_inout_unreg_if(&serial_tx_clbk[0], &serial_tx_clbk[INOUT_MAX_FUNC],
                          last_tx_clbk, clbk);
}

static void __bsp_stdin_iter_fwd 
                (cmd_func_t *_begin, cmd_func_t *end,
                int argc, const char **argv);

static void
__bsp_stout_iter_fwd_raw (cmd_func_t *_begin, cmd_func_t *end, int argc, const char **argv);

cmd_func_t stdin_redir = NULL;

cmd_func_t bsp_stdin_stash (cmd_func_t func)
{
    cmd_func_t tmp = stdin_redir;
    stdin_redir = func;
    return tmp;
}

cmd_func_t bsp_stdin_unstash (cmd_func_t func)
{
    cmd_func_t tmp = stdin_redir;
    stdin_redir = func;
    return tmp;
}

void bsp_inout_forward (char *buf, int size, char dir)
{
    char *argv_buf[MAX_TOKENS] = {NULL};
    const char **argv = (const char **)&argv_buf[0];
    int argc = MAX_TOKENS, offset = 0;

    if (dir == '<' && stdin_redir) {
        offset = size;
        size = stdin_redir(size, (const char **)&buf);
        offset = offset - size;
    }
    if (!size) {
        return;
    }
    switch (dir) {
        case '>':
            argc = 1;
            argv[0] = buf;
            __bsp_stout_iter_fwd_raw(&serial_tx_clbk[0], last_tx_clbk, argc, argv);
        break;
        case '<':

            argc = str_tokenize_parms(buf, argc, &argv[0]);
            __bsp_stdin_iter_fwd(&serial_rx_clbk[0], last_rx_clbk, argc, argv);
        break;
        default: assert(0);
    }
    
}


static void
__bsp_stout_iter_fwd_raw (cmd_func_t *_begin, cmd_func_t *end, int argc, const char **argv)
{
    cmd_func_t *begin = _begin;
    while (*begin && begin < end) {
        (*begin)(argc, argv);
        begin++;
    };
}

static void __bsp_stdin_iter_fwd 
                (cmd_func_t *_begin, cmd_func_t *end,
                int argc, const char **argv)
{
    cmd_func_t *begin = _begin;
    int prev_argc = argc;

    while (d_true) {

        do {
            argv = &argv[prev_argc - argc];
            argc = (*begin)(argc, argv);
            if (argc <= 0) {
                return;
            }
            begin++;
        } while (argc > 0 && begin < end);
        if (argc <= 0) {
            break;
        }
        if (prev_argc == argc) {
            dprintf("garbage\n");
            break;
        }
        prev_argc = argc;
        begin = _begin;
    }
}

void hexdump_u8 (const uint8_t *data, int len, int rowlength)
{
    int col, row, colmax;
    int maxrows, total = 0;

    if (!rowlength) {
        rowlength = len;
    }
    maxrows = len / rowlength;
    colmax = min(len, rowlength);

    for (row = 0; row < maxrows; row++) {

        dprintf("[0x%04x:0x%04x] : ", total, total + colmax);

        for (col = 0; col < colmax; col++) {
            dprintf("0x%02x ", data[row + row * rowlength]);
        }
        total += colmax;
        dprintf("\n");
    }
    len = len - (row * rowlength);
    assert(len >= 0 && len < rowlength);

    if (len) {

        dprintf("[0x%04x:0x%04x] >> ", total, total + colmax);
        for (col = 0; col < len; col++) {
            dprintf("0x%02x ", data[row + row * rowlength]);
        }
    }
}

void hexdump_le_u32 (const uint32_t *data, int len, int rowlength)
{
    int col, row, colmax;
    int maxrows, total;

    len = len / sizeof(uint32_t);

    if (!rowlength) {
        rowlength = len;
    }
    maxrows = len / rowlength;
    colmax = min(len, rowlength);

    for (row = 0; row < maxrows; row++) {

        dprintf("[0x%04x:0x%04x] >> ", total, total + colmax);

        for (col = 0; col < colmax; col++) {
            dprintf("0x%08x ", data[row + row * rowlength]);
        }
        total += colmax;
        dprintf("\n");
    }
    len = len - (row * rowlength);
    assert(len >= 0 && len < rowlength);

    if (len) {

        dprintf("[0x%04x:0x%04x] : ", total, total + colmax);
        for (col = 0; col < len; col++) {
            dprintf("0x%08x ", data[row + row * rowlength]);
        }
    }
}


#endif

