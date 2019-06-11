#include "string.h"
#include "stm32f769i_discovery_audio.h"
#include "audio_main.h"
#include "audio_int.h"
#include "wm8994.h"
#include "nvic.h"
#include "debug.h"
#include <dev_conf.h>

#if AUDIO_MODULE_PRESENT

A_COMPILE_TIME_ASSERT(samplerate,
    (AUDIO_SAMPLE_RATE == I2S_AUDIOFREQ_8K) ||
    (AUDIO_SAMPLE_RATE == I2S_AUDIOFREQ_11K) ||
    (AUDIO_SAMPLE_RATE == I2S_AUDIOFREQ_16K) ||
    (AUDIO_SAMPLE_RATE == I2S_AUDIOFREQ_22K) ||
    (AUDIO_SAMPLE_RATE == I2S_AUDIOFREQ_44K) ||
    (AUDIO_SAMPLE_RATE == I2S_AUDIOFREQ_48K) ||
    (AUDIO_SAMPLE_RATE == I2S_AUDIOFREQ_96K) ||
    (AUDIO_SAMPLE_RATE == I2S_AUDIOFREQ_192K));


#define CHANNEL_NUM_VALID(num) \
    (((num) < AUDIO_MAX_VOICES) || ((num) == AUDIO_MUS_CHAN_START))

static isr_status_e isr_status = A_ISR_NONE;
static int isr_pending[A_ISR_MAX] = {0};

static a_channel_t channels[AUDIO_MUS_CHAN_START + 1/*Music channel*/];
static a_channel_head_t chan_llist_ready;

static d_bool a_force_stop        = d_false;
static irqmask_t audio_irq_mask;

static void (*a_mixer_callback) (int chan, void *stream, int len, void *udata) = NULL;

d_bool g_audio_proc_isr = d_true;

void error_handle (void)
{
    for (;;) {}
}

static inline void
a_chan_reset (a_channel_t *desc)
{
    memset(desc, 0, sizeof(*desc));
}

static void a_chanlist_empty_clbk (struct a_channel_head_s *head)
{
    if (cd_playing())
        return;

    a_force_stop = d_true;

}

static void a_chanlist_first_node_clbk (struct a_channel_head_s *head)
{
    if (cd_playing())
        return;
}

void a_chanlist_node_remove_clbk (struct a_channel_head_s *head, a_channel_t *rem)
{
    a_chan_reset(rem);
}

static audio_channel_t *
a_channel_insert (Mix_Chunk *chunk, int channel)
{
    a_channel_t *desc = &channels[channel];
    if (desc->inst.is_playing == 0) {

        desc->inst.is_playing = 1;
        desc->inst.chunk = *chunk;
        a_channel_link(&chan_llist_ready, desc, 0);
    } else {
        error_handle();
    }

    desc->inst.chunk         = *chunk;
    a_chunk_len(desc) = AUDIO_BYTES_2_SAMPLES(a_chunk_len(desc));
    desc->inst.id            = channel;

    desc->loopsize =     a_chunk_len(desc);
    desc->bufposition =  a_chunk_data(desc);
    desc->volume =       a_chunk_vol(desc);
    desc->priority = 0;
    
    return &desc->inst;
}

void
a_channel_remove (a_channel_t *desc)
{
    a_channel_unlink(&chan_llist_ready, desc);
}

static void a_shutdown (void)
{
    BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
}

static void
a_paint_buff_helper (a_buf_t *abuf)
{
    int compratio = chan_llist_ready.size + 2;
    d_bool mixduty = d_false;

    a_clear_abuf(abuf);
    if (a_mixer_callback) {
        a_mixer_callback(-1, abuf->buf, abuf->samples * sizeof(abuf->buf[0]), NULL);
        mixduty = d_true;
    }
    if (a_chanlist_try_reject_all(&chan_llist_ready) == 0) {
        if (!mixduty) {
            a_clear_abuf(abuf);
        }
        return;
    }
    *abuf->durty = mixduty;
    a_paint_buffer(&chan_llist_ready, abuf, compratio);
}

static void a_paint_all (d_bool force)
{
    a_buf_t master;
    int id, bufidx = 0;

    for (id = A_ISR_HALF; id < A_ISR_MAX; id++) {
        if (isr_pending[id] || force) {
            a_get_master4idx(&master, bufidx);
            a_paint_buff_helper(&master);
        }
        bufidx++;
    }
}

