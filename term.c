#if defined(BSP_DRIVER)

#include <string.h>
#include <misc_utils.h>
#include <debug.h>
#include "int/term_int.h"

#define TERM_MAX_CMD_BUF 256

#define DEBUG_SERIAL_MAX_CLBK 4
#define MAX_TOKENS 16

static serial_rx_clbk_t serial_rx_clbk[DEBUG_SERIAL_MAX_CLBK] = {NULL};
static serial_rx_clbk_t *last_rx_clbk = &serial_rx_clbk[0];

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
        p = strtok(NULL, " ");
        tok++;
        tokcnt--;
        *tok = p;
    }
    return toktotal - tokcnt;
}

void debug_add_rx_handler (serial_rx_clbk_t clbk)
{
    if (last_rx_clbk == &serial_rx_clbk[DEBUG_SERIAL_MAX_CLBK]) {
        return;
    }
    *last_rx_clbk++ = clbk;
}

void debug_rm_rx_handler (serial_rx_clbk_t clbk)
{
    serial_rx_clbk_t *first = &serial_rx_clbk[0];
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

void bsp_in_handle_cmd (char *buf, int size)
{
    serial_rx_clbk_t *clbk = &serial_rx_clbk[0];
    char *argv_buf[MAX_TOKENS] = {NULL};
    char **argv = &argv_buf[0];
    int attemption = 0;
    int argc = MAX_TOKENS, prev_argc = 0;

    if (*clbk == NULL) {
        return;
    }
    argc = str_tokenize(argv, argc, buf);
    prev_argc = argc;
    
    while (d_true) {

        while (argc > 0 && clbk != last_rx_clbk) {
            argv = &argv[prev_argc - argc];
            argc = (*clbk)(argc, argv);
            if (argc <= 0) {
                return;
            }
            clbk++;
            attemption++;
        }
        if (argc <= 0) {
            break;
        }
        if (prev_argc == argc) {
            dprintf("unknown text \'%s\'\n", buf);
            dprintf("att : %i\n", attemption);
            return;
        }
        prev_argc = argc;
        clbk = &serial_rx_clbk[0];
    }
}

void hexdump (const uint8_t *data, int len, int rowlength)
{
    int x, y, xn;
    dprintf("%s :\n", __func__);
    for (y = 0; y <= len / rowlength; y++) {
        xn = len < rowlength ? len : rowlength;
        dprintf("0x%8x:0x%8x    ", y, y + xn);
        for (x = 0; x < xn; x++) {
            dprintf("0x%02x, ", data[x + y * rowlength]);
        }
        dprintf("\n");
    }
}

#endif

