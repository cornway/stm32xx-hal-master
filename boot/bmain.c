#include <main.h>
#include <gfx.h>
#include <boot_int.h>
#include <dev_io.h>
#include <debug.h>
#include <nvic.h>
#include <input_main.h>
#include <lcd_main.h>
#include <string.h>

//#define DUAL_BANK

/* Base address of the Flash sectors */
#if defined(DUAL_BANK)
#define ADDR_FLASH_SECTOR_0     ((uint32_t)0x08000000) /* Base address of Sector 0, 16 Kbytes */
#define ADDR_FLASH_SECTOR_1     ((uint32_t)0x08004000) /* Base address of Sector 1, 16 Kbytes */
#define ADDR_FLASH_SECTOR_2     ((uint32_t)0x08008000) /* Base address of Sector 2, 16 Kbytes */
#define ADDR_FLASH_SECTOR_3     ((uint32_t)0x0800C000) /* Base address of Sector 3, 16 Kbytes */
#define ADDR_FLASH_SECTOR_4     ((uint32_t)0x08010000) /* Base address of Sector 4, 64 Kbytes */
#define ADDR_FLASH_SECTOR_5     ((uint32_t)0x08020000) /* Base address of Sector 5, 128 Kbytes */
#define ADDR_FLASH_SECTOR_6     ((uint32_t)0x08040000) /* Base address of Sector 6, 128 Kbytes */
#define ADDR_FLASH_SECTOR_7     ((uint32_t)0x08060000) /* Base address of Sector 7, 128 Kbytes */
#define ADDR_FLASH_SECTOR_8     ((uint32_t)0x08080000) /* Base address of Sector 8, 128 Kbytes */
#define ADDR_FLASH_SECTOR_9     ((uint32_t)0x080A0000) /* Base address of Sector 9, 128 Kbytes */
#define ADDR_FLASH_SECTOR_10    ((uint32_t)0x080C0000) /* Base address of Sector 10, 128 Kbytes */
#define ADDR_FLASH_SECTOR_11    ((uint32_t)0x080E0000) /* Base address of Sector 11, 128 Kbytes */
#define ADDR_FLASH_SECTOR_12    ((uint32_t)0x08100000) /* Base address of Sector 12, 16 Kbytes */
#define ADDR_FLASH_SECTOR_13    ((uint32_t)0x08104000) /* Base address of Sector 13, 16 Kbytes */
#define ADDR_FLASH_SECTOR_14    ((uint32_t)0x08108000) /* Base address of Sector 14, 16 Kbytes */
#define ADDR_FLASH_SECTOR_15    ((uint32_t)0x0810C000) /* Base address of Sector 15, 16 Kbytes */
#define ADDR_FLASH_SECTOR_16    ((uint32_t)0x08110000) /* Base address of Sector 16, 64 Kbytes */
#define ADDR_FLASH_SECTOR_17    ((uint32_t)0x08120000) /* Base address of Sector 17, 128 Kbytes */
#define ADDR_FLASH_SECTOR_18    ((uint32_t)0x08140000) /* Base address of Sector 18, 128 Kbytes */
#define ADDR_FLASH_SECTOR_19    ((uint32_t)0x08160000) /* Base address of Sector 19, 128 Kbytes */
#define ADDR_FLASH_SECTOR_20    ((uint32_t)0x08180000) /* Base address of Sector 20, 128 Kbytes */
#define ADDR_FLASH_SECTOR_21    ((uint32_t)0x081A0000) /* Base address of Sector 21, 128 Kbytes */
#define ADDR_FLASH_SECTOR_22    ((uint32_t)0x081C0000) /* Base address of Sector 22, 128 Kbytes */
#define ADDR_FLASH_SECTOR_23    ((uint32_t)0x081E0000) /* Base address of Sector 23, 128 Kbytes */
#else
#define ADDR_FLASH_SECTOR_0     ((uint32_t)0x08000000) /* Base address of Sector 0, 32 Kbytes */
#define ADDR_FLASH_SECTOR_1     ((uint32_t)0x08008000) /* Base address of Sector 1, 32 Kbytes */
#define ADDR_FLASH_SECTOR_2     ((uint32_t)0x08010000) /* Base address of Sector 2, 32 Kbytes */
#define ADDR_FLASH_SECTOR_3     ((uint32_t)0x08018000) /* Base address of Sector 3, 32 Kbytes */
#define ADDR_FLASH_SECTOR_4     ((uint32_t)0x08020000) /* Base address of Sector 4, 128 Kbytes */
#define ADDR_FLASH_SECTOR_5     ((uint32_t)0x08040000) /* Base address of Sector 5, 256 Kbytes */
#define ADDR_FLASH_SECTOR_6     ((uint32_t)0x08080000) /* Base address of Sector 6, 256 Kbytes */
#define ADDR_FLASH_SECTOR_7     ((uint32_t)0x080C0000) /* Base address of Sector 7, 256 Kbytes */
#define ADDR_FLASH_SECTOR_8     ((uint32_t)0x08100000) /* Base address of Sector 8, 256 Kbytes */
#define ADDR_FLASH_SECTOR_9     ((uint32_t)0x08140000) /* Base address of Sector 9, 256 Kbytes */
#define ADDR_FLASH_SECTOR_10    ((uint32_t)0x08180000) /* Base address of Sector 10, 256 Kbytes */
#define ADDR_FLASH_SECTOR_11    ((uint32_t)0x081C0000) /* Base address of Sector 11, 256 Kbytes */
#endif /* DUAL_BANK */

