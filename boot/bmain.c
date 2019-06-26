#include <gfx.h>
#include <boot_int.h>
#include <dev_io.h>
#include <debug.h>
#include <nvic.h>
#include <input_main.h>
#include <lcd_main.h>
#include <string.h>
#include <heap.h>
#include <bsp_sys.h>


#define BOOT_SYS_DIR_PATH "/sys"
#define BOOT_SYS_LOG_NAME "log.txt"
#define BOOT_SYS_LOG_PATH BOOT_SYS_DIR_PATH"/"BOOT_SYS_LOG_NAME

#define BOOT_MAX_NAME 24
#define BOOT_MAX_PATH 128

extern uint32_t g_app_program_addr;

static int boot_load_existing = 0;

component_t *com_browser;
component_t *com_title;
gui_t gui;
pane_t *pane, *alert_pane;

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

    arch_word_t progaddr;
    char name[BOOT_MAX_NAME];
    char dirpath[BOOT_MAX_PATH];
    char path[BOOT_MAX_PATH];
} boot_bin_t;

typedef struct {
    /*Must be pointer to a static*/
    const char *cmd;
    const char *text;
    void *user1, *user2;
} boot_cmd_t;

typebind_t typebind[] =
{
    {BIN_FILE, ".bin"},
    {BIN_LINK, ".lnk"},
};


boot_cmd_t boot_cmd_pool[6];
boot_cmd_t *boot_cmd_top = &boot_cmd_pool[0];


static inline void boot_cmd_push (const char *cmd, const char *text, void *user1, void *user2)
{
    if (boot_cmd_top == &boot_cmd_pool[arrlen(boot_cmd_pool)]) {
        return;
    }
    boot_cmd_top->cmd = cmd;
    boot_cmd_top->text = text;
    boot_cmd_top->user1 = user1;
    boot_cmd_top->user2 = user2;
    boot_cmd_top++;
    dprintf("%s() : \'%s\' [%s]\n", __func__, cmd, text);
}

boot_cmd_t *boot_cmd_pop (boot_cmd_t *cmd)
{
    if (boot_cmd_top == &boot_cmd_pool[0]) {
        return NULL;
    }
    boot_cmd_top--;
    cmd->cmd = boot_cmd_top->cmd;
    cmd->text = boot_cmd_top->text;
    cmd->user1 = boot_cmd_top->user1;
    cmd->user2 = boot_cmd_top->user2;
    return cmd;
}


boot_bin_t *boot_bin_head = NULL;
boot_bin_t *boot_bin_selected = NULL;

static void boot_bin_link (boot_bin_t *bin)
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

static inline bintype_t
boot_check_bin_compat (const char *in)
{
#define EXT_LEN (4)
    int pos = strlen(in) - EXT_LEN;
    int i;

    for (i = 0; i < arrlen(typebind); i++) {
        if (!strcasecmp(in + pos, typebind->ext)) {
            return typebind->type;
        }
    }
    return BIN_NONE;
}

arch_word_t
boot_get_entry_addr (boot_bin_t *bin)
{
    int f;
    arch_word_t entry;

    d_open(bin->path, &f, "r");
    if (f < 0) {
        return 0;
    }
    d_read(f, &entry, sizeof(entry));
    d_read(f, &entry, sizeof(entry));
    d_close(f);
    return entry;
}

#define _GET_PAD(x, a) ((a) - ((x) % (a)))
#define _ROUND_UP(x, a) ((x) + _GET_PAD(a, x))
#define _ROUND_DOWN(x, a) ((x) - ((x) % (a)))

static boot_bin_t *
boot_setup_bin (const char *dirpath, const char *name, bintype_t type)
{
    boot_bin_t *bin;
    arch_word_t entryaddr;

    bin = (boot_bin_t *)heap_malloc(sizeof(*bin));
    assert(bin);

    bin->progaddr = g_app_program_addr;
    snprintf(bin->dirpath, sizeof(bin->dirpath), "%s", dirpath);
    snprintf(bin->name, sizeof(bin->name), "%s", name);
    snprintf(bin->path, sizeof(bin->path), "%s/%s", dirpath, name);
    bin->type = type;
    entryaddr = boot_get_entry_addr(bin);
    entryaddr = _ROUND_DOWN(entryaddr, 0x4000);
    assert(g_app_program_addr == entryaddr);
    boot_bin_link(bin);
    return bin;
}

