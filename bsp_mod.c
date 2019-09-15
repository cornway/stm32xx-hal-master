
#include <string.h>
#include <debug.h>
#include "boot/int/boot_int.h"
#include "int/bsp_mod_int.h"
#include <bsp_sys.h>
#include <misc_utils.h>
#include <dev_io.h>

#define MOD_MAX_NAME 24
#define MOD_MAX_PATH 128

typedef struct bspmod_s {
    struct bspmod_s *next;
    void *progaddr;
    void *entry;
    uint32_t size;

    char name[MOD_MAX_NAME];
    union {
        void *symdef;
        void *extra;
    };

    uint8_t type_rwpi: 1, /*position independent*/
            progaddr_offset: 3;
} bspmod_t;

typedef struct {
    bspmod_t *head;
    int elements;
} bspmod_list_t;

static bspmod_list_t bspmod_list = {0};

#define BSPMOD_IRAM_POOL_SIZE (0X1000)

static uint8_t bspmod_iram_pool[BSPMOD_IRAM_POOL_SIZE] __attribute__ ((section (".modules")));
static uint32_t bspmod_iram_pool_left = sizeof(bspmod_iram_pool);
static uint8_t *iram_pool_ptr = bspmod_iram_pool;

static int bspmod_iram_pool_ptr_chk (void *ptr)
{
    if ((uint8_t *)ptr < &bspmod_iram_pool[0]) {
        return 0;
    }
    if ((uint8_t *)ptr >= &bspmod_iram_pool[BSPMOD_IRAM_POOL_SIZE]) {
        return 0;
    }
    return 1;
}

static void *bspmod_iram_pool_alloc (uint32_t size)
{
    void *ptr;
    if (size >= (&bspmod_iram_pool[BSPMOD_IRAM_POOL_SIZE] - iram_pool_ptr)) {
        return NULL;
    }
    ptr = iram_pool_ptr;
    bspmod_iram_pool_left -= size;
    iram_pool_ptr += size;
    return ptr;
}

static void bspmod_iram_pool_release (uint32_t size)
{
    bspmod_iram_pool_left += size;
    if (bspmod_iram_pool_left == sizeof(bspmod_iram_pool)) {
        iram_pool_ptr = bspmod_iram_pool;
    }
}

static int bspmod_link (bspmod_t *mod)
{
    mod->next = bspmod_list.head;
    bspmod_list.head = mod;
    return bspmod_list.elements++;
}

static void
bspmod_unlink (bspmod_t *modrm)
{
    bspmod_t *mod = bspmod_list.head, *prev = NULL;

    while (mod) {
        if (mod == modrm) {
            if (prev) {
                prev->next = modrm->next;
            } else {
                bspmod_list.head = modrm->next;
            }
            bspmod_list.elements--;
            break;
        }
        prev = mod;
        mod = mod->next;
    }
    if (bspmod_iram_pool_ptr_chk((uint8_t *)modrm->progaddr - modrm->progaddr_offset)) {
        bspmod_iram_pool_release(modrm->size);
        modrm->progaddr = NULL;
    }
}

static bspmod_t *
bspmod_search_mod (const char *name)
{
    bspmod_t *mod = bspmod_list.head;

    while (mod) {
        if (d_astrmatch(mod->name, name) == 0) {
            return mod;
        }
        mod = mod->next;
    }
    return NULL;
}

static inline void *
_ARM_Linker_Symdef_Get_Main (bspmod_t *mod)
{
#define _ARM_ENTRY_SYMB_NAME ("Reset_Handler")

    void *entrypoint;

    entrypoint = bspmod_get_sym(mod, _ARM_ENTRY_SYMB_NAME);
    if (NULL == entrypoint) {
        dprintf("No entry point found; module \'%s\'\n", mod->name);
    } else {
        entrypoint = (void *)((uint8_t *)entrypoint + (arch_word_t)mod->progaddr);
    }
    return entrypoint;
#undef _ARM_ENTRY_SYMB_NAME
}

static inline int
_ARM_Linker_Symdef_Hdr_Chk (const char *hdr)
{
#define _CHK_HDR ("#<SYMDEFS>#")

    return !strncmp(hdr, _CHK_HDR, sizeof(_CHK_HDR) - 1);
#undef _CHK_HDR
}

static inline int
_ARM_Linker_Symdef_Line_Tok (int argc, const char **argv, char *str)
{
    argc = d_wstrtok(argv, argc, str);
    if (argc < 3) {
        return 0;
    }
    if (argv[0][0] == ';' || argv[0][1] == '#') {
        return 0;
    }
    return argc;
}

static inline void *
_ARM_Linker_Symdef_Ptr_Map (void *ptr)
{
#define ARM_LINK_SYM_OFFSET (0x8000)
    return (void *)((arch_word_t)ptr - 0x8000);
#undef ARM_LINK_SYM_OFFSET
}

