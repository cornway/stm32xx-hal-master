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
#include <debug.h>

#include <audio_int.h>
#include <audio_main.h>

#if AUDIO_MODULE_PRESENT

extern SAI_HandleTypeDef haudio_out_sai;

isr_status_e g_audio_isr_status = A_ISR_NONE;
int g_audio_isr_pend[A_ISR_MAX] = {0};

d_bool g_audio_proc_isr = d_true;

static void
__DMA_on_tx_complete_isr (isr_status_e status)
{
    a_buf_t master;
    g_audio_isr_status = status;
    a_get_master4idx(&master, status == A_ISR_HALF ? 0 : 1);
    a_paint_buff_helper(&master);
}

static void
__DMA_on_tx_complete_dsr (isr_status_e status)
{
    g_audio_isr_status = status;
    g_audio_isr_pend[status]++;
    a_dsr_hung_fuse(status);
}

static void
__DMA_on_tx_complete (isr_status_e status)
{
    if (g_audio_proc_isr) {
        __DMA_on_tx_complete_isr(status);
    } else {
        __DMA_on_tx_complete_dsr(status);
    }
}

void
a_hal_configure (a_intcfg_t *cfg)
{
  a_buf_t master;
  irqmask_t irq_flags;
  irq_bmap(&irq_flags);

  a_get_master_base(&master);
#if defined(USE_STM32F769I_DISCO)
  BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, cfg->volume, cfg->samplerate);
  BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);
  BSP_AUDIO_OUT_Play((uint16_t *)master.buf, AUDIO_SAMPLES_2_BYTES(master.samples));
#elif defined(USE_STM32H745I_DISCO) || defined(USE_STM32H747I_DISCO)
  BSP_AUDIO_Init_t init;
    
  init.BitsPerSample = cfg->samplebits;
  init.ChannelsNbr = cfg->channels;
  init.Device = 0;
  init.SampleRate = cfg->samplerate;
  init.Volume = cfg->volume;
  BSP_AUDIO_OUT_Init(0, &init);
  BSP_AUDIO_OUT_Play(0, (uint8_t *)master.buf, AUDIO_SAMPLES_2_BYTES(master.samples));

#else
#error
#endif
  irq_bmap(&cfg->irq);
  cfg->irq = cfg->irq & (~irq_flags);
}

void a_hal_shutdown (void)
{
    BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
}

#if defined(USE_STM32F769I_DISCO)

void
a_hal_deinit(void)
{
  BSP_AUDIO_OUT_DeInit();
  BSP_AUDIO_OUT_DeInit();
}

void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{
    __DMA_on_tx_complete(A_ISR_HALF);
}

void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
{
    __DMA_on_tx_complete(A_ISR_COMP);
}

void BSP_AUDIO_OUT_Error_CallBack(void)
{
    a_hal_shutdown();
    error_handle();
}

#elif defined(USE_STM32H745I_DISCO) || defined(USE_STM32H747I_DISCO)

void
a_hal_deinit(void)
{
  BSP_AUDIO_OUT_DeInit(0);
  BSP_AUDIO_OUT_DeInit(0);
}

void BSP_AUDIO_OUT_HalfTransfer_CallBack(uint32_t inst)
{
    __DMA_on_tx_complete(A_ISR_HALF);
}

void BSP_AUDIO_OUT_TransferComplete_CallBack(uint32_t inst)
{
    __DMA_on_tx_complete(A_ISR_COMP);
}

void BSP_AUDIO_OUT_Error_CallBack(uint32_t inst)
{
    a_hal_shutdown();
    error_handle();
}

#else
#error
#endif

void AUDIO_OUT_SAIx_DMAx_IRQHandler(void)
{
  HAL_DMA_IRQHandler(haudio_out_sai.hdmatx);
}

void
a_hal_check_cfg (a_intcfg_t *cfg)
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
