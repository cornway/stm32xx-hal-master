#include <string.h>
#include <debug.h>
#include "int/boot_int.h"
#include "../int/term_int.h"
#include <gfx.h>
#include <gui.h>
#include <dev_io.h>
#include <nvic.h>
#include <input_main.h>
#include <lcd_main.h>
#include <heap.h>
#include <bsp_sys.h>
#include <bsp_cmd.h>
#include <audio_main.h>

#define BOOT_SYS_DIR_PATH "/sys"
#define BOOT_SYS_LOG_NAME "log.txt"
#define BOOT_BIN_DIR_NAME "BIN"
#define BOOT_SYS_LOG_PATH BOOT_SYS_DIR_PATH"/"BOOT_SYS_LOG_NAME

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
static int gui_stdout_hook (int argc, const char **argv);

bsp_exec_file_type_t
bsp_bin_file_compat (const char *in)
{
#define EXT_LEN (4)

    const struct exec_file_ext_map {
        bsp_exec_file_type_t type;
        const char *ext;
    } extmap[] =
    {
        {BIN_FILE, ".bin"},
        {BIN_LINK, ".als"},
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

static int
__bsp_setup_bin_param (boot_bin_parm_t *parm, void *ptr, int size)
{
    arch_word_t *bindata = (arch_word_t *)ptr;
    parm->entrypoint = bindata[1];
    parm->progaddr = bindata[1];
    parm->spinitial = bindata[0];
    parm->size = size;
    return 0;
}

int
bsp_setup_bin_param (bsp_bin_t *bin)
{
    int f, size;
    arch_word_t entry;
    uint8_t buf[sizeof(bin->parm)];

    size = d_open(bin->path, &f, "r");
    if (f < 0) {
        return -1;
    }
    d_read(f, buf, sizeof(buf));
    __bsp_setup_bin_param(&bin->parm, buf, size);
    d_close(f);
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
    bin->parm.progaddr = ROUND_DOWN(bin->parm.entrypoint, BOOT_MIN_SECTOR);
    return bin;
}

bsp_bin_t *
bsp_setup_bin_link (bsp_bin_t *bin, const char *path,
                           const char *originname, bsp_exec_file_type_t type)
{
    snprintf(bin->name, sizeof(bin->name), "%s", originname);
    snprintf(bin->path, sizeof(bin->path), "%s", path);
    bin->filetype = type;
    bin->parm.progaddr = 0;
    return bin;
}


static bsp_bin_t *
boot_alloc_bin_desc (const char *path,
                           const char *originname, bsp_exec_file_type_t type)
{
    bsp_bin_t *bin = (bsp_bin_t *)heap_malloc(sizeof(*bin));

    if (type == BIN_FILE) {
        bin = bsp_setup_bin_desc(bin, path, originname, type);
        assert(bin);
    } else if (type == BIN_LINK) {
        bin = bsp_setup_bin_link(bin, path, originname, type);
    }
    boot_add_exec_2_list(bin);
    return bin;
}

void boot_read_path (const char *path)
{
    fobj_t fobj;
    int dir, bindir;
    char buf[BOOT_MAX_PATH];
    char binpath[BOOT_MAX_NAME];
    d_bool filesfound = 0;

    dir = d_opendir(path);
    if (dir < 0) {
        dprintf("%s() : fail\n", __func__);
    }
    while (d_readdir(dir, &fobj) >= 0) {

        if (fobj.attr.dir) {
            fobj_t binobj;
            snprintf(buf, sizeof(buf), "%s/%s/"BOOT_BIN_DIR_NAME, path, fobj.name);

            bindir = d_opendir(buf);
            if (bindir < 0) {
                continue;
            }
            while (d_readdir(bindir, &binobj) >= 0) {
                if (binobj.attr.dir == 0) {
                    bsp_exec_file_type_t type = bsp_bin_file_compat(binobj.name);

                    if (type != BIN_MAX) {
                        snprintf(binpath, sizeof(binpath), "%s/%s", buf, binobj.name);
                        if (boot_alloc_bin_desc(binpath, binobj.name, type)) {
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
        dprintf("%s() : failed to open : \'%s\'\n", __func__, path);
        return NULL;
    }
    size = ROUND_UP(fsize, 32);
    cache = heapapi->malloc(size);
    assert(cache);

    dprintf("caching bin : dest <0x%p> size [0x%08x]\n", cache, fsize);

    if (d_read(f, cache, fsize) < fsize) {
        dprintf("%s() : missing part\n", __func__);
        heapapi->free(cache);
        cache = NULL;
    } else {
        *binsize = fsize;
    }
    d_close(f);

    if (cache)
        dprintf("Cache done : <0x%p> : 0x%08x bytes\n", cache, fsize);

    return cache;
}

static int b_handle_lnk (const char *path);

int bsp_exec_link (arch_word_t *progaddr, const char *path)
{
    return b_handle_lnk(path);
}

int bsp_install_exec (arch_word_t *progptr, const char *path)
{
    void *bindata;
    int binsize = 0, err = 0;
    bsp_heap_api_t heap = {.malloc = heap_alloc_shared, .free = heap_free};
    boot_bin_parm_t parm;

    dprintf("Installing : \'%s\'\n", path);

    bindata = bsp_cache_bin_file(&heap, path, &binsize);
    if (!bindata) {
        return -1;
    }
    if (progptr == NULL) {
        __bsp_setup_bin_param(&parm, bindata, binsize);
        progptr = (arch_word_t *)parm.progaddr;
    }

    if (!boot_program_bypas && !bhal_prog_exist(progptr, bindata, binsize / sizeof(arch_word_t))) {
        err = bhal_load_program(NULL, progptr, bindata, binsize / sizeof(arch_word_t));
    }
    if (err < 0) {
        return -1;
    }
    return 0;
}

extern void bsp_argc_argv_set (const char *arg);
extern int bsp_argc_argv_check (const char *arg);

int bsp_start_exec (arch_word_t *progaddr, int argc, const char **argv)
{
    dprintf("Starting application... \n");

    bsp_argc_argv_set(argv[0]);
    if (bsp_argc_argv_check(argv[0]) < 0) {
        return -1;
    }
    bsp_stout_unreg_if(gui_stdout_hook);
    gui_destroy(&gui);
    dev_deinit();
    bhal_execute_app(progaddr);
    return 0;
}

static int b_handle_lnk (const char *path)
{
    int f;
    char strbuf[256];

    d_open(path, &f, "r");
    if (f < 0) {
        dprintf("%s() : fail\n", __func__);
        return -1;
    }
    if (!d_gets(f, strbuf, sizeof(strbuf))) {
        return -1;
    }
    d_close(f);
    cmd_exec_dsr("bsp", strbuf, NULL, NULL);
    return 0;
}

static int b_handle_selected (pane_t *pane, component_t *com, void *user)
{
    bsp_bin_t *bindesc = *((bsp_bin_t **)com->user);

    if (bindesc == NULL) {
        gui_print(com_title, "Search result empty\n");
        return 0;
    }

    if (bindesc->filetype == BIN_FILE) {
        cmd_exec_dsr("boot", bindesc->path, NULL, NULL);
    } else if (bindesc->filetype == BIN_LINK) {
        assert(b_handle_lnk(bindesc->path));
    }
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

static int gui_stdout_hook (int argc, const char **argv)
{
    assert(argc > 0);
    win_con_append(pane_console, argv[0], COLOR_WHITE);
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

    bsp_stdout_register_if(gui_stdout_hook);
}

int boot_main (int argc, const char **argv)
{
    cd_track_t cd;

    boot_read_path("");
    cmd_register_i32(&boot_program_bypas, "skipflash");
    dprintf("Ready\n");

    cd_play_name(&cd, "/doom/music/psx/PSXCRDTS.wav");

    while (!gui.destroy) {

        gui_draw(&gui);
        input_proc_keys(NULL);
        bsp_tickle();
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
