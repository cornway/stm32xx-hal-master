#include "string.h"
#include "stdio.h"
#include "audio_main.h"
#include "audio_int.h"
#include "dev_io.h"

#define A_MAXNAME 9
#define A_MAXPATH 128
#define A_MAXCACHE 128
#define A_MAX_REFCNT 255

typedef struct {
    int size;
    int16_t dataoff;
    int16_t lumpnum;
    uint8_t ref; /*TODO : make atomic*/
    char name[A_MAXNAME];
} snd_cache_t;

static snd_cache_t snd_num_cache[A_MAXCACHE] = {0};

extern const char *snd_dir_path;

static inline char *
__get_full_path (char *buf, int bufsize, const char *base, const char *name)
{
    snprintf(buf, bufsize, "%s/%s.WAV", base, name);
    return buf;
}

static int
__alloc_slot ()
{
    for (int i = 0; i < arrlen(snd_num_cache); i++) {
        if (snd_num_cache[i].size == 0) {
            return i;
        }
    }
    return -1;
}

static void __release_slot (int num)
{
    assert(num < arrlen(snd_num_cache));
    memset(&snd_num_cache[num], 0, sizeof(snd_num_cache[num]));
}

static int
__get_slot (int lumnum)
{
    for (int i = 0; i < arrlen(snd_num_cache); i++) {
        if (snd_num_cache[i].lumpnum == lumnum) {
            return i;
        }
    }
    return -1;
}

static void
__set_name (int slot, const char *name)
{
    snd_cache_t *pslot = &snd_num_cache[slot];
    snprintf(pslot->name, sizeof(pslot->name), name);
}

d_bool a_check_wave_sup (wave_t *wave)
{
    if (readShort(&wave->BitPerSample) != AUDIO_OUT_BITS) {
        return d_false;
    }
    if (readShort(&wave->NbrChannels) != AUDIO_OUT_CHANNELS) {
        return d_false;
    }
    if (readLong(&wave->SampleRate) != AUDIO_SAMPLE_RATE) {
        return d_false;
    }
    return d_true;
}

static int
a_check_wave_exist (const char *name)
{
    int i;

    for (i = 0; i < arrlen(snd_num_cache); i++) {
        if (strcmp(snd_num_cache[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int
audio_open_wave (const char *name, int num)
{
    int cache_num = -1;
    int file, size;
    wave_t wave;

    char path[A_MAXPATH], *ppath;

    cache_num = __get_slot(num);
    if (cache_num >= 0) {
        return cache_num;
    }
    cache_num = a_check_wave_exist(name);
    if (cache_num >= 0) {
        return cache_num;
    }

    ppath = __get_full_path(path, sizeof(path), snd_dir_path, name);

    size = d_open(ppath, &file, "r");

    if (file < 0) {
        return cache_num;
    }
    assert(size >= 0);

    if (d_read(file, &wave, sizeof(wave)) <= 0) {
        goto closefile;
    }
    if (!a_check_wave_sup(&wave)) {
        goto closefile;
    }

    cache_num = __alloc_slot();
    if (cache_num < 0) {
        return -1;
    }
    __set_name(cache_num, name);
    snd_num_cache[cache_num].lumpnum = cache_num;
    snd_num_cache[cache_num].size = size - wave.SubChunk1Size;
    /*TODO : fix for SubChunk2Size*/
    snd_num_cache[cache_num].dataoff = wave.SubChunk1Size;
closefile:
    d_close(file);
    return cache_num;
}

int
audio_wave_size (int num)
{
    if (num >= arrlen(snd_num_cache)) {
        return -1;
    }
    if (snd_num_cache[num].size > 0) {
        return snd_num_cache[num].size;
    }
    error_handle();
    return 0;
}

int
audio_cache_wave (int num, uint8_t *dest, int size)
{
    char path[A_MAXPATH], *ppath;
    int file;

    ppath = __get_full_path(path, sizeof(path), snd_dir_path, snd_num_cache[num].name);
    d_open(ppath, &file, "r");
    if (file < 0) {
        return -1;
    }
    d_seek(file, snd_num_cache[num].dataoff, DSEEK_SET);
    if (d_read(file, dest, size) < 0) {
        error_handle();
        return -1;
    }
    d_close(file);
    return size;
}

void
audio_wave_close (int num)
{
    __release_slot(num);
}

