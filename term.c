
#include <misc_utils.h>
#include <debug.h>

#define DEBUG_SERIAL_MAX_CLBK 4

static serial_rx_clbk_t serial_rx_clbk[DEBUG_SERIAL_MAX_CLBK] = {NULL};
static serial_rx_clbk_t *last_rx_clbk = &serial_rx_clbk[0];

void term_register_handler (serial_rx_clbk_t clbk)
{
    if (last_rx_clbk == &serial_rx_clbk[DEBUG_SERIAL_MAX_CLBK]) {
        return;
    }
    *last_rx_clbk++ = clbk;
}

void term_unregister_handler (serial_rx_clbk_t clbk)
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



