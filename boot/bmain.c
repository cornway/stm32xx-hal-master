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
#include <misc_utils.h>

#define BOOT_SYS_DIR_PATH "/sys"
#define BOOT_SYS_LOG_NAME "log.txt"
#define BOOT_BIN_DIR_NAME "BIN"
#define BOOT_SYS_LOG_PATH BOOT_SYS_DIR_PATH"/"BOOT_SYS_LOG_NAME

static int boot_program_bypas = 0;

component_t *com_console = NULL;
gui_t gui;
pane_t *pane, *alert_pane;
pane_t *pane_console, *pane_selector, *pane_alert, *pane_progress;

bsp_bin_t *boot_bin_head = NULL;
int bin_collected_cnt = 0;
bsp_bin_t **boot_bin_packed_array = NULL;
int boot_bin_packed_size = 0;
int boot_bin_selected = 0;

static void boot_gui_bsp_init (gui_t *gui);
static int gui_stdout_hook (int argc, const char **argv);

bsp_exec_file_type_t
bsp_bin_file_compat (const char *in)
{
    const struct exec_file_ext_map {
        bsp_exec_file_type_t type;
        const char ext[5];
    } extmap[] =
    {
        {BIN_FILE, ".bin"},
        {BIN_LINK, ".als"},
        {BIN_LINK, ".lnk"},
        {BIN_CMD,  ".cmd"},
    };
    int i;

    for (i = 0; i < arrlen(extmap); i++) {
        if (!d_astrnmatch(in, extmap[i].ext, 1 - sizeof(extmap[i].ext))) {
            return extmap[i].type;
        }
    }
    return BIN_MAX;
}

static void boot_bin_link (bsp_bin_t *bin)
{
    bin->next = boot_bin_head;
    boot_bin_head = bin;
    bin_collected_cnt++;
}

static void boot_bin_unlink (bsp_bin_t *del)
{
    bsp_bin_t *bin = boot_bin_head, *prev = NULL;

    while (bin) {

        if (del == bin) {
            if (prev) {
                prev->next = bin->next;
            } else {
                boot_bin_head = bin->next;
            }
            bin_collected_cnt--;
            heap_free(bin);
            break;
        }
        prev = bin;
        bin = bin->next;
    }
}

static void boot_destr_exec_list (void)
{
    bsp_bin_t *bin = boot_bin_head, *prev;

    while (bin) {
        prev = bin;
        bin = bin->next;
        heap_free(prev);
    }
    boot_bin_head = NULL;
    bin_collected_cnt = 0;
    if (boot_bin_packed_array) {
        heap_free(boot_bin_packed_array);
    }
}

int
bsp_setup_bin_param (bsp_bin_t *bin)
{
    int f, size;
    uint8_t buf[sizeof(bin->parm)];

    size = d_open(bin->path, &f, "r");
    if (f < 0) {
        return -1;
    }
    d_read(f, buf, sizeof(buf));
    b_setup_bin_param(&bin->parm, buf, size);
    d_close(f);
    return 0;
}

bsp_bin_t *
bsp_setup_bin_desc (bsp_bin_t *bin, const char *path,
                           const char *originname, bsp_exec_file_type_t type)
{
    snprintf(bin->name, sizeof(bin->name), "%s", originname);
    snprintf(bin->path, sizeof(bin->path), "%s", path);
    bin->filetype = type;
    if (bsp_setup_bin_param(bin) < 0) {
        return NULL;
    }
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
    boot_bin_link(bin);
    return bin;
}

static void boot_pack_bin_list (bsp_bin_t *head, int cnt)
{
    bsp_bin_t *bin = head;
    boot_bin_packed_array = heap_malloc(sizeof(bsp_bin_t *) * (cnt + 1));
    assert(boot_bin_packed_array);
    boot_bin_packed_size = 0;

    while (bin) {
        boot_bin_packed_array[boot_bin_packed_size] = bin;
        bin->id = boot_bin_packed_size;
        bin = bin->next;
        boot_bin_packed_size++;
    }
}

static int b_rebuild_exec_list (bsp_bin_t *head, int cnt)
{
    if (boot_bin_packed_array) {
        heap_free(boot_bin_packed_array);
    }
    boot_pack_bin_list(head, cnt);
    return CMDERR_OK;
}

static void boot_bin_select_next (int dir)
{
    if (dir < 0) {
        boot_bin_selected--;
        if (boot_bin_selected < 0) {
            boot_bin_selected = boot_bin_packed_size - 1;
        }
    } else {
        boot_bin_selected++;
        if (boot_bin_selected == boot_bin_packed_size) {
            boot_bin_selected = 0;
        }
    }
}

