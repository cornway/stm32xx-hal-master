#include <stdint.h>
#include <heap.h>
#include <bsp_sys.h>
#include <misc_utils.h>

#define BOOT_MAX_NAME 24
#define BOOT_MAX_PATH 128

#define BOOT_INV_ADDR ((arch_word_t)-1)

typedef enum {
    BIN_FILE,
    BIN_LINK,
    BIN_CMD,
    BIN_MAX,
} bsp_exec_file_type_t;

typedef struct boot_bin_parm_s {
    arch_word_t size;
    arch_word_t progaddr;
    arch_word_t entrypoint;
    arch_word_t spinitial;
} boot_bin_parm_t;

typedef struct boot_bin_s {
    struct boot_bin_s *next;
    void *pic;
    bsp_exec_file_type_t filetype;
    exec_mem_type_t memtype;

    boot_bin_parm_t parm;
    char name[BOOT_MAX_NAME];
    char path[BOOT_MAX_PATH];
    uint8_t id;
} bsp_bin_t;

typedef void (*bhal_cplth_t) (const char *, int);

arch_word_t *bsp_install_exec (arch_word_t *progaddr, const char *path);
int bsp_start_exec (arch_word_t *progaddr, int argc, const char **argv);

void *bsp_cache_bin_file (const bsp_heap_api_t *heapapi, const char *path, int *binsize);
bsp_bin_t *bsp_setup_bin_desc (const char *dirpath, bsp_bin_t *bin, const char *path,
                           const char *originname, bsp_exec_file_type_t type);
int bsp_setup_bin_param (bsp_bin_t *bin);
int bhal_load_program (bhal_cplth_t cplth, arch_word_t *progaddr,
                            void *progdata, size_t progsize);
int bhal_set_mem (bhal_cplth_t cplth, arch_word_t *progaddr,
                        size_t progsize, arch_word_t value);
bsp_exec_file_type_t bsp_bin_file_compat (const char *in);
d_bool bhal_prog_exist (bhal_cplth_t cplth, arch_word_t *progaddr, void *progdata, size_t progsize);
void bhal_execute_app (void *addr);
int bhal_execute_module (arch_word_t addr);


int bsp_open_wave_sfx (const char *name);
int bsp_play_wave_sfx (int hdl, uint8_t volume);
int bsp_stop_wave_sfx (int hdl);
void bsp_release_wave_sfx (int hdl);

int boot_cmd_handle (int argc, const char **argv);

int boot_print_bin_list (int argc, const char **argv);

enum {
    BOOT_LOG_NONE = -1,
    BOOT_LOG_ERR = 0,
    BOOT_LOG_WARN,
    BOOT_LOG_INFO,
    BOOT_LOG_INFO2,
    BOOT_LOG_MAX,
};

extern int g_boot_log_level;

#define BOOT_LOG_CHECK(lvl) (g_boot_log_level >= lvl)
#define BOOT_ERR(args ...)      ((BOOT_LOG_CHECK(BOOT_LOG_ERR) ? boot_log(0, "[ERROR]: "args) : -1)
#define BOOT_WARN(args ...)     ((BOOT_LOG_CHECK(BOOT_LOG_WARN) ? boot_log(0, "[WARN]: "args) : -1)
#define BOOT_INFO(args ...)     ((BOOT_LOG_CHECK(BOOT_LOG_INFO) ? boot_log(0, "[INFO]: "args) : -1)
#define BOOT_INFO2(args ...)    ((BOOT_LOG_CHECK(BOOT_LOG_INFO2) ? boot_log(0, "[INFO_2]: "args) : -1)
#define BOOT_LOG_ALWAYS(args ...) boot_log(0, "[INFO_3]: "args)

int boot_log (int, const char *, ...) PRINTF_ATTR(2, 3);
int boot_log_comp_hex_le_u32 (const void *a, const void *b, int size);
int boot_log_hex (const void *data, int len);

#define B_MAX_LINEBUF (1 << 10)
#define B_MAX_BIN_SIZE (RW_PORTION * MAX_LINEBUF) /*1 Mbyte*/


