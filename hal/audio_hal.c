#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if defined(USE_STM32H747I_DISCO)
#include <stm32h747i_discovery_audio.h>
#elif defined(USE_STM32H745I_DISCO)
#include <stm32h745i_discovery_audio.h>
#elif defined(USE_STM32F769I_DISCO)
#include <stm32f769i_discovery_audio.h>
#include <wm8994.h>
#else
#error
#endif

#include <config.h>
#include <nvic.h>
#include <bsp_api.h>
#include <debug.h>
#include <heap.h>
#include <misc_utils.h>
#include <audio_main.h>
#include <audio_int.h>
#include <bsp_cmd.h>
#include <smp.h>

#if AUDIO_MODULE_PRESENT

extern audio_t *audio;
extern SAI_HandleTypeDef haudio_out_sai;

isr_status_e g_audio_isr_status = A_ISR_NONE;
int g_audio_isr_pend[A_ISR_MAX] = {0};

d_bool g_audio_proc_isr = d_true;

static void
__DMA_on_tx_complete_isr (audio_t *audio, isr_status_e status)
{
    a_buf_t master;
    g_audio_isr_status = status;
    a_get_master4idx(&master, status == A_ISR_HALF ? 0 : 1);
    a_paint_buff_helper(audio, &master);
}

static void
__DMA_on_tx_complete_dsr (audio_t *audio, isr_status_e status)
{
    g_audio_isr_status = status;
    g_audio_isr_pend[status]++;
    a_dsr_hung_fuse(audio, status);
}

static void
__DMA_on_tx_complete (audio_t *audio, isr_status_e status)
{
    if (g_audio_proc_isr) {
        __DMA_on_tx_complete_isr(audio, status);
    } else {
        __DMA_on_tx_complete_dsr(audio, status);
    }
}

typedef struct {
    mixdata_t mixdata[AUDIO_MUS_CHAN_START + 1];
    a_buf_t abuf;
    int compratio;
    int mixcnt;
} a_smp_task_arg_t;

static hal_smp_task_t *task = NULL;

static IRAMFUNC void __a_paint_buf_ex_smp_task (void *_arg);

static IRAMFUNC void __a_paint_buf_ex_smp_task (void *_arg)
{
    a_smp_task_arg_t *arg = (a_smp_task_arg_t *)_arg;
    int i;

    a_clear_abuf(&arg->abuf);
    for (i = 0; i < arg->mixcnt; i++) {
        if (arg->mixdata[i].size) {
            a_mix_single_to_master(arg->abuf.buf, &arg->mixdata[i], arg->compratio);
        }
    }
    *arg->abuf.dirty = d_true;
}

void a_paint_buf_ex_smp_task (a_buf_t *abuf, mixdata_t *mixdata, int mixcnt, int compratio)
{
    int hsem;
    a_smp_task_arg_t arg;

    arg.abuf = *abuf;
    arg.compratio = compratio;
    arg.mixcnt = mixcnt;
    d_memcpy(arg.mixdata, mixdata, mixcnt * sizeof(mixdata[0]));

    if (task) {
        hal_smp_sync_task(task);
        hal_smp_free_task(task);
    }
    hsem = hal_smp_hsem_alloc("hsem_task");
    assert(hsem >= 0);
    hal_smp_hsem_spinlock(hsem);
    task = hal_smp_sched_task(__a_paint_buf_ex_smp_task, &arg, sizeof(arg));
    hal_smp_hsem_release(hsem);
}

void
a_hal_configure (audio_t *audio)
{
    uint8_t ret;
    a_buf_t master;
    irqmask_t irq_flags;
    a_intcfg_t *cfg = &audio->config;

    irq_bmap(&irq_flags);

    a_get_master_base(&master);
#if defined(USE_STM32F769I_DISCO)
    ret = BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, cfg->volume, cfg->samplerate);
    if (ret) {
        dprintf("%s() : Init failed!\n", __func__);
        return;
    }
    ret = BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);
    if (ret) {
        dprintf("%s() : Set frame slot failed!\n", __func__);
        return;
    }
    ret = BSP_AUDIO_OUT_Play((uint16_t *)master.buf, AUDIO_SAMPLES_2_BYTES(master.samples));
    if (ret) {
        dprintf("%s() : Play failed!\n", __func__);
        return;
    }
