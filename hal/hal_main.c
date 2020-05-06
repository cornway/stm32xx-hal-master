/* Includes ------------------------------------------------------------------*/

#include <bsp_api.h>
#include <stdarg.h>
#include <misc_utils.h>
#include <bsp_cmd.h>
#include <lcd_main.h>
#include <audio_main.h>
#include <input_main.h>
#include <debug.h>
#include <dev_io.h>
#include <nvic.h>
#include "../../common/int/mpu.h"
#include <heap.h>
#include <bsp_sys.h>

#if defined(USE_STM32F769I_DISCO)

#include "stm32f769i_discovery_lcd.h"

int const __cache_line_size = 32;

#elif defined(USE_STM32H745I_DISCO)

#include "stm32h745i_discovery_lcd.h"

int const __cache_line_size = 32;

#else /*USE_STM32F769I_DISCO*/
#error "Not supported"
#endif /*USE_STM32F769I_DISCO*/

#ifdef __MICROLIB
#error "I don't want to use microlib"
/*Microlib will reduce code size at the cost of performance,
    also due to HEAP replacement
*/
#endif

static void SystemClock_Config(void);

static void sys_exception_handler (uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3);

static void clock_fault (void)
{
    bug();
}

/*TODO : move to gpio.c/gpio.h*/
void hdd_led_on (void)
{
#if defined(USE_STM32F769I_DISCO)
    BSP_LED_On(LED2);
#endif
}

void hdd_led_off (void)
{
#if defined(USE_STM32F769I_DISCO)
    BSP_LED_Off(LED2);
#endif
}

void serial_led_on (void)
{
#if defined(USE_STM32F769I_DISCO)
    BSP_LED_On(LED1);
#endif
}

void serial_led_off (void)
{
#if defined(USE_STM32F769I_DISCO)
    BSP_LED_Off(LED1);
#endif
}

static void SystemClock_Config(void)
{
#if defined(USE_STM32F769I_DISCO)
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;
    HAL_StatusTypeDef  ret = HAL_OK;

    /* Enable Power Control clock */
    __HAL_RCC_PWR_CLK_ENABLE();

    /* The voltage scaling allows optimizing the power consumption when the device is
     clocked below the maximum system frequency, to update the voltage scaling value 
     regarding system frequency refer to product datasheet.  */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* Enable HSE Oscillator and activate PLL with HSE as source */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 400;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 8;
    RCC_OscInitStruct.PLL.PLLR = 7;

    ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if(ret != HAL_OK)
        clock_fault();

    /* Activate the OverDrive to reach the 200 MHz Frequency */
    ret = HAL_PWREx_EnableOverDrive();
    if(ret != HAL_OK)
        clock_fault();

    /* Select PLLSAI output as USB clock source */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CLK48;
    PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48SOURCE_PLL;
    PeriphClkInitStruct.PLLSAI.PLLSAIN = 192;
    PeriphClkInitStruct.PLLSAI.PLLSAIQ = 4;
    PeriphClkInitStruct.PLLSAI.PLLSAIP = RCC_PLLSAIP_DIV4;
    PeriphClkInitStruct.PLLSAIDivR = RCC_PLLSAIDIVR_2;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
        clock_fault();

    /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 clocks dividers */
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_6);
    if(ret != HAL_OK)
        clock_fault();
#endif
}

void CPU_CACHE_Enable(void)
{
    SCB_EnableICache();
    SCB_EnableDCache();
}

void CPU_CACHE_Disable (void)
{
    SCB_DisableDCache();
    SCB_DisableICache();
    SCB_CleanInvalidateDCache();
    SCB_InvalidateICache();
}

void CPU_CACHE_Reset (void)
{
    CPU_CACHE_Disable();
    CPU_CACHE_Enable();
}

int dev_hal_init (void)
{
    HAL_Init();
    SystemClock_Config();
    CPU_CACHE_Enable();
#if defined(USE_STM32F769I_DISCO)
    BSP_LED_Init(LED1);
    BSP_LED_Init(LED2);
#endif
    uart_hal_tty_init();
    cs_load_code(NULL, NULL, 0);
    return 0;
}