static void
_bspmod_set_symdef (bspmod_t *mod, const char *dirpath)
{
    char name[MOD_MAX_NAME] = {0};
    const char *argv[2] = {0};

    /*TODO : dynamic linkage, now only position indep. supported*/
    mod->type_rwpi = 1;

    strcpy(name, mod->name);
    d_vstrtok(argv, 2, name, '.');

    snprintf((char *)mod->symdef, MOD_MAX_PATH, "%s/%s.sym", dirpath, argv[0]);
}

void *bspmod_insert (const bsp_heap_api_t *heap, const char *dirpath, const char *name)
{
#define _PROG_ALIGN (8)
    void *(*_alloc) (size_t) = heap ? heap->malloc : heap_malloc;
    void (*_free) (void *) = heap ? heap->free : heap_free;

    const int extra_space = MOD_MAX_PATH;
    char path[MOD_MAX_PATH];
    bspmod_t *mod;
    int err = -1;
    void *cache, *progaddr;

    mod = bspmod_search_mod(name);
    if (mod) {
        return NULL;
    }
    if (bsp_bin_file_fmt_supported(name) != BIN_FILE) {
        return NULL;
    }
    mod = _alloc(sizeof(*mod) + extra_space);
    assert(mod);

    d_memzero(mod, sizeof(*mod));
    mod->extra = (void *)(mod + 1);
    
    snprintf(path, MOD_MAX_PATH, "%s/%s", dirpath, name);
    cache = bres_cache_file_2_mem(_alloc, path, (int *)&mod->size);

    mod->size += _PROG_ALIGN;
    if (NULL == cache || mod->size == 0) {
        _free(mod);
    } else if (NULL == heap) {
        progaddr = bspmod_iram_pool_alloc(mod->size);
    } else {
        progaddr = _alloc(mod->size);
    }
    mod->progaddr = (void *)ROUND_UP((arch_word_t)progaddr, _PROG_ALIGN);
    mod->progaddr_offset = (unsigned)((uint8_t *)mod->progaddr - (uint8_t *)progaddr);

    if (mod->progaddr) {
        err = bhal_bin_2_mem_load(NULL, mod->progaddr, cache, mod->size / sizeof(arch_word_t));
        _free(cache);
    }
    if (err < 0) {
        _free(mod); _free((uint8_t *)mod->progaddr - mod->progaddr_offset);
        mod = NULL;
    } else {
        snprintf(mod->name, sizeof(mod->name), "%s", name);
        _bspmod_set_symdef(mod, dirpath);
        bspmod_link(mod);
    }
    return mod;
}

int bspmod_remove (void (*_free) (void *), const char *name)
{
    bspmod_t *mod;

    mod = bspmod_search_mod(name);
    if (!mod) {
        return -1;
    }
    bspmod_unlink(mod);
    if (mod->progaddr) {
        _free((uint8_t *)mod->progaddr - mod->progaddr_offset);
    }
    _free(mod);
    return 0;
}

int bspmod_probe (const char *name, int argc, const char **argv)
{
    void *entry;
    bspmod_t *mod;

    mod = bspmod_search_mod(name);
    if (!mod) {
        return -1;
    }
    if (NULL == mod->entry) {
        mod->entry = _ARM_Linker_Symdef_Get_Main(mod);
    }
    bsp_argc_argv_set(argv[0]);

    entry = mod->entry ? mod->entry : mod->progaddr;
    return bhal_execute_module(entry);
}

static void *
__bspmod_get_sym_separate (void *_mod, const char *name)
{
    bspmod_t *mod = (bspmod_t *)_mod;
    char buf[512], *str;
    const char *argv[4];
    int f, argc, symdef_ok = 0, line = 0;
    void *ptr = NULL;

    d_open((char *)mod->symdef, &f, "r");
    if (f  < 0) {
        return NULL;
    }
    if (!d_eof(f)) {
        line++;
        str = d_gets(f, buf, sizeof(buf));
        symdef_ok = _ARM_Linker_Symdef_Hdr_Chk(str);
    }
    while (symdef_ok && !d_eof(f)) {
        line++;
        str = d_gets(f, buf, sizeof(buf));

        argc = _ARM_Linker_Symdef_Line_Tok(argc, argv, str);
        if (!argc) {
            continue;
        }

        if (strcmp(argv[2], name) == 0) {
            if (!sscanf(argv[0], "%p", &ptr)) {
                ptr = NULL;
            }
            break;
        }
    }
    d_close(f);
    return ptr ? _ARM_Linker_Symdef_Ptr_Map(ptr) : NULL;
}

void *bspmod_get_sym (void *_mod, const char *name)
{
    bspmod_t *mod = (bspmod_t *)_mod;

    /*TODO :*/
    if (mod->type_rwpi) {
        return __bspmod_get_sym_separate(_mod, name);
    }
    dprintf("%s() : supported only \'type_rwpi\'\n", __func__);
    return NULL;
}

