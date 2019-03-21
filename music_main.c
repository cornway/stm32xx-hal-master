#include "audio_main.h"
#include "audio_int.h"
#include "ff.h"
#include <stdio.h>

#if MUSIC_MODULE_PRESENT

extern int32_t systime;

static void error_handle (void)
{
    for (;;) {}
}

#define N(x) (sizeof(x) / sizeof(x[0]))

#define MUS_BUF_GUARD_MS (AUDIO_MS_TO_SIZE(AUDIO_SAMPLE_RATE, AUDIO_OUT_BUFFER_SIZE) / 2)
#define MUS_CHK_GUARD(cd) ((HAL_GetTick()- (cd)->last_upd_tsf) > MUS_BUF_GUARD_MS)

#define MUS_BUF_LEN_MS 1000

/*IMPORTANT NOTE : music won't play with small cache buffer (less than 'AUDIO_OUT_BUFFER_SIZE * 5'), -
  frequent access to the sd card (or any other storage)
  in blocking mode (currently used mode) - will hungs up system forever!
*/
/*NOTE 2 : cache size of 'AUDIO_OUT_BUFFER_SIZE * 5' seems to be ok,
  but such size will cause less fps.
*/
#define MUS_RAM_BUF_SIZE AUDIO_OUT_BUFFER_SIZE * 5

static snd_sample_t mus_ram_buf[2][MUS_RAM_BUF_SIZE];

typedef struct song song_t;

#define SONG_DESC(cd_track) ((song_t *)(cd_track)->desc)

struct song {
    FIL *file;
    int track_rem;
    int rem_buf;
    snd_sample_t *buf;
    wave_t info;
    uint32_t mem_offset;
    audio_channel_t *channel;
    char path[64];
    uint8_t ready;
    uint8_t volume;
};

typedef enum {
    MUS_IDLE,
    MUS_UPDATING,
    MUS_PLAYING,
    MUS_REPEAT,
    MUS_PAUSE,
    MUS_STOP,
} mus_state_t;

typedef enum {
    NONE,
    SINGLE_LOOP,
} mus_repeat_t;

typedef struct cdaudio_s cdaudio_t;

struct cdaudio_s {
    mus_repeat_t repeat;
    mus_state_t state;
    int last_upd_tsf;
    int8_t rd_idx;
};

FIL cd_tack_fhandle;
song_t cd_track;
cdaudio_t cdaudio;

static void song_update_next_buf (cdaudio_t *cdaudio, song_t *song);
static void song_setup (cdaudio_t *cd, song_t *song);
static int chunk_setup (cdaudio_t *cd, song_t *song);


static void song_reset (song_t *song)
{
    memset(song, 0, sizeof(*song));
}

static void mus_reset (cdaudio_t *cd)
{
    cd->rd_idx = 0;
    cd->repeat = SINGLE_LOOP;
    cd->state = MUS_IDLE;
}

static inline void
song_alloc_file (song_t *song)
{
    song->file = &cd_tack_fhandle;
}

static int mus_cplt_hdlr (int complete)
{
    if (complete == 2) {
        if (cd_track.track_rem <=  0) {
            if (cdaudio.repeat) {
                cdaudio.state = MUS_REPEAT;
            }
            return 1;
        }
        if (cdaudio.state == MUS_PAUSE ||
            cdaudio.state == MUS_STOP ||
            cdaudio.state == MUS_REPEAT ||
            !cd_track.volume) {
            /*This will cause 'fake channel' - it will won't play, but still in 'ready'*/
            return 0;
        }
        song_setup(&cdaudio, &cd_track);
    } else if (complete == 1) {
        if (cdaudio.state == MUS_PAUSE ||
            cdaudio.state == MUS_STOP ||
            cdaudio.state == MUS_REPEAT ||
            !cd_track.volume) {
            /*Skip this*/
            return 1;
        }
    }
    return 0;
}

static int mus_open_wav (song_t *song, const char *path)
{
    uint32_t btr;
    FRESULT res = FR_OK;
    if (song->file == NULL) {
        song_alloc_file(song);
    }
    strcpy(song->path, path);
    res = f_open(song->file, song->path, FA_READ | FA_OPEN_EXISTING);
    if (res != FR_OK)
        return -1;
    res = f_read(song->file, &song->info, sizeof(song->info), &btr);
    if (res != FR_OK || btr != sizeof(song->info)) {
        error_handle();
    }
    if (song->info.BitPerSample != 16) {
        error_handle();
    }
    if (song->info.NbrChannels != 2) {
        error_handle();
    }
    if (song->info.SampleRate != AUDIO_SAMPLE_RATE) {
        error_handle();
    }
    song->track_rem = song->info.FileSize / sizeof(snd_sample_t);
    song->ready = 1;
}

static void mus_close_wav (cdaudio_t *cd, song_t *song)
{
    f_close(song->file);
    song_reset(song);
    mus_reset(cd);
}