void boot_read_path (const char *path)
{
    fobj_t fobj;
    int dir, bindir;
    char buf[BOOT_MAX_PATH];
    char bindir_name[BOOT_MAX_NAME];

    dir = d_opendir(path);
    if (dir < 0) {
        dprintf("%s() : fail\n", __func__);
    }
    while (d_readdir(dir, &fobj) >= 0) {

        if (fobj.type == FTYPE_DIR) {
            fobj_t binobj;
            snprintf(buf, sizeof(buf), "%s/%s", path, fobj.name);
            bindir = d_opendir(buf);
            if (bindir < 0) {
                dprintf("%s() : Cannot open : \'%s\'\n", __func__, buf);
                continue;
            }
            snprintf(bindir_name, sizeof(bindir_name), "%s", fobj.name);
            while (d_readdir(bindir, &binobj) >= 0) {
                d_bool hexpresent;

                if (binobj.type == FTYPE_DIR && strcmp(binobj.name, "BIN") == 0) {
                    d_closedir(bindir);
                    snprintf(buf, sizeof(buf), "%s/%s", buf, binobj.name);
                    bindir = d_opendir(buf);
                    if (bindir < 0) {
                        dprintf("%s() : Cannot open : \'%s\'\n", __func__, buf);
                        break;
                    }
                    hexpresent = d_false;
                    while (d_readdir(bindir, &binobj) >= 0) {
                        if (binobj.type == FTYPE_FILE) {
                            bintype_t type = boot_check_bin_compat(binobj.name);

                            if (type != BIN_NONE) {
                                boot_setup_bin(buf, binobj.name, type);
                                hexpresent = d_true;
                            }
                        }
                    }
                    if (!hexpresent) {
                        dprintf("%s() : bin dir with no .hex file!\n", __func__);
                    }
                    d_closedir(bindir);
                    bindir = -1;
                    break;
                }
            }
            if (bindir >= 0) {
                d_closedir(bindir);
            }
        }
    }
    d_closedir(dir);
}

static void *cache_bin (const char *path, int *binsize)
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
    cache = heap_alloc_shared(size);
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

int boot_execute_boot (arch_word_t *progaddr, const char *path, const char *argv)
{
    void *bindata;
    int binsize = 0, err = 0;

    dprintf("Booting : \'%s\'\n", path);

    bindata = cache_bin(path, &binsize);
    if (!bindata) {
        return 0;
    }

    if (!boot_load_existing && !bhal_prog_exist(progaddr, bindata, binsize / sizeof(arch_word_t))) {
        err = bhal_load_program(NULL, progaddr, bindata, binsize / sizeof(arch_word_t));
    }
    if (err < 0) {
        return 0;
    }
    dprintf("Starting app... \n");

    dev_deinit();
    bhal_boot(progaddr);
    return 0;
}

static void boot_destroy_bins (void)
{
    boot_bin_t *bin = boot_bin_head;

    while (bin) {

        heap_free(bin);
        bin = bin->next;
    }
}

static int boot_handle_selected (pane_t *pane, component_t *com, void *user)
{
    boot_bin_t *bindesc = *((boot_bin_t **)com->user);

    if (bindesc == NULL) {
        gui_print(com_title, "Search result empty\n");
        return 0;
    }

    pane->parent->destroy = 1;
    g_app_program_addr = bindesc->progaddr;
    boot_cmd_push("boot", bindesc->path, NULL, NULL);
    boot_destroy_bins();
    return 1;
}

