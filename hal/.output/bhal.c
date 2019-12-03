#include <stdint.h>

#include <stm32f769i_discovery_sdram.h>

#include <debug.h>
#include <boot_int.h>
#include <bsp_mod_int.h>

#include <misc_utils.h>
#include <main.h>
#include <bsp_sys.h>

#define BHAL_MEM_RW_PORTION (1 << 10)
#define BHAL_DBG_LINE_LEN ((1 << 6) - 1)

exec_region_t g_exec_region = EXEC_DRIVER;

static uint32_t
__bhal_FLASH_get_sect_num(uint32_t Address);

exec_mem_type_t
__bhal_get_memory_type (arch_word_t addr)
{
    if ((addr >= FLASH_BASE) && (addr <= FLASH_END)) {
        return EXEC_ROM;
    }
    if ((addr >= SDRAM_DEVICE_ADDR) && (addr <= (SDRAM_DEVICE_ADDR + SDRAM_DEVICE_SIZE))) {
        return EXEC_SDRAM;
    }
    if ((addr >= SRAM1_BASE) && (addr <= SRAM2_BASE)) {
        return EXEC_SRAM;
    }
    return EXEC_INVAL;
}

static inline void
__bhal_progmode_leave (void)
{
    FLASH_OBProgramInitTypeDef OBInit = {0};
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
}

static inline void
__bhal_progmode_enter (void)
{
    HAL_FLASH_Lock();
}

static int
__bhal_FLASH_memory_erase (complete_ind_t cplth, arch_word_t *_addr, void *_bin, uint32_t size)
{
    arch_word_t addr = (arch_word_t)_addr;
    FLASH_EraseInitTypeDef EraseInitStruct = {0};
    uint32_t sectnum, firstsector, sectorerror;

    size = size * sizeof(arch_word_t);
    firstsector = __bhal_FLASH_get_sect_num(addr);
    /* Get the number of sector to erase from 1st sector*/
    sectnum = __bhal_FLASH_get_sect_num(addr + size) - firstsector + 1;
    /* Fill EraseInit structure*/

    EraseInitStruct.TypeErase     = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange  = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector        = firstsector;
    EraseInitStruct.NbSectors     = sectnum;
    hdd_led_on();
    dprintf("Start erase at <0x%p> : %u bytes..\n", (uint32_t *)addr, sectnum);
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &sectorerror) != HAL_OK) {
        dprintf("Flash erase fail!\n");
        return -1;
    }
    hdd_led_off();
    dprintf("Erase done\n");
    return 0;
}

static int
__bhal_FLASH_memory_program (arch_word_t *dst, const arch_word_t *src, int size)
{
    int errcnt = 0;
    int _size = size;

    if (BOOT_LOG_CHECK(BOOT_LOG_INFO2)) {
        BOOT_LOG_ALWAYS("%s() BEFORE:\n", __func__);
        BOOT_LOG_ALWAYS("src: \n");  boot_log_hex(src, size);
    }
    hdd_led_on();
    while (size > 0) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, (arch_word_t)dst, *src) != HAL_OK) {
            errcnt++;
        }
        src++;
        dst++;
        size--;
    }
    hdd_led_off();
    if (BOOT_LOG_CHECK(BOOT_LOG_INFO2)) {
        BOOT_LOG_ALWAYS("%s() COMPARE:\n", __func__);
        boot_log_comp_hex_u32(dst, src, _size);
    }
    return errcnt ? -errcnt : 0;
}

static int
__bhal_memory_read (arch_word_t *dst, const arch_word_t *src, int size)
{
    int _size = size;
    while (size > 0) {
        *dst++ = *src++;
        size--;
    }
    if (BOOT_LOG_CHECK(BOOT_LOG_INFO2)) {
        BOOT_LOG_ALWAYS("%s():\n", __func__);
        BOOT_LOG_ALWAYS("src: \n");  boot_log_hex(dst, _size);
    }
    return 0;
}

static int
__bhal_memory_write (arch_word_t *dst, const arch_word_t *src, int size)
{
    return __bhal_memory_read(dst, src, size);
}

static int 
__bhal_memory_erase (arch_word_t *dst, const arch_word_t *src, int size)
{
    int _size = size;
    if (BOOT_LOG_CHECK(BOOT_LOG_INFO2)) {
        BOOT_LOG_ALWAYS("%s() BEFORE:\n", __func__);
        BOOT_LOG_ALWAYS("src: \n");  boot_log_hex(src, size);
        BOOT_LOG_ALWAYS("dest: \n"); boot_log_hex(dst, size);
    }
    hdd_led_on();
    while (size > 0) {
        *dst = 0;
        dst++;
        size--;
    }
    hdd_led_off();
    if (BOOT_LOG_CHECK(BOOT_LOG_INFO2)) {
        BOOT_LOG_ALWAYS("%s() COMPARE:\n", __func__);
        boot_log_comp_hex_u32(dst, src, _size);
    }
    return 0;
}