#if defined(DUAL_BANK)
uint32_t g_app_program_addr = ADDR_FLASH_SECTOR_12;
#else
uint32_t g_app_program_addr = ADDR_FLASH_SECTOR_8;
#endif

#define BOOT_MAX_NAME 24
#define BOOT_MAX_PATH 128

component_t *com_browser;
component_t *com_title;
gui_t gui;
pane_t *pane, *alert_pane;

typedef struct boot_bin_s {
    struct boot_bin_s *next;


    char name[BOOT_MAX_NAME];
    char dirpath[BOOT_MAX_PATH];
    char path[BOOT_MAX_PATH];
} boot_bin_t;

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

#define BIN_FEXT "bin"

static inline d_bool
boot_cmp_fext (const char *in, const char *ext)
{
    int pos = strlen(in) - strlen(ext);

    return !strcasecmp(in + pos, ext);
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
                        if (binobj.type == FTYPE_FILE && boot_cmp_fext(binobj.name, BIN_FEXT)) {
                            boot_bin_t *bin;

                            bin = Sys_Malloc(sizeof(*bin));
                            assert(bin);

                            snprintf(bin->dirpath, sizeof(bin->dirpath), "%s", buf);
                            snprintf(buf, sizeof(buf), "%s/%s", buf, binobj.name);
                            snprintf(bin->name, sizeof(bin->name), "%s", bindir_name);
                            snprintf(bin->path, sizeof(bin->path), "%s", buf);
                            boot_bin_link(bin);
                            hexpresent = d_true;
                            break;
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

static void *cache_bin (boot_bin_t *bin, int *binsize)
{
    int f;
    int fsize;
    void *cache;

    fsize = d_open(bin->path, &f, "r");
    if (f < 0) {
        dprintf("%s() : open fail : \'%s\'\n", __func__, bin->path);
        return NULL;
    }
    cache = Sys_Malloc(ROUND_UP(fsize, 32));
    assert(cache);

    if (d_read(f, cache, fsize) < fsize) {
        dprintf("%s() : missing part\n", __func__);
        Sys_Free(cache);
        cache = NULL;
    } else {
        *binsize = fsize;
    }
    d_close(f);

    return cache;
}

static uint32_t GetSector(uint32_t Address);

static int flash_verify (uint32_t addr, void *_bin, uint32_t size)
{
    uint32_t writeaddr = addr, *bin = (uint32_t *)_bin;
    uint32_t *flash = (uint32_t *)addr;
    uint32_t errors = 0, errors_total = 0;
    int blkcnt = 0;

    dprintf("%s() : Verifying :\n[", __func__);
    while (writeaddr < addr + size) {
        uint32_t start, end;

        start = writeaddr / 4;
        end = start + 256;
        for (; start < end; start++) {
            if (*flash != *bin) {
                errors++;
            }
            flash++;
            bin++;
            writeaddr += 4;
        }
        errors_total += errors;
        if (errors) {
            dprintf("X");
        } else {
            dprintf(".");
        }
        if ((blkcnt & 0x3f) == 0x3f) {
            dprintf("\n");
        }
        blkcnt++;
     }
    dprintf("]\n Done, missed : %u words\n", errors_total);
    if (errors_total) {
        dprintf("Fatal : currupted .hex will be not exec.\n");
        return -1;
    }
    return 0;
}

static int flash_program (uint32_t addr, void *_bin, uint32_t size)
{
    FLASH_EraseInitTypeDef EraseInitStruct;
    FLASH_OBProgramInitTypeDef    OBInit = {0};
    uint32_t sectnum, firstsector, sectorerror;
    uint32_t writeaddr, *bin = (uint32_t *)_bin;
    int blkcnt = 0;

    dprintf("Starting program flash ...\n");

    HAL_FLASH_Unlock();

    HAL_FLASH_OB_Unlock();
    HAL_FLASHEx_OBGetConfig(&OBInit);
#if defined(DUAL_BANK)  
    if((OBInit.USERConfig & OB_NDBANK_SINGLE_BANK) == OB_NDBANK_SINGLE_BANK)
#else
    if((OBInit.USERConfig & OB_NDBANK_SINGLE_BANK) == OB_NDBANK_DUAL_BANK)
#endif
    {
        assert(0);
    }

    firstsector = GetSector(addr);
    /* Get the number of sector to erase from 1st sector*/
    sectnum = GetSector(addr + size) - firstsector + 1;
    /* Fill EraseInit structure*/
    EraseInitStruct.TypeErase     = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange  = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector        = firstsector;
    EraseInitStruct.NbSectors     = sectnum;

    dprintf("Start erase at [%p] size %u...\n", (uint32_t *)firstsector, sectnum);
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &sectorerror) != HAL_OK) {
        dprintf("Flash erase fail!\n");
        return -1;
    }
    dprintf("Erase done\n");
    writeaddr = addr;

    dprintf("Start program...\n [");
    while (writeaddr < addr + size) {
        uint32_t start, end;

        start = writeaddr / 4;
        end = start + 256;
        for (; start < end; start++) {
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, writeaddr, *bin) != HAL_OK) {
                dprintf("%s() : Write fail at [%p]\n", __func__, (uint32_t *)writeaddr);
                return -1;
            }
            bin++;
            writeaddr += 4;
        }
        dprintf("#");
        if ((blkcnt & 0x3f) == 0x3f) {
            dprintf("\n");
        }
        blkcnt++;
    }
    HAL_FLASH_Lock();
    dprintf("]\n");
    dprintf("Program finished\n");
    return 0;
}

