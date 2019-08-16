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
#define BOOT_SFX_DIR_PATH (BOOT_SYS_DIR_PATH"/sfx")
#define BOOT_SYS_LOG_NAME "log.txt"
#define BOOT_BIN_DIR_NAME "BIN"
#define BOOT_SYS_LOG_PATH BOOT_SYS_DIR_PATH"/"BOOT_SYS_LOG_NAME
#define BOOT_STARTUP_MUSIC_PATH (BOOT_SYS_DIR_PATH"/mus/title.wav")

static int boot_program_bypas = 0;

static gui_t gui;
static pane_t *pane_console, *pane_selector,
              *pane_alert, *pane_progress,
              *pane_jpeg;

static bsp_bin_t *boot_bin_head = NULL;
static int bin_collected_cnt = 0;
static bsp_bin_t **boot_bin_packed_array = NULL;
static int boot_bin_packed_size = 0;
static int boot_bin_selected = 0;

static cd_track_t boot_cd = {NULL};

enum {
    SFX_MOVE,
    SFX_SCROLL,
    SFX_WARNING,
    SFX_CONFIRM,
    SFX_START_APP,
    SFX_NOWAY,
    SFX_MAX,
};

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

static void boot_gui_bsp_init (gui_t *gui);
static int gui_stdout_hook (int argc, const char **argv);

static void boot_sfx_chart_alloc (gui_t *gui)
{
    int i;
    char path[BOOT_MAX_PATH];

    for (i = 0; i < arrlen(sfx_names); i++) {
        snprintf(path, sizeof(path), "%s/%s", BOOT_SFX_DIR_PATH, sfx_names[i]);
        sfx_ids[i] = gui_bsp_sfxalloc(gui, path);
    }
}

int boot_print_bin_list (int argc, const char **argv)
{
    int i = 0;
    bsp_bin_t *bin = boot_bin_head;
    boot_bin_parm_t *parm;

    dprintf("%s() :\n", __func__);
    while (bin) {
        parm = &bin->parm;

        if (bin->filetype == BIN_FILE) {
            dprintf("[%i] %s, %s, <0x%p> %u bytes\n",
                i++, bin->name, bin->path, (void *)parm->progaddr, parm->size);
        } else if (bin->filetype == BIN_LINK) {
            dprintf("[%i] %s, %s, <link file>\n",
                i++, bin->name, bin->path);
        } else {
            dprintf("unable to handle : [%i] %s, %s, ???\n",
                i++, bin->name, bin->path);
            assert(0)
        }
        bin = bin->next;
    }
    return argc;
}

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

static void
bsp_setup_title_pic (const char *dirpath, bsp_bin_t *bin)
{
    char name[D_MAX_NAME] = {0};
    char tpath[D_MAX_PATH];
    const char *argv[2] = {0};

    strcpy(name, bin->name);
    d_vstrtok(argv, 2, name, '.');

    snprintf(tpath, sizeof(tpath), "%s/%s.jpg", dirpath, argv[0]);
    bin->pic = win_jpeg_decode(pane_jpeg, tpath);
}

bsp_bin_t *
bsp_setup_bin_desc (const char *dirpath, bsp_bin_t *bin, const char *path,
                           const char *originname, bsp_exec_file_type_t type)
{
    snprintf(bin->name, sizeof(bin->name), "%s", originname);
    snprintf(bin->path, sizeof(bin->path), "%s", path);
    bin->filetype = type;
    bsp_setup_title_pic(dirpath, bin);
    if (bsp_setup_bin_param(bin) < 0) {
        return NULL;
    }
    return bin;
}

bsp_bin_t *
bsp_setup_bin_link (const char *dirpath, bsp_bin_t *bin, const char *path,
                           const char *originname, bsp_exec_file_type_t type)
{
    snprintf(bin->name, sizeof(bin->name), "%s", originname);
    snprintf(bin->path, sizeof(bin->path), "%s", path);
    bin->filetype = type;
    bin->parm.progaddr = 0;
    bsp_setup_title_pic(dirpath, bin);
    return bin;
}


