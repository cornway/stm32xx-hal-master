#ifndef _AUDIO_MAIN_H
#define _AUDIO_MAIN_H

#include "stdint.h"
#include "dev_conf.h"
#include <nvic.h>
#include <bsp_api.h>

#define AUDIO_SIZE_TO_MS(rate, size) (((long long)(size) * 1000) / (rate))
#define AUDIO_MS_TO_SIZE(rate, ms) (((((rate) << 2) / 1000) * (ms)) >> 2)

#ifndef AUDIO_SAMPLE_RATE
#define AUDIO_SAMPLE_RATE 11025U
#endif

#define AUDIO_OUT_CHANNELS 2
#define AUDIO_OUT_BITS 16

#define AUDIO_MAX_VOICES 16
#define AUDIO_MUS_CHAN_START AUDIO_MAX_VOICES + 1
#define AUDIO_OUT_BUFFER_SIZE 0x800
#define AUDIO_BUFFER_MS AUDIO_SIZE_TO_MS(AUDIO_SAMPLE_RATE, AUDIO_OUT_BUFFER_SIZE)

#define REVERB_DELAY 5/*Ms*/
#define REVERB_SAMPLE_DELAY AUDIO_MS_TO_SIZE(AUDIO_SAMPLE_RATE, REVERB_DELAY)
#define REVERB_DECAY_MAX 255
#define REVERB_DECAY 128
#define REVERB_LIFE_TIME 1000

#define MAX_VOL (0x7f)

#define AUDIO_SAMPLES_2_BYTES(samples) ((samples) * sizeof(snd_sample_t))
#define AUDIO_SAMPLES_2_WORDS(samples) (AUDIO_SAMPLES_2_BYTES(samples) / sizeof(uint32_t))
#define AUDIO_SAMPLES_2_DWORDS(samples) (AUDIO_SAMPLES_2_BYTES(samples) / sizeof(uint64_t))

#define AUDIO_BYTES_2_SAMPLES(bytes) ((bytes) / sizeof(snd_sample_t))

typedef struct {
  uint32_t ChunkID;       /* 0 */ 
  uint32_t FileSize;      /* 4 */
  uint32_t FileFormat;    /* 8 */
  uint32_t SubChunk1ID;   /* 12 */
  uint32_t SubChunk1Size; /* 16*/  
  uint16_t AudioFormat;   /* 20 */ 
  uint16_t NbrChannels;   /* 22 */   
  uint32_t SampleRate;    /* 24 */
  
  uint32_t ByteRate;      /* 28 */
  uint16_t BlockAlign;    /* 32 */  
  uint16_t BitPerSample;  /* 34 */  
  uint32_t SubChunk2ID;   /* 36 */   
  uint32_t SubChunk2Size; /* 40 */    
} wave_t;


typedef int16_t snd_sample_t;

typedef struct {
    int allocated;
    snd_sample_t *abuf;
    int32_t alen;
    void **cache;
    int loopstart;
    uint8_t volume;     /* Per-sample volume, 0-128 */
} Mix_Chunk;

typedef enum {
    A_NONE,
    A_HALF,
    A_FULL
} cplt_stat_t;

typedef struct {
    Mix_Chunk chunk;
    int8_t is_playing;
    uint8_t id;
    int (*complete) (uint8_t *, uint32_t, cplt_stat_t);
} audio_channel_t;

typedef struct {
    void *desc;
} cd_track_t;

#if BSP_INDIR_API

#define audio_init   g_bspapi->snd.init
#define audio_deinit   g_bspapi->snd.deinit
#define audio_mixer_ext   g_bspapi->snd.mixer_ext
#define audio_play_channel   g_bspapi->snd.play
#define audio_stop_channel   g_bspapi->snd.stop
#define audio_pause   g_bspapi->snd.pause
#define audio_stop_all   g_bspapi->snd.stop_all
#define audio_is_playing   g_bspapi->snd.is_play
#define audio_chk_priority   g_bspapi->snd.check
#define audio_set_pan   g_bspapi->snd.set_pan
#define audio_change_sample_volume   api->snd.sample_volume
#define audio_update   g_bspapi->snd.tickle
#define audio_irq_save   g_bspapi->snd.irq_save
#define audio_irq_restore   g_bspapi->snd.irq_restore

#define audio_open_wave   g_bspapi->snd.wave_open
#define audio_wave_size   g_bspapi->snd.wave_size
#define audio_cache_wave   g_bspapi->snd.wave_cache
#define audio_wave_close   g_bspapi->snd.wave_close

#define cd_play_name   g_bspapi->cd.play
#define cd_pause   g_bspapi->cd.pause
#define cd_resume   g_bspapi->cd.resume
#define cd_stop   g_bspapi->cd.stop
#define cd_volume   g_bspapi->cd.volume
#define cd_getvol   g_bspapi->cd.getvol
#define cd_playing   g_bspapi->cd.playing

#else
void audio_init (void);
void audio_deinit (void);
void audio_mixer_ext (void (*mixer_callback) (int, void *, int, void *));
audio_channel_t *audio_play_channel (Mix_Chunk *chunk, int channel);
audio_channel_t *audio_stop_channel (int channel);
void audio_pause (int channel);
void audio_stop_all (void);
int audio_is_playing (int handle);
int audio_chk_priority (int priority);
void audio_set_pan (int handle, int l, int r);
void audio_change_sample_volume (audio_channel_t *achannel, uint8_t volume);
void audio_update (void);
void audio_irq_save (irqmask_t *flags);
void audio_irq_restore (irqmask_t flags);

int audio_open_wave (const char *name, int num);
int audio_wave_size (int num);
int audio_cache_wave (int num, uint8_t *dest, int size);
void audio_wave_close (int num);

cd_track_t *cd_play_name (cd_track_t *track, const char *path);
int cd_pause (cd_track_t *track);
int cd_resume (cd_track_t *track);
int cd_stop (cd_track_t *track);
int cd_volume (cd_track_t *track, uint8_t vol);
uint8_t cd_getvol (cd_track_t *track);
int cd_playing (void);
#endif


#endif /*_AUDIO_MAIN_H*/