static inline void boot_reloc_vectortable (uint32_t addr)
{
    register uint32_t offset = addr - FLASH_BASE;
    SCB->VTOR = FLASH_BASE | offset;
}

static void __boot_exec (uint32_t addr) __attribute__((noreturn));

static void __boot_exec (uint32_t addr)
{
    register volatile arch_word_t *entryptr, *spinitial;
    
    entryptr = (arch_word_t *)(addr + sizeof(arch_word_t));
    spinitial = (arch_word_t *)(addr);

    SCB_DisableDCache();
    SCB_DisableICache();
    SCB_CleanInvalidateDCache();
    SCB_InvalidateICache();

    __DSB();
    boot_reloc_vectortable(addr);
    __msp_set(*spinitial);
    arch_asmgoto(*entryptr);
}

static void boot_exec (void *addr)
{
    dprintf("%s() :\n", __func__);
    dev_deinit();
    __boot_exec(((uint32_t)addr));
    assert(0);
}

#define BOOT_SYS_DIR_PATH "/sys"
#define BOOT_SYS_LOG_NAME "log.txt"
#define BOOT_SYS_LOG_PATH BOOT_SYS_DIR_PATH"/"BOOT_SYS_LOG_NAME


static int boot_get_last_bin (char *name)
{
    int dir, f;
    char buf[BOOT_MAX_PATH];

    name[0] = 0;
    dir = d_opendir(BOOT_SYS_DIR_PATH);
    if (dir < 0) {
        if (d_mkdir(BOOT_SYS_DIR_PATH) < 0) {
            dprintf("%s() : fail\n", __func__);
            return -1;
        }
    } else {
        d_closedir(dir);
    }
    d_open(BOOT_SYS_LOG_PATH, &f, "r");
    if (f < 0) {
        d_open(BOOT_SYS_LOG_PATH, &f, "+w");
        if (f < 0) {
            dprintf("%s() : fail\n", __func__);
            return -1;
        }
    } else {
        if (d_gets(f, buf, sizeof(buf))) {
            strcpy(name, buf);
        }
    }
    d_close(f);
    return 0;
}

static int boot_set_last_bin (const char *name)
{
    int f, n;

    d_open(BOOT_SYS_LOG_PATH, &f, "w");
    if (f < 0) {
        dprintf("%s() : fail\n", __func__);
        return -1;
    }
    d_seek(f, 0, DSEEK_SET);
    n = d_printf(f, "%s", name);
    d_close(f);
    return 0;
}

