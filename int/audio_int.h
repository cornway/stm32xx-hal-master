#ifndef _AUDIO_INT_H
#define _AUDIO_INT_H

#ifdef __cplusplus
    extern "C" {
#endif

#ifndef AUDIO_RATE_DEFAULT
#define AUDIO_RATE_DEFAULT 22050U
#endif

#define AUDIO_CHANNELS_NUM_DEFAULT 2
#define AUDIO_SAMPLEBITS_DEFAULT 16

#define AUDIO_MAX_VOICES 16
#define AUDIO_MUS_CHAN_START AUDIO_MAX_VOICES + 1
#define AUDIO_OUT_BUFFER_SIZE 0x800
#define AUDIO_BUFFER_MS(rate) AUDIO_SIZE_TO_MS(rate, AUDIO_OUT_BUFFER_SIZE)

#define REVERB_DELAY 5/*Ms*/
#define REVERB_SAMPLE_DELAY(rate) AUDIO_MS_TO_SIZE(rate, REVERB_DELAY)
#define REVERB_DECAY_MAX 255
#define REVERB_DECAY 128
#define REVERB_LIFE_TIME 1000

#define USE_STEREO 0
#define USE_REVERB 0
#define COMPRESSION 1
#define USE_FLOAT 1

/*TODO use define instead of 0*/
#define AUDIO_PLAY_SCHEME 0

#define AUDIO_TIMEOUT_MAX 2000 /*2 s*/
#define MAX_2BAND_VOL ((MAX_VOL) | (MAX_VOL << 8))
#define MAX_4BAND_VOL ((MAX_2BAND_VOL) | (MAX_2BAND_VOL << 16))
#define AUDIO_VOLUME_DEFAULT (60)
#define A_MAX_MIX_SAMPLES 8

typedef void (*a_mixer_callback_t) (int chan, void *stream, int len, void *udata);

typedef struct a_channel_head_s a_channel_head_t;
typedef struct a_channel_s a_channel_t;
typedef struct audio_s audio_t;

typedef struct mixdata_s {
    struct mixdata_s *next;
    int size;
    snd_sample_t *buf;
    uint8_t volume;
} mixdata_t;

typedef struct {
    audio_t *audio;
    snd_sample_t *buf;
    int samples;
    d_bool *dirty;
} a_buf_t;

struct a_channel_s {
    a_channel_t *next;
    a_channel_t *prev;

    audio_channel_t inst;
    int loopsize;
    snd_sample_t *bufposition;
    uint8_t volume;
#if USE_STEREO
    uint8_t left, right;
#endif
    uint8_t effect;
    uint8_t priority;
};

struct a_channel_head_s {
    a_channel_t       *first,
                      *last;
    uint16_t size;
    void (*empty_handle) (struct a_channel_head_s *head);
    void (*first_link_handle) (struct a_channel_head_s *head);
    void (*remove_handle) (struct a_channel_head_s *head, a_channel_t *rem);
};

typedef enum {
    A_ISR_NONE,
    A_ISR_HALF,
    A_ISR_COMP,
    A_ISR_MAX,
} isr_status_e;

typedef struct {
    uint32_t samplerate;
    uint32_t volume;
    uint32_t channels;
    uint32_t samplebits;
    irqmask_t irq;
} a_intcfg_t;

struct audio_s {
    a_channel_t pool[AUDIO_MUS_CHAN_START + 1/*Music channel*/];
    a_channel_head_t head;

    d_bool force_stop;
    irqmask_t irq_mask;

    a_mixer_callback_t amixer_callback = NULL;
    a_intcfg_t config;
};

#define a_chunk_len(chan) \
    ((chan)->inst.chunk.alen)

#define a_chunk_data(chan) \
    ((chan)->inst.chunk.abuf)

#define a_chunk_vol(chan) \
    ((chan)->inst.chunk.volume)

#define a_chn_play(chan) \
    ((chan)->inst.is_playing)

#define a_chunk_cache(chan) \
    ((chan)->inst.chunk.cache)

#define a_chn_cplt(chan) \
    (chan)->inst.complete

#define a_chn_loopstart(chan) \
    (chan)->inst.chunk.loopstart

#define a_chan_foreach_safe(head, channel, next) \
for (channel = (head)->first,\
     next = channel->next;   \
     channel;                \
     channel = next,         \
     next = next->next)

void a_hal_configure (a_intcfg_t *cfg);

void a_dsr_hung_fuse (isr_status_e status);
void a_paint_buff_helper (a_buf_t *abuf);
int a_channel_link (a_channel_head_t *head, a_channel_t *link, uint8_t sort);
int a_channel_unlink (a_channel_head_t *head, a_channel_t *node);
void a_channel_remove (audio_t *audio, a_channel_t *desc);
void a_paint_buffer (a_channel_head_t *chanlist, a_buf_t *abuf, int compratio);
uint8_t a_chanlist_try_reject_all (audio_t *audio, a_channel_head_t *chanlist);
a_intcfg_t *audio_current_config (void);

void error_handle (void);


#if USE_REVERB

void
a_rev_init (void);

#endif /*USE_REVERB*/

void a_mem_init (void);
void a_hal_deinit(void);
void a_get_master_base (a_buf_t *master);
void a_get_master4idx (a_buf_t *master, int idx);
void a_grab_mixdata (a_channel_t *channel, a_buf_t *track, mixdata_t *mixdata);
void a_clear_abuf (a_buf_t *abuf);
void a_clear_master (void);
d_bool a_wave_supported (wave_t *wave);
void a_hal_check_cfg (a_intcfg_t *cfg);
void a_mem_deinit (void);

void error_handle (void);

#ifdef __cplusplus
    }
#endif

#endif /*_AUDIO_INT_H*/