static int
__bhal_memory_compare (arch_word_t *dst, const arch_word_t *src, int size)
{
    int miss = 0;
    int _size = size;
    while (size > 0) {
        if (*dst++ != *src++) {
            miss++;
        }
        size--;
    }
    if (BOOT_LOG_CHECK(BOOT_LOG_INFO)) {
        BOOT_LOG_ALWAYS("%s() :\n", __func__);
        boot_log_comp_hex_u32(dst, src, _size);
    }
    return miss;
}

typedef struct {
    const char *entermsg;
    const char *leavemsg;
    const char *fatalmsg;
    const char *statchar;

    d_bool isfatal;
    d_bool srcinc; /*Increment source ptr*/
    int (*func) (arch_word_t *dst, const arch_word_t *src, int size);
} prog_func_t;

static const prog_func_t func_program =
{
    "Writing...",
    "",
    "Corrupted memory",
    "#O",
    d_true,
    d_true,
    __bhal_FLASH_memory_program,
};

static const prog_func_t func_clear =
{
    "Erasing...",
    "",
    "Erase failed",
    "E?",
    d_true,
    d_false,
    __bhal_memory_erase,
};

static const prog_func_t func_compare =
{
    "Checking...",
    "",
    "Mismatch",
    ".X",
    d_false,
    d_true,
    __bhal_memory_compare,
};

static const prog_func_t func_read =
{
    "Reading...",
    "",
    "Read fail",
    "..",
    d_false,
    d_true,
    __bhal_memory_read,
};

static const prog_func_t func_write =
{
    "Writing...",
    "",
    "Write fail",
    "#O",
    d_true,
    d_true,
    __bhal_memory_write,
};

static int
bhal_bin_memory_op_wrapper 
    (const complete_ind_t cplth, const prog_func_t *func,
    arch_word_t *addr, void *_bin, arch_word_t size)
{
    arch_word_t *tmpaddr = addr, *bin = (arch_word_t *)_bin;
    int errors = 0, errors_total = 0;
    int blkcnt = 0, blktotal = size / BHAL_MEM_RW_PORTION;
    char linebuf[B_MAX_LINEBUF];

    if (cplth) {
        cplth(func->entermsg, 0);
    }

    dprintf("%s :\n", func->entermsg);
    dprintf("addr : <0x%p>; size : 0x%08x\n", addr, size);
    while (blkcnt < blktotal) {

        errors += func->func(tmpaddr, bin, BHAL_MEM_RW_PORTION);
        tmpaddr += BHAL_MEM_RW_PORTION;
        if (func->srcinc) {
            bin += BHAL_MEM_RW_PORTION;
        }
        errors_total += errors;

        linebuf[blkcnt] = func->statchar[!!errors];
        if ((blkcnt & BHAL_DBG_LINE_LEN) == BHAL_DBG_LINE_LEN) {
            linebuf[blkcnt + 1] = 0;
            dprintf("[ %s ]\n", linebuf);
        }
        blkcnt++;
        if (cplth) {
            int per = (blkcnt * 100) / blktotal;
            cplth(NULL, per);
        }
    }
    size = size - (tmpaddr - addr);
    if (size > 0) {
        errors_total += func->func(tmpaddr, bin, size);
    }
    dprintf("Done, errors : %i\n", errors_total);
    if (errors_total && func->isfatal) {
        dprintf("Fatal : %s\n", func->fatalmsg);
        if (cplth) {
            cplth(func->fatalmsg, -errors_total);
        }
        return -1;
    }
    if (cplth) {
        cplth(func->leavemsg, 100);
    }
    return errors_total;
}

static void
__bhal_ll_boot_noreturn (arch_word_t addr)
{
extern void CPU_CACHE_Disable (void);
    register volatile arch_word_t *entryptr, *spinitial;

    entryptr = (arch_word_t *)(addr + sizeof(arch_word_t));
    spinitial = (arch_word_t *)(addr);

    CPU_CACHE_Disable();

    arch_dsb();
    arch_set_sp(*spinitial);
    arch_asmgoto(*entryptr);
}

void bhal_execute_application (void *addr)
{
    exec_region_t exec_prev = g_exec_region;

    assert(g_exec_region != EXEC_APPLICATION);

    g_exec_region = EXEC_APPLICATION;
    __bhal_ll_boot_noreturn(((arch_word_t)addr));
    assert(0);
    g_exec_region = exec_prev;
}

int bhal_execute_module (arch_word_t addr)
{
typedef int (*exec_t) (void);
    exec_region_t exec_prev = g_exec_region;
    register volatile arch_word_t *entryptr;
    exec_t exec;
    int ret = 0;

    assert(g_exec_region != EXEC_DRIVER);
    g_exec_region = EXEC_APPLICATION;

    entryptr = (arch_word_t *)(addr + sizeof(arch_word_t));
    exec = (exec_t)entryptr;

    __DSB();
    ret = exec();
    g_exec_region = exec_prev;
    return ret;
}


d_bool bhal_bin_check_in_mem (complete_ind_t cplth, arch_word_t *progaddr, void *progdata, size_t progsize)
{
    uint32_t pad = sizeof(arch_word_t) - 1;

    assert(!((arch_word_t)progdata & pad));

    if (bhal_bin_memory_op_wrapper(cplth, &func_compare, progaddr, progdata, progsize)) {
        return d_false;
    }
    return d_true;
}

