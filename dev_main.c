/* Includes ------------------------------------------------------------------*/
#include <bsp_api.h>
#include <stdarg.h>
#include "main.h"
#include <stm32f769i_discovery.h>
#include "lcd_main.h"
#include "audio_main.h"
#include "input_main.h"
#include "dev_io.h"
#include "debug.h"
#include "misc_utils.h"
#include "nvic.h"
#include <mpu.h>
#include <heap.h>
#include <bsp_sys.h>

extern bspapi_t *bsp_api_attach (void);
extern void VID_PreConfig (void);

static void SystemClock_Config(void);
static void CPU_CACHE_Enable(void);
static void SystemDump (void);

int g_dev_debug_level = DBG_ERR;

extern int mainloop (int argc, const char *argv[]);

static void clock_fault (void)
{
    bug();
}

#if !BSP_INDIR_API

#if (_USE_LFN == 3)
#error "ff_malloc, ff_free must be redefined to Sys_HeapAlloc"
#endif

#ifdef STM32F7
int const __cache_line_size = 32;
#else
#error "Cache line size unknown"
#endif

/** The prototype for the application's main() function */

void dumpstack (void)
{
}

void fatal_error (char *message, ...)
{
    va_list argptr;

    va_start (argptr, message);
    dvprintf (message, argptr);
    va_end (argptr);

    serial_flush();
    bug();
}

/* Private function prototypes -----------------------------------------------*/

/*TODO : move to gpio.c/gpio.h*/
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

static int con_echo (const char *buf, int len)
{
    dprintf("@: %s\n", buf);
    return 0; /*let it be processed by others*/
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

void dev_deinit (void)
{
extern void screen_release (void);
extern void Sys_LeakCheck (void);

    irqmask_t irq = NVIC_IRQ_MASK;
    dprintf("%s() :\n", __func__);
    debug_rm_rx_handler(con_echo);

    screen_release();
    vid_deinit();
    dev_io_deinit();
    audio_deinit();
    profiler_deinit();
    input_bsp_deinit();
    vid_deinit();

    irq_save(&irq);
    assert(!irq);
    HAL_DeInit();
}

#endif /*!BSP_INDIR_API*/


#if defined(BSP_DRIVER)

int dev_init (void (*userinit) (void))
{
    SystemClock_Config();
    HAL_Init();
    serial_init();
    userinit();
    
    dev_io_init();

    BSP_LED_Init(LED1);
    BSP_LED_Init(LED2);

    debug_add_rx_handler(con_echo);

    audio_init();
    input_bsp_init();
    profiler_init();
    vid_init();
    SystemDump();
    d_dvar_int32(&g_dev_debug_level, "dbglvl");
    return 0;
}

void __dev_init (void)
{
    heap_init();
    mpu_init();
}

#else /*BSP_DRIVER*/

#define dev_init(user) g_bspapi->sys.init(user)

void __dev_init (void)
{
    heap_init();
}

#endif /*BSP_DRIVER*/

int dev_main (void)
{
    g_bspapi = bsp_api_attach();
#if defined(BSP_DRIVER)
    CPU_CACHE_Enable();
#endif /*BSP_DRIVER*/
    dev_init(__dev_init);

    audio_conf("samplerate=22050, volume=100");

    VID_PreConfig();
    mainloop(0, NULL);

    return 0;
}


