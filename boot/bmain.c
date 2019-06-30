#include <string.h>
#include "int/boot_int.h"
#include "../int/term_int.h"
#include <gfx.h>
#include <gui.h>
#include <dev_io.h>
#include <debug.h>
#include <nvic.h>
#include <input_main.h>
#include <lcd_main.h>
#include <heap.h>
#include <bsp_sys.h>
#include <bsp_cmd.h>

#if defined(BOOT)

#define BOOT_SYS_DIR_PATH "/sys"
#define BOOT_SYS_LOG_NAME "log.txt"
#define BOOT_BIN_DIR_NAME "BIN"
#define BOOT_SYS_LOG_PATH BOOT_SYS_DIR_PATH"/"BOOT_SYS_LOG_NAME

extern uint32_t g_app_program_addr;

static int boot_program_bypas = 0;

component_t *com_browser = NULL;
component_t *com_console = NULL;
component_t *com_title = NULL;
gui_t gui;
pane_t *pane, *alert_pane;
pane_t *pane_console;

bsp_bin_t *boot_bin_head = NULL;
bsp_bin_t *boot_bin_selected = NULL;

static void boot_gui_bsp_init (gui_t *gui);

bsp_exec_file_type_t
bsp_bin_file_compat (const char *in)
{
#define EXT_LEN (4)

    const struct exec_file_ext_map {
        bsp_exec_file_type_t type;
        const char *ext;
    } extmap[BIN_MAX] =
    {
        {BIN_FILE, ".bin"},
        {BIN_LINK, ".lnk"},
    };

    int pos = strlen(in) - EXT_LEN;
    int i;

    for (i = 0; i < arrlen(extmap); i++) {
        if (!strcasecmp(in + pos, extmap[i].ext)) {
            return extmap[i].type;
        }
    }
    return BIN_MAX;
}

static void boot_add_exec_2_list (bsp_bin_t *bin)
{
    bin->next = NULL;
    if (!boot_bin_head) {
        boot_bin_head = bin;
        boot_bin_selected = bin;
    } else {
        bin->next = boot_bin_head;
        boot_bin_head = bin;
    }
}

static void boot_destr_exec_list (void)
{
    bsp_bin_t *bin = boot_bin_head;

    while (bin) {

        heap_free(bin);
        bin = bin->next;
    }
    boot_bin_head = NULL;
}


int
bsp_setup_bin_param (bsp_bin_t *bin)
{
    int f, size;
    arch_word_t entry;

    size = d_open(bin->path, &f, "r");
    if (f < 0) {
        return -1;
    }
    d_read(f, &bin->spinitial, sizeof(entry));
    d_read(f, &bin->entrypoint, sizeof(entry));
    d_close(f);
    bin->size = size;
    return 0;
}

bsp_bin_t *
bsp_setup_bin_desc (bsp_bin_t *bin, const char *path,
                           const char *originname, bsp_exec_file_type_t type)
{
#define BOOT_MIN_SECTOR (0x4000)

    snprintf(bin->name, sizeof(bin->name), "%s", originname);
    snprintf(bin->path, sizeof(bin->path), "%s", path);
    bin->filetype = type;
    if (bsp_setup_bin_param(bin) < 0) {
        return NULL;
    }
    bin->progaddr = ROUND_DOWN(bin->entrypoint, BOOT_MIN_SECTOR);
    return bin;
}

static bsp_bin_t *
boot_alloc_bin_desc (const char *path,
                           const char *originname, bsp_exec_file_type_t type)
{
    bsp_bin_t *bin = (bsp_bin_t *)heap_malloc(sizeof(*bin));

    bin = bsp_setup_bin_desc(bin, path, originname, type);
    assert(bin && g_app_program_addr == bin->progaddr);
    boot_add_exec_2_list(bin);
    return bin;
}

