#include "string.h"
#include "stm32f769i_discovery_audio.h"
#include "audio_main.h"
#include "audio_int.h"
#include "wm8994.h"

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


isr_status_e isr_status = A_ISR_NONE;
int isr_pending[A_ISR_MAX] = {0};

static a_channel_t channels[AUDIO_MUS_CHAN_START + 1/*Music channel*/];
a_channel_head_t chan_llist_ready;
a_channel_head_t chan_llist_susp;

static int8_t audio_need_update = 0;
static uint8_t audio_need_stop  = 0;
static uint32_t timeout         = 0;
static uint32_t play_enabled    = 0;
static uint8_t tr_state_changed = 1;
#if COMPRESSION
uint8_t comp_weight = 0;
#endif

static uint32_t audio_irq_saved;

static void a_paint_buffer (uint8_t idx);

void error_handle (void)
{
    for (;;) {}
}

void audio_irq_save (int *irq)
{
    if (audio_irq_saved != AUDIO_OUT_SAIx_DMAx_IRQ) {
        HAL_NVIC_DisableIRQ(AUDIO_OUT_SAIx_DMAx_IRQ);
        *irq = AUDIO_OUT_SAIx_DMAx_IRQ;
    } else {
        *irq = 0;
    }
    audio_irq_saved = AUDIO_OUT_SAIx_DMAx_IRQ;
}

void audio_irq_restore (int irq)
{
    if (irq) {
        HAL_NVIC_EnableIRQ(irq);
        audio_irq_saved = 0;
    }
}

static inline void
a_chan_reset (a_channel_t *desc)
{
    memset(desc, 0, sizeof(*desc));
}

static void a_chanlist_empty_clbk (struct a_channel_head_s *head)
{
    if (music_playing())
        return;

    audio_need_stop = 1;

}

static void a_chanlist_first_node_clbk (struct a_channel_head_s *head)
{
    if (music_playing())
        return;

    audio_need_update = 1;
}

void a_chanlist_node_remove_clbk (struct a_channel_head_s *head, a_channel_t *rem)
{
    a_chan_reset(rem);
}

static void
a_check_heartbeat ()
{
    if (chan_llist_ready.size) {
        if (tr_state_changed == 1) {
            tr_state_changed = 0;
            timeout = HAL_GetTick();
        }
        if (HAL_GetTick() - timeout >= AUDIO_TIMEOUT_MAX) {
            if (play_enabled)
                error_handle();
        }
    }
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
    desc->inst.chunk.alen    /= sizeof(snd_sample_t);/*len coming in bytes*/
    desc->inst.id            = channel;
    
    return &desc->inst;
}

static void
a_channel_remove (a_channel_t *desc)
{
    a_channel_unlink(&chan_llist_ready, desc);
}

static inline uint8_t
a_chan_try_reject (a_channel_t *desc)
{
    void **cache = chan_cache(desc);

    if (cache && (*cache == NULL)) {
        goto remove;
    }
    if (chan_len(desc) <= 0) {
        if (!chan_complete(desc) || chan_complete(desc)(2)) {
            goto remove;
        }
    }

    return 0;
remove :
    a_channel_remove(desc);
    return 1;
}

static inline uint8_t
a_chanlist_try_reject_all (void)
{
    chan_foreach(&chan_llist_ready, cur) {
        if (chan_is_play(cur))
            a_chan_try_reject(cur);
    }
    return 0;
}

static void
a_paint_buff_helper (uint8_t idx)
{
    tr_state_changed = 1;
    a_clear_master_idx(idx, 1);
    a_paint_buffer(idx);
}

static void a_shutdown (void)
{
    BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
    play_enabled = 0;
    chan_foreach(&chan_llist_ready, cur) {
        a_channel_remove(cur);
    }
}

static void
DMA_on_tx_complete (isr_status_e status)
{
    isr_status = status;
    isr_pending[status]++;
    if (isr_pending[status] > 100) {
        error_handle();
    } else if (isr_pending[status] > 5) {
        a_clear_master_all();
    }
}

static void AUDIO_InitApplication(void)
{
  BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_AUTO, 75, AUDIO_SAMPLE_RATE);
  BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);

  BSP_AUDIO_OUT_Play((uint16_t *)&audio_raw_buf[0][0], AUDIO_OUT_BUFFER_SIZE * 4);
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

void audio_update (void)
{
    int irq;
    audio_irq_save(&irq);
    if (isr_status) {
        if (isr_pending[A_ISR_HALF]) {
            a_paint_buff_helper(0);
            isr_pending[A_ISR_HALF] = 0;
        }
        if (isr_pending[A_ISR_COMP]) {
            a_paint_buff_helper(1);
            isr_pending[A_ISR_COMP] = 0;
        }
    }
    isr_status = A_ISR_NONE;
    audio_irq_restore(irq);

    a_check_heartbeat();
    if (audio_need_stop) {
        a_clear_master_all();
        audio_need_stop = 0;
    }
    
    if (audio_need_update == 0)
        goto exit;

    a_paint_buff_helper(0);
    a_paint_buff_helper(1);
    audio_need_update = 0;
exit:
    music_tickle();
}

