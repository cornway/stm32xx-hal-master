
#include <stm32h7xx_hal.h>

#include <bsp_sys.h>

#define HSEM_ID_0 (0U)

void hdd_led_on (void)
{
}

void hdd_led_off (void)
{
}

void serial_led_on (void)
{
}

void serial_led_off (void)
{
}

void CPU_CACHE_Disable (void)
{
}

static void clock_fault (void)
{
    arch_rise(NULL);
}

static void SystemClock_Config(void)
{
    /*HW semaphore Clock enable*/
    __HAL_RCC_HSEM_CLK_ENABLE();

    /* Activate HSEM notification for Cortex-M4*/
    HAL_HSEM_ActivateNotification(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_0));

    /* 
    Domain D2 goes to STOP mode (Cortex-M4 in deep-sleep) waiting for Cortex-M7 to
    perform system initialization (system clock config, external memory configuration.. )   
    */
    HAL_PWREx_ClearPendingEvent();
    HAL_PWREx_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFE, PWR_D2_DOMAIN);

    /* Clear HSEM flag */
    __HAL_HSEM_CLEAR_FLAG(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_0));
}

int dev_hal_init (void)
{
    SystemClock_Config();
    HAL_Init();
}