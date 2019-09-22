#ifndef __INPUT_INT_H__
#define __INPUT_INT_H__

#include <stdint.h>

void joypad_bsp_init (void);
void joypad_tickle (void);
void joypad_bsp_deinit (void);


/*Drivers*/

typedef struct {
    void *(*init) (uint32_t *);
    void (*deinit) (void *);
    int (*read) (int8_t *, void *, int8_t *);
} gamepad_drv_t;

void joypad_attach_def (gamepad_drv_t *drv);
void joypad_attach_gp_50 (gamepad_drv_t *drv);

#endif /*__INPUT_INT_H__*/