static void a_paint_buf_ex (snd_sample_t *dest, uint8_t idx)
{
    int32_t size;
    snd_sample_t *ps;
    uint8_t vol;
    uint16_t mark_to_clear = 0;

    chan_foreach(&chan_llist_ready, cur) {
        if (chan_complete(cur) && chan_complete(cur)(1)) {
            mark_to_clear++;
            continue;
        }

        a_chanlist_move_window(cur, AUDIO_OUT_BUFFER_SIZE, &ps, &size);
        vol = chan_vol(cur);
        if (size) {
            a_mix_single_to_master(dest, &ps, &vol, size);
            mark_to_clear++;
        }
    }
#if (USE_REVERB)
    mark_to_clear += a_rev_proc(dest);
#endif
    if (mark_to_clear) {
        a_mark_master_idx(idx);
    }
}

#if (AUDIO_PLAY_SCHEME == 1)

static void a_paint_buffer (uint8_t idx)
{
    snd_sample_t *dest = audio_raw_buf[idx];
#if COMPRESSION
    comp_weight = chan_llist_ready.size + 2;
#endif
    /*TODO : fix this*/
    while (chan_llist_ready.size) {
        int32_t psize[AUDIO_MUS_CHAN_START + 1];
        uint16_t *ps[AUDIO_MUS_CHAN_START + 1];
        uint8_t  vol[AUDIO_MUS_CHAN_START + 1];

        a_mark_master_idx(idx);
        if (a_chanlist_try_reject_all() == 0) {
            break;
        }

        if (chan_len(chan_llist_ready.first) < AUDIO_OUT_BUFFER_SIZE) {
            a_paint_buf_ex(dest, idx);
        }

        a_chanlist_move_window_all(AUDIO_OUT_BUFFER_SIZE, ps, psize);
        chan_to_vol_arr(vol);

        chunk_proc_raw_all(dest,
                ps,
                vol,
                chan_llist_ready.size,
                AUDIO_OUT_BUFFER_SIZE);

        break;
    }
}

#else /*AUDIO_PLAY_SCHEME*/

static void a_paint_buffer (uint8_t idx)
{
    snd_sample_t *dest = audio_raw_buf[idx];
#if COMPRESSION
    comp_weight = chan_llist_ready.size + 2;
#endif
    a_chanlist_try_reject_all();
    a_paint_buf_ex(dest, idx);
}

#endif /*AUDIO_PLAY_SCHEME*/

void audio_init (void)
{
    memset(&chan_llist_ready, 0, sizeof(chan_llist_ready));
    chan_llist_ready.empty_handle = a_chanlist_empty_clbk;
    chan_llist_ready.first_link_handle = a_chanlist_first_node_clbk;
    chan_llist_ready.remove_handle = a_chanlist_node_remove_clbk;
    AUDIO_InitApplication();
#if (USE_REVERB)
    a_rev_init();
#endif
    play_enabled = 1;
    music_init();
}

audio_channel_t *audio_play_channel (Mix_Chunk *chunk, int channel)
{
    audio_channel_t *ch = NULL;
    if (channel >= AUDIO_MAX_CHANS &&
       channel != AUDIO_MUS_CHAN_START) {
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
    if (channel >= AUDIO_MAX_CHANS &&
       channel != AUDIO_MUS_CHAN_START) {
       return;
    }
    if (chan_is_play(&channels[channel])) {
        a_channel_remove(&channels[channel]);
    }
}

void audio_resume (void)
{
    if (chan_llist_ready.size)
        return;
    audio_need_update = 1;
}

int audio_is_playing (int handle)
{
    if (handle >= AUDIO_MAX_CHANS &&
       handle != AUDIO_MUS_CHAN_START) {
       return 1;
   }
    return chan_is_play(&channels[handle]);
}
void audio_set_pan (int handle, int l, int r)
{
   if (handle >= AUDIO_MAX_CHANS &&
       handle != AUDIO_MUS_CHAN_START) {
       return;
   }
   if (chan_is_play(&channels[handle])) {
#if USE_STEREO
        chan_vol(&channels[handle]) = ((l + r)) & MAX_VOL;
        channels[handle].left = (uint8_t)l << 1;
        channels[handle].right = (uint8_t)r << 1;
#else
        chan_vol(&channels[handle]) = ((l + r)) & MAX_VOL;
#endif
   }
}



#else /*AUDIO_MODULE_PRESENT*/

void audio_irq_save (int *irq)
{
    *irq = 0;
}

void audio_irq_restore (int irq)
{

}

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
