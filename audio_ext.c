#include "string.h"
#include "stdio.h"
#include "audio_main.h"
#include "audio_int.h"
#include "dev_io.h"

typedef struct {
    char name[9];
    int size;
    int16_t lumpnum;
} snd_cache_t;

static snd_cache_t snd_num_cache[128];

extern const char *snd_dir_path;


#define GET_WAV_PATH(buf, path, name) \
    snprintf(buf, sizeof(buf), "%s/%s.WAV", path, name);

static int
snd_ext_alloc_slot ()
{
    for (int i = 0; i < arrlen(snd_num_cache); i++) {
        if (snd_num_cache[i].size == 0) {
            return i;
        }
    }
    return -1;
}

static int
snd_ext_get_slot (int lumnum)
{
    for (int i = 0; i < arrlen(snd_num_cache); i++) {
        if (snd_num_cache[i].lumpnum == lumnum) {
            return i;
        }
    }
    return -1;
}


static void
snd_cache_set_name (int slot, char *name)
{
    strncpy(snd_num_cache[slot].name, name, 8);
    snd_num_cache[slot].name[8] = 0;
}

int
search_ext_sound (char *_name, int num)
{
    int cache_num = -1;
    int file, size;

    char path[64];
    char name[9] = {0};

    cache_num = snd_ext_get_slot(num);
    if (cache_num >= 0) {
        return cache_num;
    }

    strncpy(name, _name, sizeof(name));
    
    GET_WAV_PATH(path, snd_dir_path, name);

    size = d_open(path, &file, "r");

    if (file < 0) {
        return cache_num;
    }
    if (size < 0) {
        d_close(file);
        return cache_num;
    }
    cache_num = snd_ext_alloc_slot();
    if (cache_num < 0) {
        error_handle();
    }
    snd_cache_set_name(cache_num, name);
    snd_num_cache[cache_num].lumpnum = cache_num;
    snd_num_cache[cache_num].size = size;
    d_close(file);
    return cache_num;
}

int
get_ext_snd_size (int num)
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
cache_ext_sound (int num, uint8_t *dest, int size)
{
    char path[64];
    int file;

    GET_WAV_PATH(path, snd_dir_path, snd_num_cache[num].name);
    d_open(path, &file, "r");
    if (file < 0) {
        return -1;
    }

    if (d_read(file, dest, size) < 0) {
        error_handle();
        return -1;
    }
    
    d_close(file);
    return size;
}

