#include "main.h"
#include "misc_utils.h"
#include "arch.h"
#include "mpu.h"
#include <string.h>
#include <bsp_sys.h>

#define MPU_REG_POOL_MAX (MPU_REGION_NUMBER7 + 1)

typedef struct {

    MPU_Region_InitTypeDef init;
    d_bool alloced;
} mpu_reg_t;

static mpu_reg_t mpu_reg_pool[MPU_REG_POOL_MAX];

static uint32_t size_to_mpu_size (uint32_t *size_p);

static mpu_reg_t *mpu_alloc_reg (int *id)
{
    int i;
    for (i = 0; i < MPU_REG_POOL_MAX; i++) {
        if (mpu_reg_pool[i].alloced == d_false) {
            mpu_reg_pool[i].alloced = d_true;
            *id = i + MPU_REGION_NUMBER0;
            return &mpu_reg_pool[i];
        }
    }
    return NULL;
}

static void mpu_release_reg (mpu_reg_t *reg)
{
    reg->alloced = d_false;
}

void mpu_init (void)
{
    memset(mpu_reg_pool, 0, sizeof(mpu_reg_pool));
}

int mpu_lock (arch_word_t addr, arch_word_t *size, const char *mode)
{
    d_bool wr_protect = d_false, r_protect = d_false;
    d_bool shareable = d_true, cacheable = d_true, bufferable = d_true;
    d_bool exclude_flags = d_false;
    uint8_t tex = 0;
    mpu_reg_t *reg;
    int id = 0;

    reg = mpu_alloc_reg(&id);
    if (!reg) {
        return -1;
    }

    if (*mode == '-') {
        exclude_flags = d_true;
        mode++;
    }

    while (*mode) {
        switch (*mode) {
            case 'w':
                wr_protect = exclude_flags ? d_true : d_false;
            break;
            case 'r':
                r_protect = exclude_flags ? d_true : d_false;
            break;
            case 's':
                shareable = exclude_flags ? d_false : d_true;
            break;
            case 'c':
                cacheable = exclude_flags ? d_false : d_true;
            break;
            case 'b':
                bufferable = exclude_flags ? d_false : d_true;
            break;
            case 't':
                /*TODO : Extend;*/
                tex = 1;
            break;
            case 'x':
                reg->init.DisableExec = exclude_flags ?
                    MPU_INSTRUCTION_ACCESS_DISABLE : MPU_INSTRUCTION_ACCESS_ENABLE;
            break;
            case ' ':
            break;
            default:
                dprintf("%s() : Unexpected symbols. - [%s]; asserting\n", __func__, mode);
                assert(0);
            break;
        }
        mode++;
    }
    if (r_protect) {
        assert(wr_protect);
        reg->init.AccessPermission = MPU_REGION_NO_ACCESS;
    } else if (wr_protect) {
        reg->init.AccessPermission = MPU_REGION_PRIV_RO;
    }
    switch (tex) {
        case 0: reg->init.TypeExtField = MPU_TEX_LEVEL0;
        break;
        case 1: reg->init.TypeExtField = MPU_TEX_LEVEL1;
        break;
    }

    reg->init.Number = id;
    reg->init.Enable = MPU_REGION_ENABLE;
    reg->init.BaseAddress = addr;
    reg->init.IsShareable = shareable ? MPU_ACCESS_SHAREABLE : MPU_ACCESS_NOT_SHAREABLE;
    reg->init.IsCacheable = cacheable ?  MPU_ACCESS_CACHEABLE : MPU_ACCESS_NOT_CACHEABLE;
    reg->init.IsBufferable = bufferable ?  MPU_ACCESS_BUFFERABLE : MPU_ACCESS_NOT_BUFFERABLE;
    reg->init.AccessPermission = MPU_REGION_FULL_ACCESS;
    reg->init.SubRegionDisable = 0;
    reg->init.Size = size_to_mpu_size(size);

    HAL_MPU_Disable();
    HAL_MPU_ConfigRegion(&reg->init);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

    return reg->init.Number;
}

int mpu_unlock (arch_word_t addr, arch_word_t size)
{
    int id = mpu_read(addr, size);
    mpu_reg_t *reg;

    if (id < 0) {
        dprintf("%s() : Unable to find MPU region : <%p> - <%p>\n",
                __func__, (void *)addr, (void *)size);
        return id;
    }

    reg = &mpu_reg_pool[id];
    reg->init.Enable = MPU_REGION_DISABLE;

    HAL_MPU_Disable();
    HAL_MPU_ConfigRegion(&reg->init);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

    mpu_release_reg(reg);
    return 0;
}

static int mpu_search (arch_word_t addr, arch_word_t size)
{
    int i;
    mpu_reg_t *reg;
    int mpu_size;

    for (i = 0; i < MPU_REG_POOL_MAX; i++) {
        reg = &mpu_reg_pool[i];
        if (reg->init.BaseAddress == addr) {
            mpu_size = size_to_mpu_size(&size);
            assert(reg->init.Size == mpu_size);
            return i;
        }
    }
    return -1;
}

