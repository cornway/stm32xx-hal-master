
#include <misc_utils.h>
#include <audio_main.h>
#include <boot_int.h>
#include <heap.h>

#define BSFX_POOLMAX 16

typedef struct bsfx_s {
    int channel;
    int wavenum;
    uint8_t cache[1];
} bsfx_t;

bsfx_t *bsfx_pool[BSFX_POOLMAX] = {NULL};

static inline int
__alloc_sfx (void)
{
    int i;
    for (i = 0; i < BSFX_POOLMAX; i++) {
        if (bsfx_pool[i] == NULL) {
            return i;
        }
    }
    return -1;
}

static inline void
__set_sfx (int i, bsfx_t *sfx)
{
    assert(i < BSFX_POOLMAX);
    bsfx_pool[i] = sfx;
}

static inline bsfx_t *
__get_sfx (int i)
{
    assert(i < BSFX_POOLMAX);
    return bsfx_pool[i];
}

static bsfx_t *__boot_check_sfx_exist (int num)
{
    int i;
    for (i = 0; i < BSFX_POOLMAX; i++) {
        if (bsfx_pool[i] && bsfx_pool[i]->wavenum == num) {
            return bsfx_pool[i];
        }
    }
    return NULL;
}

int boot_audio_open_wave (const char *name)
{
    int cachenum, cachesize, sfxidx;
    bsfx_t *sfx;

    cachenum = audio_open_wave(name, -1);
    if (cachenum < 0) {
        return -1;
    }
    sfx = __boot_check_sfx_exist(cachenum);
    if (sfx) {
        return 0;
    }
    cachesize = audio_wave_size(cachenum);
    if (cachesize < 0) {
        return -1;
    }
    sfxidx = __alloc_sfx();
    if (sfxidx < 0) {
        return -1;
    }
    sfx = Sys_Malloc(sizeof(*sfx) + cachesize + 1);
    if (!sfx) {
        return -1;
    }
    sfx->wavenum = cachenum;
    sfx->channel = -1;
    __set_sfx(sfxidx, sfx);
    audio_cache_wave(cachenum, sfx->cache, cachesize);

    return sfxidx;
}

int boot_audio_play_wave (int hdl, uint8_t volume)
{
    int chanum;
    Mix_Chunk chunk;
    bsfx_t *sfx = __get_sfx(hdl);

    if (!sfx) {
        return -1;
    }
    chanum = audio_chk_priority(-1);
    if (chanum < 0) {
        chanum = audio_chk_priority(0);
        assert(chanum >= 0);
    }
    if (audio_is_playing(chanum)) {
        audio_stop_channel(chanum);
    }
    chunk.abuf = (snd_sample_t *)sfx->cache;
    chunk.alen = audio_wave_size(sfx->wavenum);
    chunk.allocated = 1;
    chunk.cache = (void **)&sfx->cache;
    chunk.loopstart = -1;
    chunk.volume = volume;
    if (!audio_play_channel(&chunk, chanum)) {
        return -1;
    }
    sfx->channel = chanum;

    return 0;
}

int boot_audio_stop_wave (int hdl)
{
    bsfx_t *sfx = __get_sfx(hdl);
    if (!audio_is_playing(sfx->channel)) {
        return -1;
    }
    audio_stop_channel(hdl);
}

void boot_audio_release_wave (int hdl)
{
    bsfx_t *sfx = __get_sfx(hdl);

    boot_audio_stop_wave(hdl);
    __set_sfx(hdl, NULL);
    audio_wave_close(sfx->wavenum);
    Sys_Free(sfx);
}

