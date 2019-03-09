#ifndef _INPUT_MAIN_H
#define _INPUT_MAIN_H

#include "stdint.h"
#include "touch.h"

#define GAMEPAD_USE_FLYLOOK 1

enum {
    TS_IDLE,
    TS_PRESSED,
    TS_CLICK,
    TS_RELEASED,
};

enum {
    TS_PRESS_OFF,
    TS_PRESS_ON,
};

#define PAD_FREQ_LOW   0x1
#define PAD_FUNCTION     0x2
#define PAD_SET_FLYLOOK   0x4
#define PAD_LOOK_CONTROL 0x8
#define PAD_LOOK_UP     0x10
#define PAD_LOOK_DOWN  0x20
#define PAD_LOOK_CENTRE 0x40

struct usb_gamepad_to_kbd_map {
    uint8_t key;
    uint8_t flags;
    uint8_t hit_cnt;
    uint8_t lo_trig: 4,
            hi_trig: 4;
};

typedef struct {
    uint8_t type;
} i_event_t;

enum {
    K_UP = 0,
    K_DOWN = 1,
    K_LEFT = 2,
    K_RIGHT = 3,

    K_K1 = 4,
    K_K2 = 5,
    K_K3 = 6,
    K_K4 = 7,
    K_BL = 8,
    K_BR = 9,
    K_TL = 10,
    K_TR = 11,
    K_START = 12,
    K_SELECT = 13,
    K_MAX = 14,
};


void I_GetEvent (void);

int gamepad_read (int8_t *pads);

void gamepad_process (void);



#endif /*_INPUT_MAIN_H*/
