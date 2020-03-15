#include "stm32f769i_discovery_ts.h"

d_bool ts_hal_get_state (uint32_t *x, uint32_t *y)
{
    TS_StateTypeDef  TS_State;

    if (BSP_TS_GetState(&TS_State) != TS_OK) {
        *x = -1;
        *y = -1;
        return d_false;
    }
    *x = TS_State.touchX[0];
    *y = TS_State.touchY[0];
    return TS_State.touchDetected;
}