static int __bhal_write_ROM (complete_ind_t cplth, arch_word_t *progaddr,
                            void *progdata, size_t progsize)
{
    int err = 0;

    __bhal_progmode_leave();
    err = __bhal_FLASH_memory_erase(cplth, progaddr, progdata, progsize);
    if (err >= 0) {
        err = bhal_bin_memory_op_wrapper(cplth, &func_program, progaddr, progdata, progsize);
    }
    __bhal_progmode_enter();
    if (err >= 0) {
        err = bhal_bin_memory_op_wrapper(cplth, &func_compare, progaddr, progdata, progsize);
    }
    return err;
}

static int __bhal_write_RAM (complete_ind_t cplth, arch_word_t *progaddr,
                            void *progdata, size_t progsize)
{
    bhal_bin_memory_op_wrapper(cplth, &func_clear, progaddr, progdata, progsize);
    return bhal_bin_memory_op_wrapper(cplth, &func_write, progaddr, progdata, progsize);
}


int __bhal_clear_RAM (complete_ind_t cplth, arch_word_t *progaddr, void *val, size_t progsize)
{
    return bhal_bin_memory_op_wrapper(cplth, &func_clear, progaddr, val, progsize);
}


int bhal_bin_2_mem_load (complete_ind_t cplth, arch_word_t *progaddr,
                            void *progdata, size_t progsize)
{
    int (*bhal_op) (complete_ind_t, arch_word_t *, void *, size_t) = NULL;
    exec_mem_type_t exec_mem_type = __bhal_get_memory_type((arch_word_t)progaddr);

    switch (exec_mem_type) {
        case EXEC_ROM :
            bhal_op = __bhal_write_ROM;
        break;
        case EXEC_SDRAM :
        case EXEC_SRAM :
            bhal_op = __bhal_write_RAM;
        break;
        default:
        break;
    }
    if (bhal_op) {
        return bhal_op(cplth, progaddr, progdata, progsize);
    }
    dprintf("Unsupported memory region type : %i\n", exec_mem_type);
    return -1;
}

int bhal_set_mem_with_value (complete_ind_t cplth, arch_word_t *progaddr,
                        size_t progsize, arch_word_t value)
{
    int (*bhal_op) (complete_ind_t, arch_word_t *, void *, size_t) = NULL;
    exec_mem_type_t exec_mem_type = __bhal_get_memory_type((arch_word_t)progaddr);

    switch (exec_mem_type) {
        case EXEC_ROM :
            dprintf("%s() : [EXEC_ROM] not yet\n", __func__);
            return -1;
        break;
        case EXEC_SDRAM :
        case EXEC_SRAM :
            bhal_op = __bhal_clear_RAM;
        break;
        default:
        break;
    }
    if (bhal_op) {
        return bhal_op(cplth, progaddr, &value, progsize);
    }
    dprintf("Unsupported memory region type : %i\n", exec_mem_type);
    return -1;
}

int
bhal_setup_bin_param (boot_bin_parm_t *parm, void *ptr, int size)
{
    arch_word_t *bindata = (arch_word_t *)ptr;
    parm->entrypoint = bindata[1];
    parm->progaddr = ROUND_DOWN(parm->entrypoint, 0x1000);
    parm->spinitial = bindata[0];
    parm->size = size;
    return 0;
}

arch_word_t *
bhal_install_executable (complete_ind_t clbk, arch_word_t *progptr,
                                 arch_word_t *progsize, const char *path)
{

    void *bindata;
    int binsize = 0, err = 0;
    bsp_heap_api_t heap = {.malloc = heap_alloc_shared, .free = heap_free};
    boot_bin_parm_t parm;

    dprintf("Installing : \'%s\'\n", path);

    bindata = bres_cache_file_2_mem(&heap, path, &binsize);
    if (!bindata) {
        return NULL;
    }
    if (progptr == NULL) {
        bhal_setup_bin_param(&parm, bindata, binsize);
        progptr = (arch_word_t *)parm.progaddr;
    }

    if (!bhal_bin_check_in_mem(clbk, progptr, bindata, binsize / sizeof(arch_word_t))) {
        err = bhal_bin_2_mem_load(clbk, progptr, bindata, binsize / sizeof(arch_word_t));
    }
    if (err < 0) {
        return NULL;
    }
    *progsize = binsize;
    return progptr;
}

int
bhal_start_application (arch_word_t *progaddr, arch_word_t progbytes,
                                int argc, const char **argv)
{
    dprintf("Starting application... \n");

    bsp_argc_argv_set(argv[0]);
    if (bsp_argc_argv_check(argv[0]) < 0) {
        return -1;
    }
    dev_deinit();
    bhal_execute_application(progaddr);
    return 0;
}




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
/**
  * @brief  Gets the sector of a given address
  * @param  None
  * @retval The sector of a given address
  */
static uint32_t __bhal_FLASH_get_sect_num (uint32_t Address)
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
