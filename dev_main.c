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
#include <bsp_api.h>

#if (_USE_LFN == 3)
#error "ff_malloc, ff_free must be redefined to Sys_HeapAlloc"
#endif

#ifdef STM32F7
int const __cache_line_size = 32;
#else
#error "Cache line size unknown"
#endif

bspapi_t bspapi;

bspapi_t *g_bspapi;

static void bsp_api_attach (bspapi_t *api);

extern void VID_PreConfig (void);

/** The prototype for the application's main() function */
extern int mainloop (int argc, const char *argv[]);

#if !BSP_INDIR_API

int g_dev_debug_level = DBG_ERR;

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

    serial_flush();
    bug();
}

static void clock_fault (void)
{
    bug();
}


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

static int con_echo (const char *buf, int len)
{
    dprintf("@: %s\n", buf);
    return 0; /*let it be processed by others*/
}

#endif

void dev_tickle (void)
{
    audio_update();
    input_tickle();
    serial_tickle();
    profiler_reset();
}

int dev_main (void)
{
#if !BSP_INDIR_API
    CPU_CACHE_Enable();
    SystemClock_Config();
    HAL_Init();
    mpu_init();
    serial_init();
    Sys_AllocInit();
    profiler_init();

    BSP_LED_Init(LED1);
    BSP_LED_Init(LED2);

    term_register_handler(con_echo);

    audio_init();
    input_bsp_init();
    dev_io_init();
    screen_init();
    SystemDump();
    d_dvar_int32(&g_dev_debug_level, "dbglvl");
#endif

    VID_PreConfig();
    bsp_api_attach(&bspapi);
    mainloop(0, NULL);

    return 0;
}

void dev_deinit (void)
{
#if BSP_INDIR_API
    irqmask_t irq = NVIC_IRQ_MASK;
    dprintf("%s() :\n", __func__);
    term_unregister_handler(con_echo);
    input_bsp_deinit();
    audio_deinit();
    dev_io_deinit();
    screen_deinit();
    profiler_deinit();
    Sys_AllocDeInit();
    serial_deinit();
    irq_save(&irq);
    HAL_RCC_DeInit();
    HAL_DeInit();
#else
extern void screen_release (void);
    screen_release();
#endif
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

#if !BSP_INDIR_API

static void bsp_api_attach (bspapi_t *api)
{
    arch_word_t *ptr, size;

    arch_get_shared(&ptr, &size);
    assert(ptr);
    assert(size >= sizeof(*api));

    g_bspapi = api;

    api->io.io_init = dev_io_init;
    api->io.io_deinit = dev_io_deinit;
    api->io.open = d_open;
    api->io.size = d_size;
    api->io.tell = d_tell;
    api->io.close = d_close;
    api->io.unlink = d_unlink;
    api->io.seek = d_seek;
    api->io.eof = d_eof;
    api->io.read = d_read;
    api->io.gets = d_gets;
    api->io.getc = d_getc;
    api->io.write = d_write;
    api->io.printf = d_printf;
    api->io.mkdir = d_mkdir;
    api->io.opendir = d_opendir;
    api->io.closedir = d_closedir;
    api->io.readdir = d_readdir;
    api->io.time = d_time;
    api->io.dirlist = d_dirlist;
    api->io.var_reg = d_dvar_reg;
    api->io.var_int32 = d_dvar_int32;
    api->io.var_float = d_dvar_float;
    api->io.var_str = d_dvar_str;

    api->vid.init = screen_init;
    api->vid.deinit = screen_deinit;
    api->vid.get_wh = screen_get_wh;
    api->vid.mem_avail = screen_total_mem_avail_kb;
    api->vid.win_cfg = screen_win_cfg;
    api->vid.set_clut = screen_set_clut;
    api->vid.update = screen_update;
    api->vid.direct = screen_direct;
    api->vid.vsync = screen_vsync;
    api->vid.input_align = screen_ts_align;

    api->snd.init = audio_init;
    api->snd.deinit = audio_deinit;
    api->snd.mixer_ext = audio_mixer_ext;
    api->snd.play = audio_play_channel;
    api->snd.stop = audio_stop_channel;
    api->snd.pause = audio_pause;
    api->snd.stop_all = audio_stop_all;
    api->snd.is_play = audio_is_playing;
    api->snd.check = audio_chk_priority;
    api->snd.set_pan = audio_set_pan;
    api->snd.sample_volume = audio_change_sample_volume;
    api->snd.tickle = audio_update;
    api->snd.irq_save = audio_irq_save;
    api->snd.irq_restore = audio_irq_restore;

    api->snd.wave_open = audio_open_wave;
    api->snd.wave_size = audio_wave_size;
    api->snd.wave_cache = audio_cache_wave;
    api->snd.wave_close = audio_wave_close;

    api->cd.play = cd_play_name;
    api->cd.pause = cd_pause;
    api->cd.resume = cd_resume;
    api->cd.stop = cd_stop;
    api->cd.volume = cd_volume;
    api->cd.getvol = cd_getvol;
    api->cd.playing = cd_playing;

    api->sys.fatal = fatal_error;
    api->sys.alloc_init = Sys_AllocInit;
    api->sys.alloc_shared = Sys_AllocShared;
    api->sys.alloc_vid = Sys_AllocVideo;
    api->sys.avail = Sys_AllocBytesLeft;
    api->sys.malloc = Sys_Malloc;
    api->sys.realloc = Sys_Realloc;
    api->sys.calloc = Sys_Calloc;
    api->sys.free = Sys_Free;
    api->sys.prof_enter = _profiler_enter;
    api->sys.prof_exit = _profiler_exit;
    api->sys.prof_reset = profiler_reset;
    api->sys.prof_init = profiler_init;

    api->dbg.init = serial_init;
    api->dbg.putc = serial_putc;
    api->dbg.getc = serial_getc;
    api->dbg.send = serial_send_buf;
    api->dbg.flush = serial_flush;
    api->dbg.reg_clbk = term_register_handler;
    api->dbg.unreg_clbk = term_unregister_handler;
    api->dbg.tickle = serial_tickle;

    d_memcpy(ptr, api, sizeof(*api));
}

#else

static void bsp_api_attach (bspapi_t *api)
{
    arch_word_t *ptr, size;

    arch_get_shared(&ptr, &size);
    assert(ptr);
    assert(size >= sizeof(*api));

    d_memcpy(api, ptr, sizeof(*api));
    g_bspapi = api;
}

#endif /*BSP_INDIR_API*/


