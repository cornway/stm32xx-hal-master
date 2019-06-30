#if defined(BSP_DRIVER)

#include <string.h>
#include "int/term_int.h"
#include "int/bsp_cmd_int.h"
#include <bsp_cmd.h>
#include <misc_utils.h>
#include <debug.h>


#define TERM_MAX_CMD_BUF 256

#define DEBUG_SERIAL_MAX_CLBK 4
#define MAX_TOKENS 16

static cmd_func_t serial_rx_clbk[DEBUG_SERIAL_MAX_CLBK] = {NULL};
static cmd_func_t *last_rx_clbk = &serial_rx_clbk[0];

inout_clbk_t inout_clbk = NULL;


static inline char str_char_printable (char c)
{
    if (c < 0x20 || c >= 0x7f) {
        return ' ';
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

int str_tokenize (char **tok, int tokcnt, char *str)
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

void debug_add_rx_handler (cmd_func_t clbk)
{
    if (last_rx_clbk == &serial_rx_clbk[DEBUG_SERIAL_MAX_CLBK]) {
        return;
    }
    *last_rx_clbk++ = clbk;
}

void debug_rm_rx_handler (cmd_func_t clbk)
{
    cmd_func_t *first = &serial_rx_clbk[0];
    if (!last_rx_clbk) {
        return;
    }
    while (first != &serial_rx_clbk[DEBUG_SERIAL_MAX_CLBK]) {
        if (*first == clbk) {
            *first = *(--last_rx_clbk);
            *last_rx_clbk = NULL;
            return;
        }
        first++;
    }
}

static void __bsp_in_handle_cmd 
                (cmd_func_t *_begin, cmd_func_t *end,
                int argc, const char **argv);

cmd_func_t stdin_redir = NULL;

cmd_func_t bsp_stdin_push (cmd_func_t func)
{
    cmd_func_t tmp = stdin_redir;
    stdin_redir = func;
    return tmp;
}

cmd_func_t bsp_stdin_pop (cmd_func_t func)
{
    cmd_func_t tmp = stdin_redir;
    stdin_redir = func;
    return tmp;
}

void bsp_in_handle_cmd (char *buf, int size)
{
    char *argv_buf[MAX_TOKENS] = {NULL};
    const char **argv = (const char **)&argv_buf[0];
    int argc = MAX_TOKENS, offset = 0;

    if (stdin_redir) {
        offset = size;
        size = stdin_redir(size, (const char **)&buf);
        offset = offset - size;
    }
    if (!size) {
        return;
    }
    argc = str_tokenize(&argv_buf[0], argc, buf + offset);
    __bsp_in_handle_cmd(&serial_rx_clbk[0], last_rx_clbk, argc, argv);
}

static void __bsp_in_handle_cmd 
                (cmd_func_t *_begin, cmd_func_t *end,
                int argc, const char **argv)
{
    cmd_func_t *begin = _begin;
    int attemption = 0;
    int prev_argc = argc;

    while (d_true) {

        do {
            argv = &argv[prev_argc - argc];
            argc = (*begin)(argc, argv);
            if (argc <= 0) {
                return;
            }
            begin++;
            attemption++;
        } while (argc > 0 && begin < end);
        if (argc <= 0) {
            break;
        }
        if (prev_argc == argc) {
            dprintf("unknown text\n");
            dprintf("att : %i\n", attemption);
            break;
        }
        prev_argc = argc;
        begin = _begin;
    }
}

void hexdump (const uint8_t *data, int len, int rowlength)
{
    int x, y, xn;
    for (y = 0; y <= len / rowlength; y++) {
        xn = len < rowlength ? len : rowlength;
        dprintf("[0x%04x:0x%04x] : ", y, y + xn);
        for (x = 0; x < xn; x++) {
            dprintf("0x%02x ", data[x + y * rowlength]);
        }
        dprintf("\n");
    }
}

#endif