static bsp_bin_t *
boot_alloc_bin_desc (const char *dirpath, const char *path,
                           const char *originname, bsp_exec_file_type_t type)
{
    bsp_bin_t *bin = (bsp_bin_t *)heap_malloc(sizeof(*bin));

    if (type == BIN_FILE) {
        bin = bsp_setup_bin_desc(dirpath, bin, path, originname, type);
        assert(bin);
    } else if (type == BIN_LINK) {
        bin = bsp_setup_bin_link(dirpath, bin, path, originname, type);
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
                        if (boot_alloc_bin_desc(buf, binpath, binobj.name, type)) {
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
    if (win_prog_set(pane_progress, msg, percent)) {
        gui_draw(&gui, 1);
        HAL_Delay(10);
    }
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
    cmd_exec_dsr("bin", strbuf);
    return CMDERR_OK;
}

static int b_handle_cmd (const char *path)
{
    cmd_exec_dsr("runcmd", path);
    return CMDERR_OK;
}

static int __b_exec_selected (bsp_bin_t *bin)
{
    int ret = -CMDERR_INVPARM;
    switch (bin->filetype) {
        case BIN_FILE: ret = cmd_exec_dsr("boot", bin->path);
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
        win_con_printline(pane, i, "    *", COLOR_BLACK);
    }
    for (i = start + size; i < maxy; i++) {
        win_con_printline(pane, i, "    *", COLOR_BLACK);
    }
    for (i = start; i < start + size; i++) {
        if (i == selected) {
            snprintf(str, sizeof(str), "%*.*s <---", -prsize, -prsize, binarray[i - start]->name);
            win_con_printline(pane, i, str, COLOR_RED);
        } else {
            snprintf(str, sizeof(str), "%*.*s", -prsize, -prsize, binarray[i - start]->name);
            win_con_printline(pane, i, str, COLOR_BLACK);
        }
    }
    win_jpeg_set_rawpic(pane_jpeg, binarray[selected - start]->pic, 1);
    return CMDERR_OK;
}

static int b_console_user_clbk (gevt_t *evt)
{
    if (evt) {
        gui_bsp_sfxplay(&gui, sfx_ids[SFX_MOVE], 100);
        return 1;
    }
    return 0;
}

static int b_alert_user_clbk (gevt_t *evt)
{
    if (evt->sym == GUI_KEY_LEFT ||
        evt->sym == GUI_KEY_RIGHT) {
        gui_bsp_sfxplay(&gui, sfx_ids[SFX_MOVE], 100);
    }
    return 1;
}

static int
b_alert_decline_clbk (const component_t *com)
{
    gui_bsp_sfxplay(&gui, sfx_ids[SFX_NOWAY], 100);
    return 1;
}

static int b_alert_accept_clbk (const component_t *com)
{
    int ret;

    cd_stop(&boot_cd);
    gui_bsp_sfxplay(&gui, sfx_ids[SFX_START_APP], 100);
    d_sleep(100);

    gui_select_pane(&gui, pane_progress);

    ret = __b_exec_selected(boot_bin_packed_array[boot_bin_selected]);
    if (ret != CMDERR_OK) {
        cd_play_name(&boot_cd, BOOT_STARTUP_MUSIC_PATH);
    } else {
        gui.destroy = 1;
    }
    return ret;
}

static int
b_selected_clbk (pane_t *pane, component_t *com, void *user)
{
    gevt_t *evt = (gevt_t *)user;
    int dir = -1;

    switch (evt->sym) {
        case GUI_KEY_UP:
            dir = 1;
        case GUI_KEY_DOWN:
            boot_bin_select_next(dir);
            b_gui_print_bin_list(pane);
            gui_bsp_sfxplay(&gui, sfx_ids[SFX_MOVE], 100);
        break;
        case GUI_KEY_RETURN:
        {
            char buf[CMD_MAX_BUF];

            gui_bsp_sfxplay(&gui, sfx_ids[SFX_WARNING], 100);
            snprintf(buf, sizeof(buf), "Open :\n\'%s\'\nApplicarion?",
                     boot_bin_packed_array[boot_bin_selected]->name);
            win_alert(pane_alert, buf, b_alert_accept_clbk, b_alert_decline_clbk);
        }
        break;
    }
    return 1;
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
    pane_console = win_new_console(&gui, &prop, gui.dim.x, gui.dim.y, gui.dim.w, gui.dim.h);
    gui_select_pane(&gui, pane_console);
    win_set_user_clbk(pane_console, b_console_user_clbk);

    prop.fontprop.font = NULL;
    prop.bcolor = COLOR_AQUAMARINE;
    prop.name = "binselect";
    pane_selector = win_new_console(&gui, &prop, gui.dim.x, gui.dim.y, gui.dim.w / 2, gui.dim.h);
    win_con_set_clbk(pane_selector, b_selected_clbk);

    pane_alert = win_new_allert(&gui, 400, 300);
    win_set_user_clbk(pane_alert, b_alert_user_clbk);

    dim = pane_alert->dim;
    dim_set_top(&dim, &dim);
    dim.h = 64;
    dim.y += 10;
    prop.name = "progbar1";
    prop.fcolor = COLOR_BLACK;
    prop.bcolor = COLOR_WHITE;
    prop.fontprop.font = gui_get_font_4_size(&gui, 24, 1);
    pane_progress = win_new_progress(&gui, &prop, dim.x, dim.y, dim.w, dim.h);

    prop.name = "pane_jpeg";
    pane_jpeg = win_new_jpeg(&gui, &prop, gui.dim.x + (gui.dim.w / 2),
                                          gui.dim.y, gui.dim.w / 2, gui.dim.h);
    gui_set_child(pane_selector, pane_jpeg);

    bsp_stdout_register_if(gui_stdout_hook);

    gui_select_pane(&gui, pane_console);
}

int boot_main (int argc, const char **argv)
{
    boot_read_path("");
    boot_pack_bin_list(boot_bin_head, bin_collected_cnt);
    b_gui_print_bin_list(pane_selector);
    cmd_register_i32(&boot_program_bypas, "skipflash");
    boot_sfx_chart_alloc(&gui);
    dprintf("Ready\n");

    cd_play_name(&boot_cd, BOOT_STARTUP_MUSIC_PATH);
    cd_volume(&boot_cd, 40);

    while (!gui.destroy) {
        bsp_tickle();
        gui_draw(&gui, 0);
        input_proc_keys(NULL);
    }
    boot_destr_exec_list();
    bsp_tickle();
    assert(0);
    return 0;
}

static void _gui_symbolic_event_wrap (gui_t *gui, gevt_t *evt)
{
    if (evt->e != GUIACT) {
        return;
    }

    switch (evt->sym) {
        case GUI_KEY_SELECT:
            gui_bsp_sfxplay(gui, sfx_ids[SFX_SCROLL], 100);
            gui_select_next_pane(gui);
        break;
        default:
            gui_send_event(gui, evt);
        break;
    }
}

static i_event_t *__post_key (i_event_t  *evts, i_event_t *event)
{
    static uint32_t inpost_tsf = 0;
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
    evt.symbolic = 0;

    if (event->x == 0 && event->y == 0) {
        evt.symbolic = 1;
    }

    if (evt.symbolic) {
        _gui_symbolic_event_wrap(&gui, &evt);
    } else {
        gui_send_event(&gui, &evt);
    }
    return NULL;
}

static void boot_gui_bsp_init (gui_t *gui)
{
    screen_t s = {0};
    dim_t dim = {0};

    const kbdmap_t kbdmap[JOY_STD_MAX] =
    {  
        [JOY_UPARROW]       = {GUI_KEY_UP, 0},
        [JOY_DOWNARROW]     = {GUI_KEY_DOWN, 0},
        [JOY_LEFTARROW]     = {GUI_KEY_LEFT, 0},
        [JOY_RIGHTARROW]    = {GUI_KEY_RIGHT, 0},
        [JOY_K1]            = {GUI_KEY_1, PAD_FREQ_LOW},
        [JOY_K4]            = {GUI_KEY_2, 0},
        [JOY_K3]            = {GUI_KEY_3, 0},
        [JOY_K2]            = {GUI_KEY_4, PAD_FREQ_LOW},
        [JOY_K5]            = {GUI_KEY_5, 0},
        [JOY_K6]            = {GUI_KEY_6, 0},
        [JOY_K7]            = {GUI_KEY_7, 0},
        [JOY_K8]            = {GUI_KEY_8, 0},
        [JOY_K9]            = {GUI_KEY_RETURN, 0},
        [JOY_K10]           = {GUI_KEY_SELECT, PAD_FREQ_LOW},
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