static void
DMA_on_tx_complete_isr (isr_status_e status)
{
    a_buf_t master;
    isr_status = status;
    a_get_master4idx(&master, status == A_ISR_HALF ? 0 : 1);
    a_paint_buff_helper(&master);
}

static void
DMA_on_tx_complete_dsr (isr_status_e status)
{
    isr_status = status;
    isr_pending[status]++;
    if (chan_llist_ready.size > 0) {
        if ((isr_pending[status] & 0xff) == 0xff) {
            dprintf("audio_main.c, DMA_on_tx_complete : isr_pending[ %s ]= %d\n",
                    status == A_ISR_HALF ? "A_ISR_HALF" :
                    status == A_ISR_COMP ? "A_ISR_COMP" : "?", isr_pending[status]);
        } else if (isr_pending[status] == 2) {
            a_clear_master();
        }
    }
}

static void
DMA_on_tx_complete (isr_status_e status)
{
    if (g_audio_proc_isr) {
        DMA_on_tx_complete_isr(status);
    } else {
        DMA_on_tx_complete_dsr(status);
    }
}

static void AUDIO_InitApplication(void)
{
  a_buf_t master;
  irqmask_t irq_flags;
  irq_bmap(&irq_flags);
  BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, A_NITIAL_VOL, AUDIO_SAMPLE_RATE);
  BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);

  a_get_master_base(&master);
  BSP_AUDIO_OUT_Play((uint16_t *)master.buf, AUDIO_SAMPLES_2_BYTES(master.samples));
  irq_bmap(&audio_irq_mask);
  audio_irq_mask = audio_irq_mask & (~irq_flags);
}

static void AUDIO_DeInitApplication(void)
{
  BSP_AUDIO_OUT_DeInit();
}

void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{
    DMA_on_tx_complete(A_ISR_HALF);
}

void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
{
    DMA_on_tx_complete(A_ISR_COMP);
}

void BSP_AUDIO_OUT_Error_CallBack(void)
{
    a_shutdown();
    error_handle();
}

static void a_isr_clear_all (void)
{
    isr_status = A_ISR_NONE;
    isr_pending[A_ISR_HALF] = 0;
    isr_pending[A_ISR_COMP] = 0;
}


void audio_update_isr (void)
{
    irqmask_t irq_flags = audio_irq_mask;

    irq_save(&irq_flags);
    if (a_force_stop) {
        a_clear_master();
        a_force_stop = d_false;
    }
    music_tickle();
    irq_restore(irq_flags);
}

void audio_update_dsr (void)
{
    irqmask_t irq_flags = audio_irq_mask;

    irq_save(&irq_flags);
    if (isr_status) {
        a_paint_all(d_false);
    }
    a_isr_clear_all();
    irq_restore(irq_flags);

    if (a_force_stop) {
        a_clear_master();
        a_force_stop = d_false;
    }
    music_tickle();
}

void audio_update (void)
{
    if (g_audio_proc_isr) {
        audio_update_isr();
    } else {
        audio_update_dsr();
    }
}

void audio_irq_save (irqmask_t *flags)
{
    *flags = audio_irq_mask;
    irq_save(flags);
}

void audio_irq_restore (irqmask_t flags)
{
    irq_restore(flags);
}

void audio_init (void)
{
    memset(&chan_llist_ready, 0, sizeof(chan_llist_ready));
    memset(channels, 0, sizeof(channels));
    chan_llist_ready.empty_handle = a_chanlist_empty_clbk;
    chan_llist_ready.first_link_handle = a_chanlist_first_node_clbk;
    chan_llist_ready.remove_handle = a_chanlist_node_remove_clbk;
    a_mem_init();
    AUDIO_InitApplication();
#if (USE_REVERB)
    a_rev_init();
#endif
    cd_init();
}

void audio_deinit (void)
{
    dprintf("%s() :\n", __func__);
    AUDIO_DeInitApplication();
    audio_stop_all();
    cd_init();
}

