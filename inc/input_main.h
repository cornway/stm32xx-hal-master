#ifndef _INPUT_MAIN_H
#define _INPUT_MAIN_H

#include "stdint.h"
#include "touch.h"
#include <misc_utils.h>

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

#define PAD_FREQ_LOW        0x1
#define PAD_FUNCTION        0x2
#define PAD_SET_FLYLOOK     0x4
#define PAD_LOOK_CONTROL    0x8
#define PAD_LOOK_UP         0x10
#define PAD_LOOK_DOWN       0x20
#define PAD_LOOK_CENTRE     0x40

typedef struct kbdmap_s {
    int16_t key;
    int16_t flags;
} kbdmap_t;

typedef enum {
    keyup,
    keydown,
} i_key_state_t;

typedef struct {
    int sym;
    i_key_state_t state;
    int16_t x, y;
} i_event_t;

enum {
    JOY_KEY_OFFSET  = 0,
    JOY_K1          = JOY_KEY_OFFSET,
    JOY_K2          = JOY_K1 + 1,
    JOY_K3          = JOY_K2 + 1,
    JOY_K4          = JOY_K3 + 1,
    JOY_K5          = JOY_K4 + 1,
    JOY_K6          = JOY_K5 + 1,
    JOY_K7          = JOY_K6 + 1,
    JOY_K8          = JOY_K7 + 1,
    JOY_K9          = JOY_K8 + 1,
    JOY_K10         = JOY_K9 + 1,
    JOY_KMAX        = JOY_K10 + 1,

    JOY_UPARROW     = JOY_KMAX,
    JOY_DOWNARROW   = JOY_UPARROW + 1,
    JOY_LEFTARROW   = JOY_DOWNARROW + 1,
    JOY_RIGHTARROW  = JOY_LEFTARROW + 1,
    JOY_STD_MAX         = JOY_RIGHTARROW + 1,
    JOY_ANALOG      = JOY_STD_MAX,
    JOY_MAX         = JOY_ANALOG + 1,
}; /*regular keys*/

enum {
    K_EX_LOOKUP,
    K_EX_LOOKDOWN,
    K_EX_LOOKCENTER,
    K_EX_MAX,
}; /*extra keys (activsted by hold 'ctrl' key)*/


void input_bsp_init (void);
void input_bsp_deinit (void);
void input_soft_init (const kbdmap_t kbdmap[JOY_STD_MAX]);
void input_bind_extra (int type, int sym);
void input_tickle (void);
void input_proc_keys (i_event_t *evts);
i_event_t *input_post_key (i_event_t  *evts, i_event_t event);
d_bool input_is_touch_present (void);


#endif /*_INPUT_MAIN_H*/
