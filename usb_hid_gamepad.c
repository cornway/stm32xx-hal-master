#include "usbh_def.h"
#include "usbh_conf.h"
#include "usbh_core.h"
#include "usbh_hid.h"
#include "input_main.h"
#include "input_int.h"
#include "nvic.h"

#define LR_GPOS         0
#define LR_MASK         (0xffUL << LR_GPOS)
#define LR_IDLE         (0x7fUL << LR_GPOS)

#define UP_DOWN_GPOS    8
#define UP_DOWN_MASK    (0xffUL << UP_DOWN_GPOS)
#define UP_DOWN_IDLE    (0x7fUL << UP_DOWN_GPOS)

#define ANALOG_BK_GP      16
#define ANALOG_BK_MASK    (0xffUL << ANALOG_BK_GP)
#define ANALOG_BK_IDLE    (0x8CUL << ANALOG_BK_GP)

#define KEYS_GPOS       20
#define KEYS_MASK       (0xfffUL << KEYS_GPOS)
#define KEYS_IDLE       (0x0UL << KEYS_GPOS)

enum {
    __LEFTRIGHT = 0,
    __UPDOWN = 1,
    __ANALOG_BK = 2,
    __ANALOG_0 = 3,
    __ANALOG_1 = 4,
    __KSTART = 5,
    __KEND = 7,
    __MODE = 7,
    __MODE_ANALOG = 0x40,
    __MODE_STD = 0xc0,
};

#define ACT_IDLE        ((KEYS_IDLE | SHIFT_IDLE | CTL_IDLE) << 32)

#define JOY_IDLE_MASK   (UP_DOWN_IDLE | LR_IDLE | ACT_IDLE)
/*0x[C0 - analog off, 4 - on]   000  f  7f7f--[8C - analog off val]--7f7f*/
#define JOY_IDLE(in) ((in & JOY_IDLE_MASK) == JOY_IDLE_MASK)

#define M_UP     0
#define M_DOWN   0xff
#define M_LEFT   0
#define M_RIGHT  0xff
#define M_IDLE   0x7f

#define M_K1     0x1
#define M_K2     0x2
#define M_K3     0x4
#define M_K4     0x8
#define M_BL     1
#define M_TL     2
#define M_BR     4
#define M_TR     8
#define M_START  1
#define M_SELECT 2

static void USBH_UserProcess(USBH_HandleTypeDef * phost, uint8_t id);
USBH_StatusTypeDef USBH_HID_GamepadInit(USBH_HandleTypeDef *phost);

static USBH_HandleTypeDef hUSBHost;

irqmask_t usb_irq;
usb_data_t gamepad_data;
int8_t gamepad_data_ready = 0;

static int8_t keypads[JOY_STD_MAX];

static inline int
set_key_state (int pad_idx, int state)
{
    if (state) {
        keypads[pad_idx] = 1;
    } else if (keypads[pad_idx] >= 0) {
        keypads[pad_idx]--;
    }
    if (keypads[pad_idx] > 0) {
        return 1;
    }
    return 0;
}



int _joypad_read_std (usb_data_t *data, int8_t *pads)
{
    int i = 0, bit;
    uint32_t keys;
    uint8_t temp;

    pads[JOY_MAX] = -128;

    keys = (*(uint32_t *)&data->data[__KSTART] >> 4);

    for (bit = 0, i = JOY_KEY_OFFSET; i < JOY_KMAX; i++, bit++) {

        set_key_state(i, (keys >> bit) & 0x1);
    }

    temp = data->data[__UPDOWN];
    set_key_state(JOY_UPARROW, temp == M_UP);
    set_key_state(JOY_DOWNARROW, temp == M_DOWN);

    temp = data->data[__LEFTRIGHT];
    set_key_state(JOY_LEFTARROW, temp == M_LEFT);
    set_key_state(JOY_RIGHTARROW, temp == M_RIGHT);
    pads[JOY_ANALOG] = data->data[__ANALOG_BK];
    memcpy(pads, keypads, JOY_MAX);
    return JOY_STD_MAX;
}

int joypad_read (int8_t *pads)
{
    usb_data_t data;
    irqmask_t irq;
    uint8_t mode;

    if (!gamepad_data_ready) {
        return 0;
    }
    gamepad_data_ready = 0;

    irq_save_mask(&irq, ~usb_irq);
    memcpy(&data, &gamepad_data, sizeof(data));
    irq_restore(irq);

    mode = data.data[__MODE];

    if (mode == __MODE_STD) {
        return _joypad_read_std(&data, pads);
    } else {
        return _joypad_read_std(&data, pads);
    }
}


void joypad_bsp_init (void)
{
    USBH_Init(&hUSBHost, USBH_UserProcess, 0);
    USBH_RegisterClass(&hUSBHost, USBH_HID_CLASS);
    USBH_Start(&hUSBHost);
    for (int i = 0; i < JOY_STD_MAX; i++) {
        keypads[i] = -1;
    }
}

void USBH_HID_EventCallback(USBH_HandleTypeDef *phost)
{

}

void joypad_tickle (void)
{
    USBH_Process(&hUSBHost);
}

static void USBH_UserProcess(USBH_HandleTypeDef * phost, uint8_t id)
{

}

USBH_StatusTypeDef USBH_HID_GamepadInit(USBH_HandleTypeDef *phost)
{
  HID_HandleTypeDef *HID_Handle =  (HID_HandleTypeDef *) phost->pActiveClass->pData;
  irqmask_t temp;

  irq_bmap(&temp);

  if(HID_Handle->length > sizeof(gamepad_data))
  {
    HID_Handle->length = sizeof(gamepad_data);
  }
  HID_Handle->pData = (uint8_t *)gamepad_data.data;
  fifo_init(&HID_Handle->fifo, phost->device.Data, /*HID_QUEUE_SIZE * */sizeof(gamepad_data));

  irq_bmap(&usb_irq);
  usb_irq = usb_irq & (~temp);
  return USBH_OK;  
}


