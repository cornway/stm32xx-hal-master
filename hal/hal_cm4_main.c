
#include <stm32h7xx_hal.h>

#include "stm32h747i_discovery.h"
#include "stm32h747i_discovery_bus.h"

#include <arch.h>
#include <debug.h>
#include <heap.h>
#include <bsp_api.h>
#include <bsp_sys.h>
#include <gfx2d_mem.h>
#include <lcd_main.h>
#include <misc_utils.h>
#include <gfx.h>
#include <smp.h>
#include "../int/lcd_int.h"

void cm4_led_on (void)
{
    BSP_LED_On(LED2);
}

void cm4_led_off (void)
{
    BSP_LED_Off(LED2);
}

void status_led_on (void)
{
    BSP_LED_On(LED1);
}

void status_led_off (void)
{
    BSP_LED_Off(LED1);
}

void hdd_led_on (void)
{
    BSP_LED_On(LED4);
}

void hdd_led_off (void)
{
    BSP_LED_Off(LED4);
}

void serial_led_on (void)
{
    BSP_LED_On(LED3);
}

void serial_led_off (void)
{
    BSP_LED_Off(LED3);
}

void msleep (volatile uint32_t sleep)
{
    while (sleep-- > 0) {};
}

void cm4_hal_tick (void)
{
    hal_smp_task_t *task = NULL;

    int hsem_id = hal_smp_hsem_alloc("hsem_task");

    if (hsem_id >= 0) {
        if (hal_smp_hsem_lock(hsem_id)) {

            task = hal_smp_next_task();
            hal_smp_hsem_release(hsem_id);
        }
        if (task) {
            if (task->func) {
                status_led_on();
                task->func(task->arg);
                status_led_off();
            }
            hal_smp_remove_task(task);
        }
    }
}

int cm4_hal_init (void)
{
    HAL_Init();

    /* Configure the Cortex-M4 ART Base address to D2_AXISRAM_BASE : 0x10000000 : */
    __HAL_ART_CONFIG_BASE_ADDRESS(D2_AXISRAM_BASE);

    hal_smp_init(1);
    cm4_led_on();
    return 0;
}

void HSEM2_IRQHandler(void)
{
    HAL_HSEM_IRQHandler();
}

void SysTick_Handler (void)
{
    HAL_IncTick();
}

void TIM3_IRQHandler (void)
{

}

void USART6_IRQHandler (void)
{

}