static int music_play_helper (cdaudio_t *cd, song_t *song, char *path, int repeat)
{
    Mix_Chunk mixchunk;
    if (cdaudio.state != MUS_IDLE) {
        mus_close_wav(cd, song);
    }

    if (mus_open_wav(song, path) < 0) {
        return -1;
    }
    cd->state = MUS_PLAYING;
    cd->repeat = repeat ? SINGLE_LOOP : NONE;

    song_update_next_buf(cd, song);
    /*Little hack - force channel reject to update it through callback*/
    mixchunk.alen = 0;
    mixchunk.loopstart = -1;

    if (audio_is_playing(AUDIO_MUS_CHAN_START)) {
        audio_stop_channel(AUDIO_MUS_CHAN_START);
    }
    song->channel = audio_play_channel(&mixchunk, AUDIO_MUS_CHAN_START);
    if (!song->channel) {
        error_handle();
    }
    song->channel->complete = mus_cplt_hdlr;
    return 0;
}

static int
chunk_setup (cdaudio_t *cd, song_t *song)
{
    snd_sample_t *ram;
    audio_channel_t *channel = song->channel;
    Mix_Chunk *chunk = &channel->chunk;
    int32_t rem = song->track_rem;

    ram = mus_ram_buf[cd->rd_idx];

    if (rem > MUS_RAM_BUF_SIZE) {
        rem = MUS_RAM_BUF_SIZE;
    }
    chunk->abuf = ram;
    chunk->alen = rem;
    chunk->volume = song->volume;
    return rem;
}

static void song_setup (cdaudio_t *cd, song_t *song)
{
    int32_t rem = song->track_rem;

    if (song->channel) {
        cd->rd_idx ^= 1;
        rem = chunk_setup(cd, song);
        song->track_rem -= rem;
        cd->state = MUS_UPDATING;
    }
}

static void song_repeat (cdaudio_t *cd, song_t *song)
{
    char path[128];
    strcpy(path, song->path);
    music_play_helper(cd, song, path, cd->repeat);
}

void music_tickle (void)
{
    if (cdaudio.state == MUS_PLAYING) {

    } else if (cdaudio.state == MUS_UPDATING) {
        song_update_next_buf(&cdaudio, &cd_track);
        cdaudio.state = MUS_PLAYING;
    } else if (cdaudio.state == MUS_STOP) {

        audio_stop_channel(AUDIO_MUS_CHAN_START);
        mus_close_wav(&cdaudio, &cd_track);
        mus_reset(&cdaudio);
    } else if (cdaudio.state == MUS_REPEAT) {

        song_repeat(&cdaudio, &cd_track);
    }
}

static void song_update_next_buf (cdaudio_t *cd, song_t *song)
{
    snd_sample_t *dest;
    uint32_t btr = 0;
    FRESULT res;
    int32_t rem = song->track_rem;

    dest = mus_ram_buf[cd->rd_idx ^ 1];

    if (rem > MUS_RAM_BUF_SIZE)
        rem = MUS_RAM_BUF_SIZE;

    res = f_read(song->file, dest, rem * sizeof(snd_sample_t), &btr);
    if (res != FR_OK) {
        error_handle();
    }
}

int music_playing (void)
{
    return (    (cdaudio.state == MUS_PLAYING) || 
                (cdaudio.state == MUS_UPDATING)
            );
}

cd_track_t *music_play_song_name (cd_track_t *track, const char *path)
{
    track->desc = &cd_track;
    if (music_play_helper(&cdaudio, &cd_track, (char *)path, 1) < 0) {
        return NULL;
    }
    return track;
}

int music_pause (cd_track_t *track)
{
    cdaudio.state = MUS_PAUSE;
    return 1;
}

int music_resume (cd_track_t *track)
{
    if (cdaudio.state == MUS_PAUSE) {
        cdaudio.state = MUS_PLAYING;
        return 0;
    }
    return 1;
}

int music_stop (cd_track_t *track)
{
    if (cdaudio.state != MUS_IDLE)
        cdaudio.state = MUS_STOP;
    return 0;
}

int music_set_vol (cd_track_t *track, uint8_t vol)
{
    song_t *s = SONG_DESC(track);
    s->volume = vol;
    return 0;
}

uint8_t music_get_volume (cd_track_t *track)
{
    return SONG_DESC(track)->volume;
}

int music_init (void)
{
    mus_reset(&cdaudio);
    song_reset(&cd_track);
    cd_track.volume = MAX_VOL / 2;
    return 0;
}

#else

int music_init (void)
{
    return 0;
}

cd_track_t *music_play_song_name (cd_track_t *track, const char *path)
{
    return NULL;
}

int music_pause (cd_track_t *track)
{
    return 0;
}

int music_resume (cd_track_t *track)
{
    return 0;
}

int music_stop (cd_track_t *track)
{
    return 0;
}


int music_playing (cd_track_t *track)
{
    return 0;
}

void music_tickle (cd_track_t *track)
{

}

#endif
