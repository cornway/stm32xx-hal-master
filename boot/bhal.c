#include <debug.h>
#include <boot_int.h>
#include <misc_utils.h>
#include <stdint.h>
#include <main.h>

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
arch_word_t g_app_program_addr = ADDR_FLASH_SECTOR_12;
#else
arch_word_t g_app_program_addr = ADDR_FLASH_SECTOR_8;
#endif

#define RW_PORTION (1 << 8)
#define RW_ALIGN_MS (RW_PORTION - 1)
#define PERCENT 100
#define DBG_MAXLINE ((1 << 6) - 1)

typedef struct {
    arch_word_t size;
} proghdr_t;

static uint32_t GetSector(uint32_t Address);
static void __boot_exec (uint32_t addr) __attribute__((noreturn));

static void bhal_prog_begin (void)
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

static void bhal_prog_end (void)
{
    HAL_FLASH_Lock();
}

static int bhal_prog_erase (cplthook_t cplth, arch_word_t *_addr, void *_bin, uint32_t size)
{
    arch_word_t addr = (arch_word_t)_addr;
    FLASH_EraseInitTypeDef EraseInitStruct = {0};
    uint32_t sectnum, firstsector, sectorerror;

    size = size * sizeof(arch_word_t);
    firstsector = GetSector(addr);
    /* Get the number of sector to erase from 1st sector*/
    sectnum = GetSector(addr + size) - firstsector + 1;
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

static int bhal_prog_write_chunk (arch_word_t *dst, const arch_word_t *src, int size)
{
    int errcnt = 0;
    while (size > 0) {
        hdd_led_on();
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (arch_word_t)dst, *src) != HAL_OK) {
            errcnt++;
        }
        hdd_led_off();
        src++;
        dst++;
        size--;
    }
    return errcnt ? -errcnt : 0;
}

static int bhal_prog_read_chunk (arch_word_t *dst, const arch_word_t *src, int size)
{
    while (size > 0) {
        *dst++ = *src++;
        size--;
    }
    return 0;
}

static int bhal_prog_cmp_chunk (arch_word_t *dst, const arch_word_t *src, int size)
{
    int miss = 0;
    while (size > 0) {
        if (*dst++ != *src++) {
            miss++;
        }
        size--;
    }
    return miss;
}

typedef struct {
    const char *entermsg;
    const char *leavemsg;
    const char *fatalmsg;
    const char *statchar;

    d_bool isfatal;
    int (*func) (arch_word_t *dst, const arch_word_t *src, int size);
} prog_func_t;

const prog_func_t func_write =
{
    "Write memory",
    "",
    "Corrupted memory",
    "#O",
    d_true,
    bhal_prog_write_chunk,
};

prog_func_t func_compare =
{
    "Compare memory",
    "",
    "Mismatch",
    ".X",
    d_false,
    bhal_prog_cmp_chunk,
};

prog_func_t func_read =
{
    "Read memory",
    "",
    "Read fail",
    "..",
    d_false,
    bhal_prog_read_chunk,
};

static int bhal_prog_handle_func 
    (const cplthook_t cplth, const prog_func_t *func,
    arch_word_t *addr, void *_bin, arch_word_t size)
{
    arch_word_t *tmpaddr = addr, *bin = (arch_word_t *)_bin;
    int errors = 0, errors_total = 0;
    int blkcnt = 0, blktotal = size / RW_PORTION;

    if (cplth) {
        cplth(func->entermsg, 0);
    }

    dprintf("%s() : %s :\n[", __func__, func->entermsg);
    while (blkcnt < blktotal) {

        errors += func->func(tmpaddr, bin, RW_PORTION);
        tmpaddr += RW_PORTION;
        bin += RW_PORTION;
        errors_total += errors;

        dprintf("%c", func->statchar[!!errors]);
        if ((blkcnt & DBG_MAXLINE) == DBG_MAXLINE) {
            dprintf("\n");
        }
        blkcnt++;
        if (cplth) {
            int per = (blkcnt * PERCENT) / blktotal;
            cplth("+", per);
        }
    }
    size = size - (tmpaddr - addr);
    if (size > 0) {
        errors_total += func->func(tmpaddr, bin, size);
    }
    dprintf("]\n Done, errors : %i\n", errors_total);
    if (errors_total && func->isfatal) {
        dprintf("Fatal : %s\n", func->fatalmsg);
        if (cplth) {
            cplth(func->fatalmsg, -errors_total);
        }
        return -1;
    }
    if (cplth) {
        cplth(func->leavemsg, PERCENT);
    }
    return errors_total;
}

static inline void __vtor_reloc (uint32_t addr)
{
    register uint32_t offset = addr - FLASH_BASE;
    SCB->VTOR = FLASH_BASE | offset;
}

static void __bhal_boot (arch_word_t addr)
{
    register volatile arch_word_t *entryptr, *spinitial;
    
    entryptr = (arch_word_t *)(addr + sizeof(arch_word_t));
    spinitial = (arch_word_t *)(addr);

/*
    SCB_DisableDCache();
    SCB_DisableICache();
    SCB_CleanInvalidateDCache();
    SCB_InvalidateICache();
*/
    __DSB();
    //__vtor_reloc(addr);
    __msp_set(*spinitial);
    arch_asmgoto(*entryptr);
}

void bhal_boot (void *addr)
{
    __bhal_boot(((arch_word_t)addr));
    assert(0);
}

d_bool bhal_prog_exist (arch_word_t *progaddr, void *progdata, size_t progsize)
{
    proghdr_t hdr;
    progsize = progsize / sizeof(arch_word_t);
    uint32_t pad = sizeof(arch_word_t) - 1;

    assert(!((arch_word_t)progdata & pad));
    bhal_prog_read_chunk((arch_word_t *)&hdr, progaddr + progsize, sizeof(hdr) / sizeof(arch_word_t));

    //if (hdr.size != progsize) {
    //    return d_false;
    //}
    if (bhal_prog_handle_func(NULL, &func_compare, progaddr, progdata, progsize)) {
        return d_false;
    }
    return d_true;
}

int bhal_load_program (cplthook_t cplth, arch_word_t *progaddr,
                            void *progdata, size_t progsize)
{
    int err = 0;
    bhal_prog_begin();
    err = bhal_prog_erase(cplth, progaddr, progdata, progsize);
    if (err >= 0) {
        err = bhal_prog_handle_func(cplth, &func_write, progaddr, progdata, progsize);
    }
    bhal_prog_end();
    if (err >= 0) {
        err = bhal_prog_handle_func(cplth, &func_compare, progaddr, progdata, progsize);
    }
    return err;
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