static int boot_handle_bins (pane_t *pane, component_t *com, void *user)
{
    gevt_t *evt = (gevt_t *)user;
    char name[BOOT_MAX_NAME] = {0};
    void *bindata;
    int binsize = 0;
    d_bool flash_valid = d_true;/*REMOVE!!*/

    if (boot_bin_selected == NULL) {
        gui_print(com_title, "Search result empty\n");
        return 0;
    }

    dprintf("Cache bin : \'%s\'\n", boot_bin_selected->name);
    bindata = cache_bin(boot_bin_selected, &binsize);
    dprintf("Cache done : %p of size %u\n", bindata, binsize);
    if (!bindata) {
        return 0;
    }

    boot_get_last_bin(name);
    if (name[0] && strcmp(name, boot_bin_selected->name) == 0) {
        /*Already flashed*/
        if (flash_verify(g_app_program_addr, bindata, binsize) == 0) {
            flash_valid = d_true;
        }
    }

    if (!flash_valid) {
        if (flash_program(g_app_program_addr, bindata, binsize) < 0) {
            return 0;
        }
        if (flash_verify(g_app_program_addr, bindata, binsize) < 0) {
            return 0;
        }
        boot_set_last_bin(boot_bin_selected->name);
    }
    dprintf("Executing... \n");
    Sys_Free(bindata);
    boot_exec((void *)g_app_program_addr);
}

static int boot_draw_bins (pane_t *pane, component_t *com, void *user)
{
    boot_bin_t *bin = boot_bin_head;
    uint8_t maxbin = 8;
    uint8_t texty = 0;

    gui_com_clear(com);
    while (bin && maxbin) {

        gui_text(com, bin->name, 16, texty);
        bin = bin->next;
        texty += (480 - 64) / 8;
    }
}

static void gamepad_handle (gevt_t *evt)
{

    switch (evt->sym) {
        case 'e':
            gui_resp(&gui, com_browser, evt);
        break;

        default :
            dprintf("%s() : unhandled event : \'%c\'\n", __func__, evt->sym);
        break;
    }
    
}

i_event_t *input_post_key (i_event_t  *evts, i_event_t event)
{
    gevt_t evt;

    evt.p.x = event.x;
    evt.p.y = event.y;
    evt.e = event.state == keydown ? GUIACT : GUIRELEASE;
    evt.sym = event.sym;

    if (event.x == 0 && event.y == 0) {
        gamepad_handle(&evt);
        return NULL;
    }

    gui_resp(&gui, NULL, &evt);
    return NULL;
}

static int _alert_close_hdlr (pane_t *pane, component_t *com, void *user)
{
    win_close_allert(pane->parent, pane);
}

static int _alert_accept_hdlr (pane_t *pane, component_t *com, void *user)
{
    win_close_allert(pane->parent, pane);
}

