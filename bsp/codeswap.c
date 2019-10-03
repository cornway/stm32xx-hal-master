
#include <misc_utils.h>
#include <heap.h>
#include <debug.h>
#include <mpu.h>

#if defined(HAVE_CODESWAP)

extern char Image$$RW_CODE$$Base;
extern char Image$$RW_CODE$$Length;
extern char Load$$RW_CODE$$Base;
extern char Load$$RW_CODE$$Length;

#define RAMCODE_IMG_BASE    (&Image$$RW_CODE$$Base)
#define RAMCODE_IMG_LENGTH  ((size_t)&Image$$RW_CODE$$Length)
#define RAMCODE_LOAD_BASE   (&Load$$RW_CODE$$Base)
#define RAMCODE_LOAD_LENGTH ((size_t)&Load$$RW_CODE$$Length)

static uint8_t cs_code_ready = 0;

int cs_load_code (void *unused1, void *unused2, int unused3)
{
    size_t size = RAMCODE_IMG_LENGTH;
    if (cs_code_ready) {
        return 0;
    }
    if (RAMCODE_IMG_LENGTH != RAMCODE_LOAD_LENGTH) {
        dprintf("%s() : length differs\n", __func__);
        return -1;
    }
    d_memcpy(RAMCODE_IMG_BASE, RAMCODE_LOAD_BASE, RAMCODE_IMG_LENGTH);
    dprintf("codeswap region: <%p> : 0x%08x bytes\n", RAMCODE_IMG_BASE, RAMCODE_IMG_LENGTH);
    mpu_lock(RAMCODE_IMG_BASE, &size, "xwr");
    cs_code_ready = 1;
    return 0;
}

int cs_check_symb (void *symb)
{
    return !!cs_code_ready;
}

#else

int cs_load_code (void *unused1, void *unused2, int unused3)
{
    return -1;
}

int cs_check_symb (void *symb)
{
    return 1;
}

#endif /*defined(HAVE_CODESWAP)*/

