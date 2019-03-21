#ifndef __INPUT_INT_H__
#define __INPUT_INT_H__

#include <stdint.h>

void joypad_bsp_init (void);
void joypad_tickle (void);

typedef union {
    uint8_t data[8];
    uint64_t dword;
} usb_data_t;


#endif /*__INPUT_INT_H__*/
