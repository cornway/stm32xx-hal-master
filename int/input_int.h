#ifndef __INPUT_INT_H__
#define __INPUT_INT_H__

#include <stdint.h>

void joypad_bsp_init (void);
void joypad_tickle (void);
void joypad_bsp_deinit (void);

void input_hal_read_ts (ts_status_t *ts_status);
void input_hal_deinit (void);
int input_hal_init (void);


/*Drivers*/

typedef struct gamepad_drv_s {
    void *(*init) (struct gamepad_drv_s *, uint32_t *);
    void (*deinit) (struct gamepad_drv_s *);
    int (*read) (struct gamepad_drv_s *, int8_t *, void *);
    void *user;
} gamepad_drv_t;

void joypad_attach_def (gamepad_drv_t *drv);
void joypad_attach_gp_50 (gamepad_drv_t *drv);

#endif /*__INPUT_INT_H__*/