#elif defined(USE_STM32H745I_DISCO) || defined(USE_STM32H747I_DISCO)
    BSP_AUDIO_Init_t init;

    init.BitsPerSample = cfg->samplebits;
    init.ChannelsNbr = cfg->channels;
    init.Device = WM8994_OUT_HEADPHONE;
    init.SampleRate = cfg->samplerate;
    init.Volume = cfg->volume;
    ret = BSP_AUDIO_OUT_Init(0, &init);
    if (ret) {
        dprintf("%s() : Init failed!\n", __func__);
        return;
    }
    BSP_AUDIO_OUT_Play(0, (uint8_t *)master.buf, AUDIO_SAMPLES_2_BYTES(master.samples));
    if (ret) {
        dprintf("%s() : Play failed!\n", __func__);
        return;
    }
#else
#error
#endif
  irq_bmap(&cfg->irq);
  cfg->irq = cfg->irq & (~irq_flags);

  audio->cvar_have_smp = 0;
  audio->irq_mask = audio->config.irq;
  cmd_register_i32(&audio->cvar_have_smp, "a_have_smp");
}

void a_hal_shutdown (void)
{
    BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
}

#if defined(USE_STM32F769I_DISCO)

void a_hal_deinit(void)
{
  BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
  BSP_AUDIO_OUT_DeInit();
}

void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{
    __DMA_on_tx_complete(audio, A_ISR_HALF);
}

void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
{
    __DMA_on_tx_complete(audio, A_ISR_COMP);
}

void BSP_AUDIO_OUT_Error_CallBack(void)
{
    a_hal_shutdown();
    error_handle();
}

#elif defined(USE_STM32H745I_DISCO) || defined(USE_STM32H747I_DISCO)

void a_hal_deinit(void)
{
    BSP_AUDIO_OUT_Stop(0);
    BSP_AUDIO_OUT_DeInit(0);
}

void BSP_AUDIO_OUT_HalfTransfer_CallBack(uint32_t inst)
{
    __DMA_on_tx_complete(audio, A_ISR_HALF);
}

void BSP_AUDIO_OUT_TransferComplete_CallBack(uint32_t inst)
{
    __DMA_on_tx_complete(audio, A_ISR_COMP);
}

void BSP_AUDIO_OUT_Error_CallBack(uint32_t inst)
{
    a_hal_shutdown();
}

#else
#error
#endif

void AUDIO_OUT_SAIx_DMAx_IRQHandler(void)
{
  HAL_DMA_IRQHandler(haudio_out_sai.hdmatx);
}

void a_hal_check_cfg (a_intcfg_t *cfg)
{
    
    if((cfg->samplerate == I2S_AUDIOFREQ_8K) ||
        (cfg->samplerate == I2S_AUDIOFREQ_11K) ||
        (cfg->samplerate == I2S_AUDIOFREQ_16K) ||
        (cfg->samplerate == I2S_AUDIOFREQ_22K) ||
        (cfg->samplerate == I2S_AUDIOFREQ_44K) ||
        (cfg->samplerate == I2S_AUDIOFREQ_48K) ||
        (cfg->samplerate == I2S_AUDIOFREQ_96K) ||
        (cfg->samplerate == I2S_AUDIOFREQ_192K)) {

    } else {
        dprintf("%s() : incompat. samplerate= %u\n", __func__, cfg->samplerate);
        cfg->samplerate = AUDIO_RATE_DEFAULT;
    }
    if (cfg->channels != AUDIO_CHANNELS_NUM_DEFAULT) {
        dprintf("%s() : incompat. channels num= %u\n", __func__, cfg->channels);
        cfg->channels = AUDIO_CHANNELS_NUM_DEFAULT;
    }
    if (cfg->samplebits != AUDIO_SAMPLEBITS_DEFAULT) {
        dprintf("%s() : incompat. bits per sample= %u\n", __func__, cfg->samplebits);
        cfg->samplebits = AUDIO_SAMPLEBITS_DEFAULT;
    }
    cfg->volume = cfg->volume & MAX_VOL;
}

#endif /*AUDIO_MODULE_PRESENT*/