audio_channel_t *audio_play_channel (Mix_Chunk *chunk, int channel)
{
    irqmask_t irq_flags = audio_irq_mask;
    audio_channel_t *ch = NULL;

    irq_save(&irq_flags);
    if (!CHANNEL_NUM_VALID(channel)) {
        irq_restore(irq_flags);
        return NULL;
    }
    ch = a_channel_insert(chunk, channel);
    irq_restore(irq_flags);
    return ch;
}

void audio_stop_all (void)
{
    a_channel_t *cur, *next;
    irqmask_t irq_flags = audio_irq_mask;
    irq_save(&irq_flags);
    a_chan_foreach_safe(&chan_llist_ready, cur, next) {
        a_channel_remove(cur);
    }
    irq_restore(irq_flags);
}

void audio_mixer_ext (void (*mixer_callback) (int, void *, int, void *))
{
    irqmask_t irq_flags = audio_irq_mask;
    irq_save(&irq_flags);
    a_mixer_callback = mixer_callback;
    irq_restore(irq_flags);
}

audio_channel_t *audio_stop_channel (int channel)
{
    irqmask_t irq_flags = audio_irq_mask;
    irq_save(&irq_flags);
    audio_pause(channel);
    irq_restore(irq_flags);
    return NULL;
}

void audio_pause (int channel)
{
    irqmask_t irq_flags = audio_irq_mask;
    irq_save(&irq_flags);

    if (!CHANNEL_NUM_VALID(channel)) {
        irq_restore(irq_flags);
        return;
    }
    if (a_chn_play(&channels[channel])) {
        a_channel_remove(&channels[channel]);
    }
    irq_restore(irq_flags);
}

void audio_resume (void)
{

}

int audio_is_playing (int handle)
{
    if (!CHANNEL_NUM_VALID(handle)) {
        return 0;
    }
    return a_chn_play(&channels[handle]);
}

int audio_chk_priority (int priority)
{
    a_channel_t *cur, *next;
    int id = 0;
    irqmask_t irq_flags = audio_irq_mask;
    irq_save(&irq_flags);

    a_chan_foreach_safe(&chan_llist_ready, cur, next) {

        if (!priority || cur->priority >= priority) {
            irq_restore(irq_flags);
            return id;
        }
        id++;
    }
    irq_restore(irq_flags);
    return -1;
}


void audio_set_pan (int handle, int l, int r)
{
    irqmask_t irq_flags = audio_irq_mask;
    irq_save(&irq_flags);

    if (!CHANNEL_NUM_VALID(handle)) {
        irq_restore(irq_flags);
        return;
    }
    if (a_chn_play(&channels[handle])) {
#if USE_STEREO
        a_chunk_vol(&channels[handle]) = ((l + r)) & MAX_VOL;
        channels[handle].left = (uint8_t)l << 1;
        channels[handle].right = (uint8_t)r << 1;
        channels[handle].volume = a_chunk_vol(&channels[handle]);
#else
        a_chunk_vol(&channels[handle]) = ((l + r)) & MAX_VOL;
        channels[handle].volume = a_chunk_vol(&channels[handle]);
#endif
    }
    irq_restore(irq_flags);
}

void audio_change_sample_volume (audio_channel_t *achannel, uint8_t volume)
{
    irqmask_t irq_flags = audio_irq_mask;

    irq_save(&irq_flags);
    a_channel_t *desc = container_of(achannel, a_channel_t, inst);
    desc->volume = volume & MAX_VOL;
    irq_restore(irq_flags);
}

#else /*AUDIO_MODULE_PRESENT*/

void audio_init (void)
{

}

void audio_deinit (void)
{

}

void audio_mixer_ext (void (*mixer_callback) (int, void *, int, void *))
{
    a_mixer_callback = mixer_callback;
}

audio_channel_t *
audio_play_channel (Mix_Chunk *chunk, int channel)
{
    return NULL;
}

void audio_stop_all (void)
{

}

audio_channel_t *
audio_stop_channel (int channel)
{
    return NULL;
}

void
audio_pause (int channel)
{

}

int
audio_is_playing (int handle)
{
    return 0;
}

int
audio_chk_priority (int handle)
{
    
}


void
audio_set_pan (int handle, int l, int r)
{

}

void
audio_update (void)
{

}

#endif /*AUDIO_MODULE_PRESENT*/
