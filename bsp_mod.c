
#include <string.h>
#include <boot_int.h>
#include <bsp_sys.h>
#include "./int/bsp_mod_int.h"

#define MOD_MAX_NAME 24

#define MOD_EXEC_ALLOWED() (g_exec_region == EXEC_APPLICATION)
#define MOD_EXEC_REGION()  (g_exec_region == EXEC_MODULE)

typedef struct bspmod_s {
    struct bspmod_s *next;
    bsp_bin_t bin;

    const void *api;
    int apisize;
    bsp_heap_api_t *heap;
    char name[MOD_MAX_NAME];
} bspmod_t;

typedef struct {
    bspmod_t *head;
    int elements;
} bspmod_list_t;

static bspmod_list_t bspmod_list = {0};

static int bspmod_link (bspmod_t *mod)
{
    mod->next = bspmod_list.head;
    bspmod_list.head = mod;
    return bspmod_list.elements++;
}

static void bspmod_unlink (bspmod_t *modrm)
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
}

static bspmod_t *bspmod_search_mod (const char *name)
{
    bspmod_t *mod = bspmod_list.head;

    while (mod) {
        if (strcmp(mod->name, name) == 0) {
            return mod;
        }
        mod = mod->next;
    }
    return NULL;
}

static d_bool bspmod_check_to_insert (bspmod_t *modchk)
{
    bspmod_t *mod = bspmod_list.head;

    while (mod) {
        if (mod->bin.entrypoint == modchk->bin.entrypoint) {
            return d_false;
        }
        mod = mod->next;
    }
    return d_true;
}

void *bspmod_insert (const bsp_heap_api_t *heap, const char *path, const char *name)
{
    bspmod_t *mod;
    bsp_bin_t *bin;
    bintype_t bintype;
    void *rawptr;

    assert(MOD_EXEC_ALLOWED());

    mod = bspmod_search_mod(name);
    if (mod) {
        return NULL;
    }

    bintype = bsp_bin_file_compat(name);

    if (bintype != BIN_FILE) {
        return NULL;
    }

    mod = heap->malloc(sizeof(*mod));
    assert(mod);

    bin = bsp_setup_bin_desc(&mod->bin, path, name, bintype);

    if (!bin || !bspmod_check_to_insert(mod)) {
        heap->free(mod);
        return NULL;
    }

    rawptr = bsp_cache_bin_file(heap, bin->path, (int *)&bin->size);

    if (!rawptr || bhal_load_program(NULL, (arch_word_t *)bin->progaddr, rawptr, bin->size) < 0) {
        heap->free(mod);
        return NULL;
    }
    mod->heap = (bsp_heap_api_t *)heap;
    bspmod_link(mod);
    heap->free(rawptr);

    return mod;
}

int bspmod_remove (const char *name)
{
    bspmod_t *mod;

    assert(MOD_EXEC_ALLOWED());

    mod = bspmod_search_mod(name);
    if (!mod) {
        return -1;
    }
    bspmod_unlink(mod);
    mod->heap->free(mod);
    return 0;
}

int bspmod_probe (const void *_mod)
{
    bspmod_t *mod = (bspmod_t *)_mod;

    assert(MOD_EXEC_ALLOWED());

    return bhal_execute_module(mod->bin.entrypoint);
}

/*must be used only within 'module' code*/
int bspmod_register_api (const char *name, const void *api, int apisize)
{
    bspmod_t *mod;

    assert(MOD_EXEC_REGION())

    mod = bspmod_search_mod(name);
    if (!mod) {
        return -1;
    }
    mod->api = api;
    mod->apisize = apisize;
    return 0;
}

const void *bspmod_get_api (const char *name, int *apisize)
{
    bspmod_t *mod;

    assert(MOD_EXEC_ALLOWED());

    mod = bspmod_search_mod(name);
    if (!mod) {
        *apisize = 0;
        return NULL;
    }
    *apisize = mod->apisize;
    return mod->api;
}

