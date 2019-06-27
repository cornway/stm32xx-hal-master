#include <stdint.h>
#include <gfx.h>
#include <misc_utils.h>
#include <debug.h>
#include <gui.h>
#include <heap.h>

#define BOOT_MAX_NAME 24
#define BOOT_MAX_PATH 128

typedef enum {
    BIN_NONE = 0,
    BIN_FILE,
    BIN_LINK,
} bintype_t;

typedef struct {
    bintype_t type;
    const char *ext;
} typebind_t;

typedef struct boot_bin_s {
    struct boot_bin_s *next;
    bintype_t type;

    arch_word_t size;
    arch_word_t progaddr;
    arch_word_t entrypoint;
    arch_word_t spinitial;
    char name[BOOT_MAX_NAME];
    char path[BOOT_MAX_PATH];
} bsp_bin_t;

typedef void (*cplthook_t) (const char *msg, int per);

void *bsp_cache_bin_file (const bsp_heap_api_t *heapapi, const char *path, int *binsize);
bsp_bin_t *bsp_setup_bin_desc (bsp_bin_t *bin, const char *path,
                           const char *originname, bintype_t type);
void bsp_setup_bin_param (bsp_bin_t *bin);
int bhal_load_program (cplthook_t cplth, arch_word_t *progaddr,
                            void *progdata, size_t progsize);
bintype_t bsp_bin_file_compat (const char *in);
d_bool bhal_prog_exist (arch_word_t *progaddr, void *progdata, size_t progsize);
void bhal_execute_app (void *addr);
int bhal_execute_module (arch_word_t addr);


int bsp_open_wave_sfx (const char *name);
int bsp_play_wave_sfx (int hdl, uint8_t volume);
int bsp_stop_wave_sfx (int hdl);
void bsp_release_wave_sfx (int hdl);

