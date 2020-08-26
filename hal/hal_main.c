/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <arch.h>
#include <bsp_api.h>
#include <misc_utils.h>
#include <nvic.h>
#include <bsp_cmd.h>
#include <gfx2d_mem.h>
#include <lcd_main.h>
#include <audio_main.h>
#include <input_main.h>
#include <debug.h>
#include <dev_io.h>
#include <heap.h>
#include <bsp_sys.h>
#include <serial.h>
#include <smp.h>

#include "../../common/int/mpu.h"

#if defined(USE_STM32H747I_DISCO)
#include "stm32h747i_discovery.h"
#include "stm32h747i_discovery_bus.h"
#elif defined(USE_STM32F769I_DISCO)
#include "stm32f769i_discovery.h"
#elif defined(USE_STM32H745I_DISCO)
#include "stm32h745i_discovery.h"
#include "stm32h745i_discovery_bus.h"
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
static void clock_fault (void)
{
    bug();
}

#if defined(USE_STM32F769I_DISCO)

/*TODO : move to gpio.c/gpio.h*/

void status_led_on (void)
{
}

void status_led_off (void)
{
}

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
}

#elif defined(USE_STM32H745I_DISCO) || defined(USE_STM32H747I_DISCO)

/*TODO : move to gpio.c/gpio.h*/

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

HAL_StatusTypeDef MX_LTDC_ClockConfig(LTDC_HandleTypeDef *hltdc)
{
    RCC_PeriphCLKInitTypeDef  PeriphClkInitStruct;
    PeriphClkInitStruct.PeriphClockSelection   = RCC_PERIPHCLK_LTDC;
    PeriphClkInitStruct.PLL3.PLL3M      = 5U;
    PeriphClkInitStruct.PLL3.PLL3N      = 132U;
    PeriphClkInitStruct.PLL3.PLL3P      = 2U;
    PeriphClkInitStruct.PLL3.PLL3Q      = 2U;
    PeriphClkInitStruct.PLL3.PLL3R      = 24U;
    if(HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
        clock_fault();
    return HAL_OK;
}

static void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;
    HAL_StatusTypeDef ret = HAL_OK;

    /*!< Supply configuration update enable */
    HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);

    /* The voltage scaling allows optimizing the power consumption when the device is 
     clocked below the maximum system frequency, to update the voltage scaling value 
     regarding system frequency refer to product datasheet.  */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    /* Enable HSE Oscillator and activate PLL with HSE as source */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
    RCC_OscInitStruct.CSIState = RCC_CSI_OFF;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;

    RCC_OscInitStruct.PLL.PLLM = 5;
    RCC_OscInitStruct.PLL.PLLN = 160;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLQ = 4;

    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
    ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if(ret != HAL_OK)
        clock_fault();

    /* Select PLL as system clock source and configure  bus clocks dividers */
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 | \
                                 RCC_CLOCKTYPE_PCLK2  | RCC_CLOCKTYPE_D3PCLK1);

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;  
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2; 
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2; 
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2; 
    ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);
    if(ret != HAL_OK)
        clock_fault();

    /*
    Note : The activation of the I/O Compensation Cell is recommended with communication  interfaces
          (GPIO, SPI, FMC, QSPI ...)  when  operating at  high frequencies(please refer to product datasheet)       
          The I/O Compensation Cell activation  procedure requires :
        - The activation of the CSI clock
        - The activation of the SYSCFG clock
        - Enabling the I/O Compensation Cell : setting bit[0] of register SYSCFG_CCCSR
    */

    /*activate CSI clock mondatory for I/O Compensation Cell*/  
    __HAL_RCC_CSI_ENABLE() ;

    /* Enable SYSCFG clock mondatory for I/O Compensation Cell */
    __HAL_RCC_SYSCFG_CLK_ENABLE() ;

    /* Enables the I/O Compensation Cell */
    HAL_EnableCompensationCell();
}

void CM4_LoadCode (void)
{
    extern const char *cm4_code; /* section containing CM4 Code */
    extern const char *cm4_code_end; /* section containing CM4 Code */

    /* Copy CM4 code from Flash to D2_SRAM memory */
    d_memcpy((void *)D2_AXISRAM_BASE,  &cm4_code, (char *)&cm4_code_end - (char *)&cm4_code);
    
    /* Configure the boot address for CPU2 (Cortex-M4) */
    HAL_SYSCFG_CM4BootAddConfig(SYSCFG_BOOT_ADDR0, D2_AXISRAM_BASE);
    
    /* Enable CPU2 (Cortex-M4) boot regardless of option byte values */
    HAL_RCCEx_EnableBootCore(RCC_BOOT_C2);
}

#else
#error
#endif

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

static void dev_hal_gpio_init (void)
{
#if defined(USE_STM32F769I_DISCO)
    BSP_LED_Init(LED1);
    BSP_LED_Init(LED2);
#elif defined(USE_STM32H747I_DISCO)
    BSP_LED_Init(LED1);
    BSP_LED_Init(LED2);
    BSP_LED_Init(LED3);
    BSP_LED_Init(LED4);
#endif
}