static void
boot_bin_get_visible (const bsp_bin_t **binarray, int *pstart,
                             int *size, int maxsize)
{
    int tmp, top, bottom, start, i;
    int mid = maxsize / 2;
    assert(boot_bin_packed_size);

    top = (boot_bin_packed_size - boot_bin_selected);
    if (top > mid) {
        top = mid;
    }
    bottom = boot_bin_selected;
    if (bottom > mid) {
        bottom = mid;
    }
    tmp = top + bottom;
    start = boot_bin_selected - bottom;

    *size = tmp;
    *pstart = mid - bottom;
    while (i < tmp) {
        binarray[i++] = boot_bin_packed_array[start++];
    }
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

                    if (type != BIN_MAX && type != BIN_FILE) {
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
static int b_handle_cmd (const char *path);

int bsp_exec_link (arch_word_t *progaddr, const char *path)
{
    return b_handle_lnk(path);
}

int bsp_exec_cmd (arch_word_t *progaddr, const char *path)
{
    return b_handle_cmd(path);
}

static void __install_status_clbk (const char *msg, int percent)
{
    win_prog_set(pane_progress, msg, percent);
    HAL_Delay(10);
    gui_draw(&gui);
}

arch_word_t *bsp_install_exec (arch_word_t *progptr, const char *path)
{
    void *bindata;
    int binsize = 0, err = 0;
    bsp_heap_api_t heap = {.malloc = heap_alloc_shared, .free = heap_free};
    boot_bin_parm_t parm;

    dprintf("Installing : \'%s\'\n", path);

    bindata = bsp_cache_bin_file(&heap, path, &binsize);
    if (!bindata) {
        return NULL;
    }
    if (progptr == NULL) {
        b_setup_bin_param(&parm, bindata, binsize);
        progptr = (arch_word_t *)parm.progaddr;
    }

    if (!boot_program_bypas && !bhal_prog_exist(__install_status_clbk, progptr, bindata, binsize / sizeof(arch_word_t))) {
        err = bhal_load_program(__install_status_clbk, progptr, bindata, binsize / sizeof(arch_word_t));
    }
    if (err < 0) {
        return NULL;
    }
    return progptr;
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
    char strbuf[CMD_MAX_BUF];

    d_open(path, &f, "r");
    if (f < 0) {
        dprintf("%s() : fail\n", __func__);
        return -CMDERR_NOPATH;
    }
    if (!d_gets(f, strbuf, sizeof(strbuf))) {
        return -CMDERR_NOCORE;
    }
    d_close(f);
    cmd_exec_dsr("bin", strbuf, NULL, NULL);
    return CMDERR_OK;
}

static int b_handle_cmd (const char *path)
{
    cmd_exec_dsr("runcmd", path, NULL, NULL);
    return CMDERR_OK;
}

static int __b_exec_selected (bsp_bin_t *bin)
{
    int ret = -CMDERR_INVPARM;
    switch (bin->filetype) {
        case BIN_FILE: ret = cmd_exec_dsr("boot", bin->path, NULL, NULL);
                       
        break;
        case BIN_LINK: ret = b_handle_lnk(bin->path);
        break;
        case BIN_CMD: ret = b_handle_cmd(bin->path);
        break;
        default:
        assert(0);
    }
    return ret;
}

static int b_gui_print_bin_list (pane_t *pane)
{
    const int prsize = 20;
    const bsp_bin_t *binarray[32];
    int start, selected, maxy, size, i, dummy;
    char str[CMD_MAX_BUF];

    win_con_clear(pane);
    win_con_get_dim(pane, &dummy, &maxy);

    assert(arrlen(binarray) > maxy);
    boot_bin_get_visible(binarray, &start, &size, maxy);

    selected = maxy / 2;
    assert(selected >= start);

    for (i = 0; i < start; i++) {
        win_con_printline(pane, i, ".", COLOR_WHITE);
    }
    for (i = start + size; i < maxy; i++) {
        win_con_printline(pane, i, ".", COLOR_WHITE);
    }
    for (i = start; i < start + size; i++) {
        if (i == selected) {
            snprintf(str, sizeof(str), "%*.*s <<<", -prsize, -prsize, binarray[i - start]->name);
            win_con_printline(pane, i, str, COLOR_YELLOW);
        } else {
            snprintf(str, sizeof(str), "%*.*s", -prsize, -prsize, binarray[i - start]->name);
            win_con_printline(pane, i, str, COLOR_WHITE);
        }
    }
    return CMDERR_OK;
}

static int b_exec_selected (pane_t *pane, void *data, int size)
{
    gui_select_pane(&gui, pane_progress);
    return __b_exec_selected(boot_bin_packed_array[boot_bin_selected]);
}

static int b_handle_selected (pane_t *pane, component_t *com, void *user)
{
    gevt_t *evt = (gevt_t *)user;
    d_bool refresh = d_false, wakeup = d_true;

    switch (evt->sym) {
        case 'u': boot_bin_select_next(1);
                  refresh = d_true;
        break;
        case 'd': boot_bin_select_next(-1);
                  refresh = d_true;
        break;
        case 'e':
        {
            char buf[CMD_MAX_BUF];

            snprintf(buf, sizeof(buf), "Open :\n\'%s\'\nApplicarion?",
                     boot_bin_packed_array[boot_bin_selected]->name);
            WIN_ALERT_ACCEPT(&gui, buf, b_exec_selected);
        }
        break;
        case '1': /*Alert*/
        break;
        default: wakeup = d_false;
    }
    if (wakeup) {
        gui_wakeup_pane(pane);
    }
    if (refresh) {
        b_gui_print_bin_list(pane);
    }
    return 1;
}

static void boot_handle_input (gevt_t *evt)
{
    if (evt->e != GUIACT) {
        return;
    }

    switch (evt->sym) {
        case 'x':
            gui_select_next_pane(&gui);
        break;
        case 'u':
        case 'd':
        case 'l':
        case 'r':
            if (gui.selected == pane_alert) {
                gui_set_next_focus(&gui);
                break;
            }
        case 'e':
            if (gui.selected == pane_alert) {
                gui_resp(&gui, NULL, evt);
                break;
            }
        default:
            if (gui.selected != pane_alert) {
                gui_resp(&gui, NULL, evt);
            }
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
    dim_t dim;
    prop_t prop = {0};

    boot_gui_bsp_init(&gui);

    prop.fcolor = COLOR_WHITE;
    prop.ispad = d_false;
    prop.user_draw = d_false;

    prop.bcolor = COLOR_BLACK;
    prop.name = "console";
    prop.fontprop.font = gui_get_font_4_size(&gui, 16, 1);
    pane_console = win_new_console(&gui, &prop, 0, 0, gui.dim.w, gui.dim.h);
    gui_select_pane(&gui, pane_console);

    prop.fontprop.font = NULL;
    prop.bcolor = COLOR_BLACK;
    prop.name = "binselect";
    pane_selector = win_new_console(&gui, &prop, 0, 0, gui.dim.w, gui.dim.h);
    win_set_act_clbk(pane_selector, b_handle_selected);

    pane_alert = win_new_allert(&gui, 400, 300);

    dim = pane_alert->dim;
    dim_set_top(&dim, &dim);
    dim.h = 64;
    dim.y += 10;
    prop.name = "progbar1";
    prop.fcolor = COLOR_BLACK;
    prop.bcolor = COLOR_WHITE;
    prop.fontprop.font = gui_get_font_4_size(&gui, 24, 1);
    pane_progress = win_new_progress(&gui, &prop, dim.x, dim.y, dim.w, dim.h);

    bsp_stdout_register_if(gui_stdout_hook);
}

int boot_main (int argc, const char **argv)
{
    cd_track_t cd;

    boot_read_path("");
    boot_pack_bin_list(boot_bin_head, bin_collected_cnt);
    b_gui_print_bin_list(pane_selector);
    cmd_register_i32(&boot_program_bypas, "skipflash");
    dprintf("Ready\n");

    cd_play_name(&cd, "/doom/music/psx/song_22.wav");

    while (!gui.destroy) {

        gui_draw(&gui);
        input_proc_keys(NULL);
        bsp_tickle();
    }
    return 0;
}

static uint32_t inpost_tsf = 0;

static i_event_t *__post_key (i_event_t  *evts, i_event_t *event)
{
    gevt_t evt;

    if (event->state == keydown) {
        if (d_rlimit_wrap(&inpost_tsf, 300) == 0) {
            return NULL;
        }
    }

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
        [JOY_LEFTARROW]     = {'l', 0},
        [JOY_RIGHTARROW]    = {'r', 0},
        [JOY_K1]            = {'1', PAD_FREQ_LOW},
        [JOY_K4]            = {'2', 0},
        [JOY_K3]            = {'3', 0},
        [JOY_K2]            = {'4', PAD_FREQ_LOW},
        [JOY_K5]            = {'5', 0},
        [JOY_K6]            = {'6', 0},
        [JOY_K7]            = {'7', 0},
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
                .alloc = bsp_open_wave_sfx,
                .release = bsp_release_wave_sfx,
                .play = bsp_play_wave_sfx,
                .stop = bsp_stop_wave_sfx,
            },
    };

    vid_wh(&s);
    dim.w = s.width;
    dim.h = s.height;

    gui_init(gui, "gui", 25, &dim, &bspapi);
    input_soft_init(__post_key, kbdmap);
}