static int _alert_decline_hdlr (pane_t *pane, component_t *com, void *user)
{
    win_close_allert(pane->parent, pane);
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

int boot_main (int argc, char **argv)
{
    screen_t s;
    prop_t prop;
    component_t *com;

    input_soft_init(gamepad_to_kbd_map);
    screen_get_wh(&s);

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
    gui_set_comp(pane, com, 0, 0, 120, 480);

    prop.ispad = d_false;
    prop.bcolor = COLOR_GREY;
    com = gui_get_comp("title", NULL);
    gui_set_prop(com, &prop);
    gui_set_comp(pane, com, 120, 0, 360, 80);
    com_title = com;

    prop.bcolor = COLOR_GREEN;
    com = gui_get_comp("browser", NULL);
    gui_set_prop(com, &prop);
    gui_set_comp(pane, com, 120, 80, 360, 400);
    com->draw = boot_draw_bins;
    com->act = boot_handle_bins;
    com_browser = com;

    alert_pane = win_new_allert(&gui, 200, 160,
        _alert_close_hdlr, _alert_accept_hdlr, _alert_decline_hdlr);

    gui_select_pane(&gui, pane);

    boot_read_path("");

    while (1) {

        gui_draw(&gui);
        dev_tickle();
        input_proc_keys(NULL);
    }
    return 0;
}

/**
  * @brief  Gets the sector of a given address
  * @param  None
  * @retval The sector of a given address
  */
static uint32_t GetSector(uint32_t Address)
{
  uint32_t sector = 0;

  if((Address < ADDR_FLASH_SECTOR_1) && (Address >= ADDR_FLASH_SECTOR_0))
  {
    sector = FLASH_SECTOR_0;
  }
  else if((Address < ADDR_FLASH_SECTOR_2) && (Address >= ADDR_FLASH_SECTOR_1))
  {
    sector = FLASH_SECTOR_1;
  }
  else if((Address < ADDR_FLASH_SECTOR_3) && (Address >= ADDR_FLASH_SECTOR_2))
  {
    sector = FLASH_SECTOR_2;
  }
  else if((Address < ADDR_FLASH_SECTOR_4) && (Address >= ADDR_FLASH_SECTOR_3))
  {
    sector = FLASH_SECTOR_3;
  }
  else if((Address < ADDR_FLASH_SECTOR_5) && (Address >= ADDR_FLASH_SECTOR_4))
  {
    sector = FLASH_SECTOR_4;
  }
  else if((Address < ADDR_FLASH_SECTOR_6) && (Address >= ADDR_FLASH_SECTOR_5))
  {
    sector = FLASH_SECTOR_5;
  }
  else if((Address < ADDR_FLASH_SECTOR_7) && (Address >= ADDR_FLASH_SECTOR_6))
  {
    sector = FLASH_SECTOR_6;
  }
  else if((Address < ADDR_FLASH_SECTOR_8) && (Address >= ADDR_FLASH_SECTOR_7))
  {
    sector = FLASH_SECTOR_7;
  }
  else if((Address < ADDR_FLASH_SECTOR_9) && (Address >= ADDR_FLASH_SECTOR_8))
  {
    sector = FLASH_SECTOR_8;
  }
  else if((Address < ADDR_FLASH_SECTOR_10) && (Address >= ADDR_FLASH_SECTOR_9))
  {
    sector = FLASH_SECTOR_9;
  }
  else if((Address < ADDR_FLASH_SECTOR_11) && (Address >= ADDR_FLASH_SECTOR_10))
  {
    sector = FLASH_SECTOR_10;
  }
#if defined(DUAL_BANK)
  else if((Address < ADDR_FLASH_SECTOR_12) && (Address >= ADDR_FLASH_SECTOR_11))
  {
    sector = FLASH_SECTOR_11;
  }
  else if((Address < ADDR_FLASH_SECTOR_13) && (Address >= ADDR_FLASH_SECTOR_12))
  {
    sector = FLASH_SECTOR_12;
  }
  else if((Address < ADDR_FLASH_SECTOR_14) && (Address >= ADDR_FLASH_SECTOR_13))
  {
    sector = FLASH_SECTOR_13;
  }
  else if((Address < ADDR_FLASH_SECTOR_15) && (Address >= ADDR_FLASH_SECTOR_14))
  {
    sector = FLASH_SECTOR_14;
  }
  else if((Address < ADDR_FLASH_SECTOR_16) && (Address >= ADDR_FLASH_SECTOR_15))
  {
    sector = FLASH_SECTOR_15;
  }
  else if((Address < ADDR_FLASH_SECTOR_17) && (Address >= ADDR_FLASH_SECTOR_16))
  {
    sector = FLASH_SECTOR_16;
  }
  else if((Address < ADDR_FLASH_SECTOR_18) && (Address >= ADDR_FLASH_SECTOR_17))
  {
    sector = FLASH_SECTOR_17;
  }
  else if((Address < ADDR_FLASH_SECTOR_19) && (Address >= ADDR_FLASH_SECTOR_18))
  {
    sector = FLASH_SECTOR_18;
  }
  else if((Address < ADDR_FLASH_SECTOR_20) && (Address >= ADDR_FLASH_SECTOR_19))
  {
    sector = FLASH_SECTOR_19;
  }
  else if((Address < ADDR_FLASH_SECTOR_21) && (Address >= ADDR_FLASH_SECTOR_20))
  {
    sector = FLASH_SECTOR_20;
  }
  else if((Address < ADDR_FLASH_SECTOR_22) && (Address >= ADDR_FLASH_SECTOR_21))
  {
    sector = FLASH_SECTOR_21;
  }
  else if((Address < ADDR_FLASH_SECTOR_23) && (Address >= ADDR_FLASH_SECTOR_22))
  {
    sector = FLASH_SECTOR_22;
  }
  else /* (Address < FLASH_END_ADDR) && (Address >= ADDR_FLASH_SECTOR_23) */
  {
    sector = FLASH_SECTOR_23;
  }  
#else  
  else /* (Address < FLASH_END_ADDR) && (Address >= ADDR_FLASH_SECTOR_11) */
  {
    sector = FLASH_SECTOR_11;
  }
#endif /* DUAL_BANK */  
  return sector;
}