static int boot_show_list (pane_t *pane, component_t *com, void *user)
{
    boot_bin_t *bin = boot_bin_head;
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

static void gamepad_handle (gevt_t *evt)
{

    switch (evt->sym) {
        case 'e':
            gui_resp(&gui, com_browser, evt);
        break;

        default :
        break;
    }
    
}

static int _alert_close_hdlr (pane_t *pane, component_t *com, void *user)
{
    win_close_allert(pane->parent, pane);
    return 0;
}

static int _alert_accept_hdlr (pane_t *pane, component_t *com, void *user)
{
    win_close_allert(pane->parent, pane);
    return 0;
}

static int _alert_decline_hdlr (pane_t *pane, component_t *com, void *user)
{
    win_close_allert(pane->parent, pane);
    return 0;
}

static const char *gui_sfx_type_to_name[GUISFX_MAX] =
{
    [GUISFX_OPEN] = "dsbarexp",
    NULL,
    NULL,
    NULL,
};

static void __gui_alloc_sfx (int *num, gui_sfx_std_type_t type)
{
    *num = -1;
    switch (type) {

        default:
            if (gui_sfx_type_to_name[type]) {
                *num = boot_audio_open_wave(gui_sfx_type_to_name[type]);
            }
        break;
    }
}

static void __gui_start_sfx (int num)
{
    if (num < 0) {
        return;
    }
    boot_audio_play_wave(num, 127);
}

const kbdmap_t gamepad_to_kbd_map[JOY_STD_MAX] =
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

i_event_t *__post_key (i_event_t  *evts, i_event_t *event)
{
    gevt_t evt;

    evt.p.x = event->x;
    evt.p.y = event->y;
    evt.e = event->state == keydown ? GUIACT : GUIRELEASE;
    evt.sym = event->sym;

    if (event->x == 0 && event->y == 0) {
        gamepad_handle(&evt);
        return NULL;
    }

    gui_resp(&gui, NULL, &evt);
    return NULL;
}

static int user_execute_boot (void *p1, void *p2)
{
    char *path = (char *)p1;
    int len = (int)p2;

    boot_execute_boot((arch_word_t *)g_app_program_addr, path, NULL);

    return len;
}

static void boot_cmd_exec (void)
{
     boot_cmd_t cmd;
     boot_cmd_t *cmdptr = boot_cmd_pop(&cmd);
     char buf[BOOT_MAX_PATH];

     while (cmdptr) {
        snprintf(buf, sizeof(buf), "%s %s\n", cmdptr->cmd, cmdptr->text);
        term_parse(buf, sizeof(buf));
        cmdptr = boot_cmd_pop(cmdptr);
     }
}

int boot_main (int argc, const char **argv)
{
    screen_t s;
    prop_t prop;
    component_t *com;
    dvar_t dvar;

    input_soft_init(__post_key, gamepad_to_kbd_map);
    vid_wh(&s);

    gui.alloc_sfx = __gui_alloc_sfx;
    gui.start_sfx = __gui_start_sfx;

    prop.bcolor = COLOR_BLUE;
    prop.fcolor = COLOR_WHITE;
    prop.ispad = d_true;
    prop.user_draw = d_false;

    dprintf("Bootloader enter\n");

    gui_init(&gui, 0, 0, s.width, s.height);
    pane = gui_get_pane("pane");
    gui_set_pane(&gui, pane);

    com = gui_get_comp("pad0", "");
    gui_set_prop(com, &prop);
    gui_set_comp(pane, com, 0, 0, 120, gui.dim.h);

    prop.ispad = d_false;
    prop.bcolor = COLOR_GREY;
    com = gui_get_comp("title", NULL);
    gui_set_prop(com, &prop);
    gui_set_comp(pane, com, 120, 0, gui.dim.w - 120, 80);
    com_title = com;

    prop.bcolor = COLOR_GREEN;
    com = gui_get_comp("browser", NULL);
    gui_set_prop(com, &prop);
    gui_set_comp(pane, com, 120, 80, gui.dim.w - 120, gui.dim.h - 80);
    com->draw = boot_show_list;
    com->act = boot_handle_selected;
    com->user = &boot_bin_selected;
    com_browser = com;

    alert_pane = win_new_allert(&gui, 200, 160,
        _alert_close_hdlr, _alert_accept_hdlr, _alert_decline_hdlr);

    gui_select_pane(&gui, pane);

    boot_read_path("");

    dvar.type = DVAR_FUNC;
    dvar.ptr = user_execute_boot;
    dvar.ptrsize = sizeof(&user_execute_boot);
    d_dvar_reg(&dvar, "boot");

    d_dvar_int32(&boot_load_existing, "skipflash");

    while (!gui.destroy) {

        gui_draw(&gui);
        dev_tickle();
        input_proc_keys(NULL);
        boot_cmd_exec();
    }
    return 0;
}
