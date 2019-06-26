#include <stdint.h>
#include <gfx.h>
#include <misc_utils.h>
#include <debug.h>
#include <gui.h>

typedef void (*cplthook_t) (const char *msg, int per);

int bhal_load_program (cplthook_t cplth, arch_word_t *progaddr,
                            void *progdata, size_t progsize);
d_bool bhal_prog_exist (arch_word_t *progaddr, void *progdata, size_t progsize);
void bhal_boot (void *addr);


int boot_audio_open_wave (const char *name);
int boot_audio_play_wave (int hdl, uint8_t volume);
int boot_audio_stop_wave (int hdl);
void boot_audio_release_wave (int hdl);