void boot_read_path (const char *path)
{
    fobj_t fobj;
    int dir, bindir;
    char buf[BOOT_MAX_PATH];
    char origin_name[BOOT_MAX_NAME];
    d_bool filesfound = 0;

    dir = d_opendir(path);
    if (dir < 0) {
        dprintf("%s() : fail\n", __func__);
    }
    while (d_readdir(dir, &fobj) >= 0) {

        if (fobj.type == FTYPE_DIR) {
            fobj_t binobj;
            snprintf(buf, sizeof(buf), "%s/%s/"BOOT_BIN_DIR_NAME, path, fobj.name);

            bindir = d_opendir(buf);
            if (bindir < 0) {
                continue;
            }
            strcpy(origin_name, fobj.name);
            while (d_readdir(bindir, &binobj) >= 0) {
                if (binobj.type == FTYPE_FILE) {
                    bsp_exec_file_type_t type = bsp_bin_file_compat(binobj.name);

                    if (type != BIN_MAX) {
                        snprintf(buf, sizeof(buf), "%s/%s", buf, binobj.name);
                        if (boot_alloc_bin_desc(buf, origin_name, type)) {
                            filesfound++;
                        }
                    }
                }
            }
            if (!filesfound) {
                dprintf("%s() : no exe here : \'%s\'\n", __func__, buf);
            }
            d_closedir(bindir);
        }
    }
    d_closedir(dir);
    dprintf("%s() : Found : %u files\n", __func__, filesfound);
}

void *bsp_cache_bin_file (const bsp_heap_api_t *heapapi, const char *path, int *binsize)
{
    int f;
    int fsize, size;
    void *cache;

    fsize = d_open(path, &f, "r");
    if (f < 0) {
        dprintf("%s() : open fail : \'%s\'\n", __func__, path);
        return NULL;
    }
    size = ROUND_UP(fsize, 32);
    cache = heapapi->malloc(size);
    assert(cache);

    if (d_read(f, cache, fsize) < fsize) {
        dprintf("%s() : missing part\n", __func__);
        heap_free(cache);
        cache = NULL;
    } else {
        *binsize = fsize;
    }
    d_close(f);

    if (cache)
        dprintf("Cache done : <0x%p> : 0x%8x Kb\n", cache, fsize / 1024);

    return cache;
}

int bsp_install_exec (arch_word_t *progaddr, const char *path,
                          int argc, const char *argv)
{
    void *bindata;
    int binsize = 0, err = 0;
    bsp_heap_api_t heap = {.malloc = heap_alloc_shared, .free = heap_free};

    dprintf("Installing : \'%s\'\n", path);

    bindata = bsp_cache_bin_file(&heap, path, &binsize);
    if (!bindata) {
        return -1;
    }

    if (!boot_program_bypas && !bhal_prog_exist(progaddr, bindata, binsize / sizeof(arch_word_t))) {
        err = bhal_load_program(NULL, progaddr, bindata, binsize / sizeof(arch_word_t));
    }
    if (err < 0) {
        return -1;
    }
    return 0;
}

int bsp_start_exec (arch_word_t *progaddr, const char *path,
                          int argc, const char *argv)
{
    if (bsp_install_exec(progaddr, path, argc, argv) < 0) {
        return 0;
    }
    dprintf("Starting app... \n");

    dev_deinit();
    bhal_execute_app(progaddr);
    return 0;
}

static int b_handle_selected (pane_t *pane, component_t *com, void *user)
{
    bsp_bin_t *bindesc = *((bsp_bin_t **)com->user);

    if (bindesc == NULL) {
        gui_print(com_title, "Search result empty\n");
        return 0;
    }

    pane->parent->destroy = 1;
    g_app_program_addr = bindesc->progaddr;
    cmd_push("boot", bindesc->path, NULL, NULL);
    boot_destr_exec_list();
    com->user = NULL;
    return 1;
}

static int b_draw_exec_list (pane_t *pane, component_t *com, void *user)
{
    bsp_bin_t *bin = boot_bin_head;
    uint8_t maxbin = 8;
    uint8_t texty = 0;

    gui_com_clear(com);
    while (bin && maxbin) {

        gui_text(com, bin->name, 16, texty);
        bin = bin->next;
        texty += (com->dim.h - 64) / maxbin;
        maxbin--;
    }
    return 0;
}