int dev_hal_preinit (void)
{
    CPU_CACHE_Enable();
    HAL_Init();
    SystemClock_Config();
    mpu_init();
#if defined(USE_STM32H745I_DISCO) || defined(USE_STM32H747I_DISCO)
    hal_smp_init(0);
    hal_smp_hsem_alloc("hsem_task");
#endif
    dev_hal_gpio_init();
    return 0;
}

int dev_hal_init (void)
{
    dev_hal_preinit();
    heap_init();
    hal_tty_vcom_attach();
    CM4_LoadCode();
    return 0;
}

int dev_hal_deinit (void)
{
#if defined(USE_STM32H745I_DISCO) || defined(USE_STM32H747I_DISCO)
    BSP_I2C4_DeInit();
    hal_smp_deinit();
#endif
    return 0;
}

void dev_hal_tickle (void)
{
    HAL_IncTick();
}

#if defined(STM32H747xx)
#include "stm32h747i_discovery_ts.h"
#elif defined(STM32H745xx)
#include "stm32h745i_discovery_ts.h"
#elif defined(STM32F769xx)
#include "stm32f769i_discovery_ts.h"
#else
#error
#endif

static uint8_t ts_prev_state = TS_IDLE;
static uint8_t ts_state_cooldown_cnt = 0;
static uint8_t ts_states_map[4][2];

static void input_ts_sm_init (void)
{
    ts_states_map[TS_IDLE][TS_PRESS_ON]         = TS_CLICK;
    ts_states_map[TS_IDLE][TS_PRESS_OFF]        = TS_IDLE;
    ts_states_map[TS_CLICK][TS_PRESS_ON]        = TS_PRESSED;
    ts_states_map[TS_CLICK][TS_PRESS_OFF]       = TS_RELEASED;
    ts_states_map[TS_PRESSED][TS_PRESS_ON]      = TS_PRESSED;
    ts_states_map[TS_PRESSED][TS_PRESS_OFF]     = TS_RELEASED;
    ts_states_map[TS_RELEASED][TS_PRESS_ON]     = TS_IDLE;
    ts_states_map[TS_RELEASED][TS_PRESS_OFF]    = TS_IDLE;
}

int input_hal_init (void)
{
    int err = CMDERR_OK;
    screen_t screen;

    vid_wh(&screen);
#if defined(STM32H745xx) || defined(STM32H747xx)
    {
        TS_Init_t ts;
        ts.Accuracy = 0;
        ts.Orientation = 1;
        ts.Width = screen.width;
        ts.Height = screen.height;
        err = BSP_TS_Init(0, &ts);
    }
#elif defined(STM32F769xx)
    err = BSP_TS_Init(screen.width, screen.height);
#else
#error
#endif
    if (err != BSP_ERROR_NONE) {
        err = -CMDERR_NOCORE;
    } else {
        input_ts_sm_init();
    }
    return err;
}

void input_hal_deinit (void)
{
    dprintf("%s() :\n", __func__);
    if (input_is_touch_avail()) {
#if defined(STM32H745xx) || defined(STM32H747xx)
        BSP_TS_DeInit(0);
#elif defined(STM32F769xx)
        BSP_TS_DeInit();
#else
#error
#endif
    }
}

void input_hal_read_ts (ts_status_t *ts_status)
{
    uint8_t state = 0;
    uint32_t x, y, td;
#if defined(STM32H745xx) || defined(STM32H747xx)
    TS_State_t TS_State;
    if (BSP_TS_GetState(0, &TS_State) != BSP_ERROR_NONE) {
        fatal_error("BSP_TS_GetState != TS_OK\n");
    }
    x = TS_State.TouchX;
    y = TS_State.TouchY;
    td = TS_State.TouchDetected;
#elif defined(STM32F769xx)
    TS_StateTypeDef  TS_State;
    if (BSP_TS_GetState(&TS_State) != TS_OK) {
        fatal_error("BSP_TS_GetState != TS_OK\n");
    }
    x = TS_State.touchX[0];
    y = TS_State.touchY[0];
    td = TS_State.touchDetected;
#else
#error
#endif

    ts_status->status = TOUCH_IDLE;
    state = ts_states_map[ts_prev_state][td ? TS_PRESS_ON : TS_PRESS_OFF];
    switch (state) {
        case TS_IDLE:
            if (ts_state_cooldown_cnt) {
                ts_state_cooldown_cnt--;
                return;
            }
            break;
        case TS_PRESSED:
            break;
        case TS_CLICK:
            ts_status->status = TOUCH_PRESSED;
            ts_status->x = x;
            ts_status->y = y;
            break;
        case TS_RELEASED:
            ts_status->status = TOUCH_RELEASED;
            ts_state_cooldown_cnt = 1;
            break;
        default:
            break;
    };
    ts_prev_state = state;
}

#if defined(STM32H745xx) || defined(STM32H747xx)

void CM4_SEV_IRQHandler (void)
{

}

#endif

