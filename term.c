#if defined(BSP_DRIVER)

#include <string.h>
#include <misc_utils.h>
#include <debug.h>
#include "int/term_int.h"

#define TERM_MAX_CMD_BUF 256

#define DEBUG_SERIAL_MAX_CLBK 4

static serial_rx_clbk_t serial_rx_clbk[DEBUG_SERIAL_MAX_CLBK] = {NULL};
static serial_rx_clbk_t *last_rx_clbk = &serial_rx_clbk[0];

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


void term_parse (const char *buf, int size)
{
    serial_rx_clbk_t *clbk = &serial_rx_clbk[0];
    int cnt = size, prev_cnt, idx, attemption;
    char *pbuf = (char *)buf;

    if (*clbk == NULL) {
        return;
    }

    prev_cnt = cnt;
    while (d_true) {
        while (cnt > 0 && *pbuf && clbk != last_rx_clbk) {
        
            idx = (*clbk)(pbuf, cnt);
            if (idx < 0 || idx >= cnt) {
                return;
                /*handled*/
            }
            if (idx > 0) {
                pbuf += idx;
                cnt -= idx;
            }
            clbk++;
            attemption++;
        }
        if (cnt <= 0 || *pbuf == 0) {
            break;
        }
        /*Try until all text will be parsed*/
        if (prev_cnt == cnt) {
            dprintf("Cannot parse text \'%s\'\n", pbuf);
            dprintf("Attemptions : %i\n", attemption);
            return;
        }
        prev_cnt = cnt;
        clbk = &serial_rx_clbk[0];
    }
}


int str_parse_tok (const char *str, const char *tok, uint32_t *val)
{
    int len = strlen(tok), ret = 0;
    tok = strstr(str, tok);
    if (!tok) {
        return ret;
    }
    str = str + len;
    if (*str != '=') {
        ret = -1;
        goto done;
    }
    str++;
    if (!sscanf(str, "%u", &val)) {
        ret = -1;
    }
    ret = 1;
done:
    if (ret < 0) {
        dprintf("invalid config : \'%s\'\n", tok);
    }
    return ret;
}


void hexdump (const uint8_t *data, int len, int rowlength)
{
    int x, y, xn;
    dprintf("%s :\n", __func__);
    for (y = 0; y <= len / rowlength; y++) {
        xn = len < rowlength ? len : rowlength;
        dprintf("%d:%d    ", y, y + xn);
        for (x = 0; x < xn; x++) {
            dprintf("0x%02x, ", data[x + y * rowlength]);
        }
        dprintf("\n");
    }
}

cmdexec_t boot_cmd_pool[6];
cmdexec_t *boot_cmd_top = &boot_cmd_pool[0];

void exec_cmd_push (const char *cmd, const char *text, void *user1, void *user2)
{
    if (boot_cmd_top == &boot_cmd_pool[arrlen(boot_cmd_pool)]) {
        return;
    }
    boot_cmd_top->cmd = cmd;
    boot_cmd_top->text = text;
    boot_cmd_top->user1 = user1;
    boot_cmd_top->user2 = user2;
    boot_cmd_top++;
    dprintf("%s() : \'%s\' [%s]\n", __func__, cmd, text);
}

cmdexec_t *exec_cmd_pop (cmdexec_t *cmd)
{
    if (boot_cmd_top == &boot_cmd_pool[0]) {
        return NULL;
    }
    boot_cmd_top--;
    cmd->cmd = boot_cmd_top->cmd;
    cmd->text = boot_cmd_top->text;
    cmd->user1 = boot_cmd_top->user1;
    cmd->user2 = boot_cmd_top->user2;
    return cmd;
}

void exec_iterate_cmd (cmd_handler_t hdlr)
{
     cmdexec_t cmd;
     cmdexec_t *cmdptr = exec_cmd_pop(&cmd);
     char buf[TERM_MAX_CMD_BUF];
     int len;

     while (cmdptr) {
        len = snprintf(buf, sizeof(buf), "%s %s\n", cmdptr->cmd, cmdptr->text);
        hdlr(buf, len);
        cmdptr = exec_cmd_pop(cmdptr);
     }
}

void exec_cmd_execute (const char *cmd, int len)
{
    term_parse(cmd, len);
}

#endif