int mpu_read (arch_word_t addr, arch_word_t size)
{
    return mpu_search(addr, size);
}

static uint32_t size_to_mpu_size (uint32_t *size_p)
{
    uint32_t size = *size_p;
    uint32_t mpu_reg_size = 0;

    if (size <= 32) {
        size = 32;
        mpu_reg_size = MPU_REGION_SIZE_32B;
    } else if (size <= 64) {
        size = 64;
        mpu_reg_size = MPU_REGION_SIZE_64B;
    } else if (size <= 128) {
        size = 128;
        mpu_reg_size = MPU_REGION_SIZE_128B;
    } else if (size <= 256) {
        size = 256;
        mpu_reg_size = MPU_REGION_SIZE_256B;
    } else if (size <= 512) {
        size = 512;
        mpu_reg_size = MPU_REGION_SIZE_512B;
    } else if (size <= 1024) {
        size = 1024;
        mpu_reg_size = MPU_REGION_SIZE_1KB;
    } else if (size <= 1024 * 2) {
        size = 1024 * 2;
        mpu_reg_size = MPU_REGION_SIZE_2KB;
    } else if (size <= 1024 * 4) {
        size = 1024 * 4;
        mpu_reg_size = MPU_REGION_SIZE_4KB;
    } else if (size <= 1024 * 8) {
        size = 1024 * 8;
        mpu_reg_size = MPU_REGION_SIZE_8KB;
    } else if (size <= 1024 * 16) {
        size = 1024 * 16;
        mpu_reg_size = MPU_REGION_SIZE_16KB;
    } else if (size <= 1024 * 32) {
        size = 1024 * 32;
        mpu_reg_size = MPU_REGION_SIZE_32KB;
    } else if (size <= 1024 * 64) {
        size = 1024 * 64;
        mpu_reg_size = MPU_REGION_SIZE_64KB;
    } else if (size <= 1024 * 128) {
        size = 1024 * 128;
        mpu_reg_size = MPU_REGION_SIZE_128KB;
    } else if (size <= 1024 * 256) {
        size = 1024 * 256;
        mpu_reg_size = MPU_REGION_SIZE_256KB;
    } else if (size <= 1024 * 512) {
        size = 1024 * 512;
        mpu_reg_size = MPU_REGION_SIZE_512KB;
    } else if (size <= 1024 * 1024) {
        size = 1024 * 1024;
        mpu_reg_size = MPU_REGION_SIZE_1MB;
    } else if (size <= 1024 * 1024 * 2) {
        size = 1024 * 1024 * 2;
        mpu_reg_size = MPU_REGION_SIZE_2MB;
    } else if (size <= 1024 * 1024 * 4) {
        size = 1024 * 1024 * 4;
        mpu_reg_size = MPU_REGION_SIZE_4MB;
    } else if (size <= 1024 * 1024 * 8) {
        size = 1024 * 1024 * 8;
        mpu_reg_size = MPU_REGION_SIZE_8MB;
    } else if (size <= 1024 * 1024 * 16) {
        size = 1024 * 1024 * 16;
        mpu_reg_size = MPU_REGION_SIZE_16MB;
    } else if (size <= 1024 * 1024 * 32) {
        size = 1024 * 1024 * 32;
        mpu_reg_size = MPU_REGION_SIZE_32MB;
    } else if (size == 1024 * 1024 * 64) {
        size = 1024 * 1024 * 64;
        mpu_reg_size = MPU_REGION_SIZE_64MB;
    } else if (size <= 1024 * 1024 * 128) {
        size = 1024 * 1024 * 128;
        mpu_reg_size = MPU_REGION_SIZE_128MB;
    } else if (size <= 1024 * 1024 * 256) {
        size = 1024 * 1024 * 256;
        mpu_reg_size = MPU_REGION_SIZE_256MB;
    } else if (size <= 1024 * 1024 * 512) {
        size = 1024 * 1024 * 512;
        mpu_reg_size = MPU_REGION_SIZE_512MB;
    } else if (size <= 1024 * 1024 * 1024) {
        size = 1024 * 1024 * 1024;
        mpu_reg_size = MPU_REGION_SIZE_1GB;
    } else if (size <= 1024 * 1024 * 1024 * 2U) {
        size = 1024 * 1024 * 1024 * 2U;
        mpu_reg_size = MPU_REGION_SIZE_2GB;
    } else if (size <= 1024 * 1024 * 1024 * 4U) {
        size = 1024 * 1024 * 1024 * 4U;
        mpu_reg_size = MPU_REGION_SIZE_4GB;
    }
    *size_p = size;
    assert(mpu_reg_size);
    return mpu_reg_size;
}