static void boot_handle_input (gevt_t *evt)
{

    switch (evt->sym) {
        case 'e':
            gui_resp(&gui, com_browser, evt);
        break;

        default :
        break;
    }
    
}

static int gui_stdio_hook (const char *str, int len, char dir)
{
    if (pane_console && dir == '>') {
        win_con_append(pane_console, str, COLOR_WHITE);
    }
    return 0;
}

void boot_gui_preinit (void)
{
    
    prop_t prop;
    component_t *com;

    boot_gui_bsp_init(&gui);

    prop.bcolor = COLOR_BLUE;
    prop.fcolor = COLOR_WHITE;
    prop.ispad = d_true;
    prop.user_draw = d_false;

    pane = gui_get_pane(&gui, "pane", 0);
    gui_set_pane(&gui, pane);

    prop.ispad = d_false;
    prop.bcolor = COLOR_GREY;
    com = gui_get_comp(&gui, "title", NULL);
    gui_set_prop(com, &prop);
    gui_set_comp(pane, com, 0, 0, gui.dim.w, 120);
    com_title = com;

    prop.bcolor = COLOR_GREEN;
    com = gui_get_comp(&gui, "browser", NULL);
    gui_set_prop(com, &prop);
    gui_set_comp(pane, com, 0, 120, gui.dim.w, gui.dim.h - 120);
    com->draw = b_draw_exec_list;
    com->act = b_handle_selected;
    com->user = &boot_bin_selected;
    com_browser = com;

    prop.bcolor = COLOR_BLACK;
    pane_console = win_new_console(&gui, &prop, 0, 0, gui.dim.w, gui.dim.h);

    gui_select_pane(&gui, pane_console);

    inout_clbk = gui_stdio_hook;
}

int boot_main (int argc, const char **argv)
{
    boot_read_path("");
    cmd_register_i32(&boot_program_bypas, "skipflash");
    dprintf("Ready\n");

    while (!gui.destroy) {

        gui_draw(&gui);
        bsp_tickle();
        input_proc_keys(NULL);
        if (gui.destroy) {
            gui_destroy(&gui);
        }
    }
    return 0;
}

static i_event_t *__post_key (i_event_t  *evts, i_event_t *event)
{
    gevt_t evt;

    evt.p.x = event->x;
    evt.p.y = event->y;
    evt.e = event->state == keydown ? GUIACT : GUIRELEASE;
    evt.sym = event->sym;

    if (event->x == 0 && event->y == 0) {
        boot_handle_input(&evt);
        return NULL;
    }

    gui_resp(&gui, NULL, &evt);
    return NULL;
}

static void boot_gui_bsp_init (gui_t *gui)
{
    screen_t s;
    dim_t dim = {0};

    const kbdmap_t kbdmap[JOY_STD_MAX] =
    {  
        [JOY_UPARROW]       = {'u', 0},
        [JOY_DOWNARROW]     = {'d', 0},
        [JOY_LEFTARROW]     = {'l',0},
        [JOY_RIGHTARROW]    = {'r', 0},
        [JOY_K1]            = {'1', PAD_FREQ_LOW},
        [JOY_K4]            = {'2',  0},
        [JOY_K3]            = {'3', 0},
        [JOY_K2]            = {'4',PAD_FREQ_LOW},
        [JOY_K5]            = {'5',    0},
        [JOY_K6]            = {'6',    0},
        [JOY_K7]            = {'7',  0},
        [JOY_K8]            = {'8', 0},
        [JOY_K9]            = {'e', 0},
        [JOY_K10]           = {'x', PAD_FREQ_LOW},
    };

    gui_bsp_api_t bspapi =
    {
        .mem =
            {
                .alloc = heap_alloc_shared,
                .free = heap_free,
            },
        .sfx =
            {
                .alloc = NULL,
                .release = NULL,
                .play = NULL,
                .stop = NULL,
            },
    };

    vid_wh(&s);
    dim.w = s.width;
    dim.h = s.height;

    gui_init(gui, "gui", 25, &dim, &bspapi);
    input_soft_init(__post_key, kbdmap);
}

#endif /*BOOT*/
