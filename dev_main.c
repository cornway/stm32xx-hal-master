

/**
  ******************************************************************************
  * @file    JPEG/JPEG_DecodingUsingFs_DMA/Src/main.c
  * @author  MCD Application Team
  * @brief   This sample code shows how to use the HW JPEG to Decode a JPEG file with DMA method.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <stdarg.h>
#include "main.h"
#include "lcd_main.h"
#include "audio_main.h"
#include "input_main.h"
#include "dev_io.h"
#include "debug.h"
#include "misc_utils.h"
#include "nvic.h"
#include <mpu.h>

#if (_USE_LFN == 3)
#error "ff_malloc, ff_free must be redefined to Sys_HeapAlloc"
#endif

#ifdef STM32F7
int const __cache_line_size = 32;
#else
#error "Cache line size unknown"
#endif

int g_dev_debug_level = DBG_ERR;

extern void VID_PreConfig (void);

static void SystemDump (void);

volatile uint32_t systime = 0;

void dumpstack (void)
{
}

void bug (void)
{
    for (;;) {}
}

void fatal_error (char *message, ...)
{
    va_list argptr;

    va_start (argptr, message);
    dvprintf (message, argptr);
    va_end (argptr);
    
    bug();
}

static void clock_fault (void)
{
    bug();
}

/** The prototype for the application's main() function */
extern int mainloop (int argc, const char *argv[]);

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void CPU_CACHE_Enable(void);

void hdd_led_on (void)
{
    BSP_LED_On(LED2);
}

void hdd_led_off (void)
{
    BSP_LED_Off(LED2);
}

void serial_led_on (void)
{
    BSP_LED_On(LED1);
}

void serial_led_off (void)
{
    BSP_LED_Off(LED1);
}

void dev_tickle (void)
{
    audio_update();
    input_tickle();
    serial_tickle();
}

static int con_echo (const char *buf, int len)
{
    dprintf("@: %s\n", buf);
    return 0; /*let it be processed by others*/
}

int dev_main (void)
{
    CPU_CACHE_Enable();
    SystemClock_Config();
    HAL_Init();
    mpu_init();
    Sys_AllocInit();
    serial_init();

    BSP_LED_Init(LED1);
    BSP_LED_Init(LED2);

    serial_rx_callback(con_echo);

    audio_init();
    input_bsp_init();
    dev_io_init();
    screen_init();
    SystemDump();

    VID_PreConfig();

    d_dvar_int32(&g_dev_debug_level, "dbglvl");
    mainloop(0, NULL);

    return 0;
}

static void SystemClock_Config(void)
{
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
    RCC_OscInitStruct.PLL.PLLN = 432;
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

    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    PeriphClkInitStruct.PLLSAI.PLLSAIN = 384;
    PeriphClkInitStruct.PLLSAI.PLLSAIR = 7;
    PeriphClkInitStruct.PLLSAIDivR = RCC_PLLSAIDIVR_2;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

    /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 clocks dividers */
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7);
    if(ret != HAL_OK)
        clock_fault();
}

static void CPU_CACHE_Enable(void)
{
    /* Enable I-Cache */
    SCB_EnableICache();

    /* Enable D-Cache */
    SCB_EnableDCache();
}

static void SystemDump (void)
{
    NVIC_dump();
}

