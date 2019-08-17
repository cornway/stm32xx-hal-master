#include <string.h>

#include <debug.h>
#include "int/boot_int.h"

#include <misc_utils.h>
#include <bconf.h>
#include <audio_main.h>

const char *sfx_names[] =
{
    [SFX_MOVE] = "sfxmove.wav",
    [SFX_SCROLL] = "sfxmisc1.wav",
    [SFX_WARNING] = "sfxwarn.wav",
    [SFX_CONFIRM] = "sfxconf.wav",
    [SFX_START_APP] = "sfxstart2.wav",
    [SFX_NOWAY] = "sfxnoway.wav"
};

static int sfx_ids[arrlen(sfx_names)] = {0};
static cd_track_t boot_cd = {NULL};

void bsfx_sound_precache (void)
{
    int i;
    char path[BOOT_MAX_PATH];

    for (i = 0; i < arrlen(sfx_names); i++) {
        snprintf(path, sizeof(path), "%s/%s", BOOT_SFX_DIR_PATH, sfx_names[i]);
        sfx_ids[i] = bsp_open_wave_sfx(path);
    }
}

void bsfx_sound_free (void)
{
    int i;

    for (i = 0; i < arrlen(sfx_names); i++) {
        bsp_release_wave_sfx(sfx_ids[i]);;
        sfx_ids[i] = -1;
    }
}

void bsfx_start_sound (int num, uint8_t volume)
{
    bsp_play_wave_sfx(num, volume);
}

void bsfx_title_music (int play, uint8_t volume)
{
    if (!play) {
        cd_stop(&boot_cd);
    } else {
        cd_play_name(&boot_cd, BOOT_STARTUP_MUSIC_PATH);
        cd_volume(&boot_cd, volume);
    }
}
