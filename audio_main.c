#include "string.h"
#include "stm32f769i_discovery_audio.h"
#include "audio_main.h"
#include "audio_int.h"
#include "wm8994.h"
#include "nvic.h"
#include "debug.h"

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
    (((num) < AUDIO_MAX_CHANS) || ((num) == AUDIO_MUS_CHAN_START))

static isr_status_e isr_status = A_ISR_NONE;
static int isr_pending[A_ISR_MAX] = {0};

static a_channel_t channels[AUDIO_MUS_CHAN_START + 1/*Music channel*/];
static a_channel_head_t chan_llist_ready;

static boolean a_force_stop        = false;
static irqmask_t audio_irq_mask;

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

    a_force_stop = true;

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
    
    return &desc->inst;
}

void
a_channel_remove (a_channel_t *desc)
{
    a_channel_unlink(&chan_llist_ready, desc);
}

static void
a_paint_buff_helper (a_buf_t *abuf)
{
    int compratio = chan_llist_ready.size + 2;

    a_clear_abuf(abuf);
    if (a_chanlist_try_reject_all(&chan_llist_ready) == 0) {
        a_clear_abuf(abuf);
        return;
    }
    a_paint_buffer(&chan_llist_ready, abuf, compratio);
}

static void a_shutdown (void)
{
    a_channel_t *cur, *next;
    BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
    a_chan_foreach_safe(&chan_llist_ready, cur, next) {
        a_channel_remove(cur);
    }
}

static void
DMA_on_tx_complete (isr_status_e status)
{
    isr_status = status;
    isr_pending[status]++;
    if (chan_llist_ready.size > 0) {
        if (isr_pending[status] > 100) {
            dprintf("audio_main.c, DMA_on_tx_complete : isr_pending[ %s ]= %d\n",
                    status == A_ISR_HALF ? "A_ISR_HALF" :
                    status == A_ISR_COMP ? "A_ISR_COMP" : "?", isr_pending[status]);
        } else if (isr_pending[status] == 2) {
            a_clear_master();
        }
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

static void a_paint_all (boolean force)
{
    a_buf_t master;

    if (isr_pending[A_ISR_HALF] || force) {
        a_get_master4idx(&master, 0);
        a_paint_buff_helper(&master);
    }
    if (isr_pending[A_ISR_COMP] || force) {
        a_get_master4idx(&master, 1);
        a_paint_buff_helper(&master);
    }
}

static void a_isr_clear_all (void)
{
    isr_status = A_ISR_NONE;
    isr_pending[A_ISR_HALF] = 0;
    isr_pending[A_ISR_COMP] = 0;
}

void audio_update (void)
{
    irqmask_t irq_flags;

    irq_save_mask(&irq_flags, ~audio_irq_mask);
    if (isr_status) {
        a_paint_all(false);
    }
    a_isr_clear_all();
    irq_restore(irq_flags);

    if (a_force_stop) {
        a_clear_master();
        a_force_stop = false;
    }
    music_tickle();
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

audio_channel_t *audio_play_channel (Mix_Chunk *chunk, int channel)
{
    audio_channel_t *ch = NULL;
    if (!CHANNEL_NUM_VALID(channel)) {
        return NULL;
    }
    ch = a_channel_insert(chunk, channel);
    return ch;
}

audio_channel_t *audio_stop_channel (int channel)
{
    audio_pause(channel);
    return NULL;
}

void audio_pause (int channel)
{
    if (!CHANNEL_NUM_VALID(channel)) {
        return;
    }
    if (a_chn_play(&channels[channel])) {
        a_channel_remove(&channels[channel]);
    }
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
void audio_set_pan (int handle, int l, int r)
{
    if (!CHANNEL_NUM_VALID(handle)) {
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
}

void audio_change_sample_volume (audio_channel_t *achannel, uint8_t volume)
{
    a_channel_t *desc = container_of(achannel, a_channel_t, inst);
    desc->volume = volume & MAX_VOL;
}

#else /*AUDIO_MODULE_PRESENT*/

void audio_init (void)
{

}

audio_channel_t *
audio_play_channel (Mix_Chunk *chunk, int channel)
{
    return NULL;
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

void
audio_set_pan (int handle, int l, int r)
{

}

void
audio_update (void)
{

}

#endif /*AUDIO_MODULE_PRESENT*/
