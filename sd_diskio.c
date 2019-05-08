/**
  ******************************************************************************
  * @file    sd_diskio.c
  * @author  MCD Application Team
  * @brief   SD Disk I/O driver.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics International N.V.
  * All rights reserved.</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice,
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other
  *    contributors to this software may be used to endorse or promote products
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under
  *    this license is void and will automatically terminate your rights under
  *    this license.
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "string.h"
#include "ff_gen_drv.h"
#include "sd_diskio.h"
#include "stm32f769i_discovery_audio.h"
#include "debug.h"
#include "nvic.h"
#include "heap.h"
#include <misc_utils.h>

#define SD_MODE_DMA 0
#define SD_UNALIGNED_WA 1

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* use the default SD timout as defined in the platform BSP driver*/
#define SD_TIMEOUT 3 * 1000

#define SD_DEFAULT_BLOCK_SIZE 512
#define SD_BLOCK_SECTOR_CNT (_MIN_SS / SD_DEFAULT_BLOCK_SIZE)

/*
 * Depending on the usecase, the SD card initialization could be done at the
 * application level, if it is the case define the flag below to disable
 * the BSP_SD_Init() call in the SD_Initialize().
 */

/* #define DISABLE_SD_INIT */

/* Private variables ---------------------------------------------------------*/
/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;

#if SD_MODE_DMA
static volatile  UINT  WriteStatus = 0, ReadStatus = 0;
#endif /*SD_MODE_DMA*/

extern void hdd_led_on (void);
extern void hdd_led_off (void);

/* Private function prototypes -----------------------------------------------*/
static DSTATUS SD_CheckStatus(BYTE lun);
DSTATUS SD_initialize (BYTE);
DSTATUS SD_status (BYTE);
DRESULT SD_read (BYTE, BYTE*, DWORD, UINT);
#if _USE_WRITE == 1
  DRESULT SD_write (BYTE, const BYTE*, DWORD, UINT);
#endif /* _USE_WRITE == 1 */
#if _USE_IOCTL == 1
  DRESULT SD_ioctl (BYTE, BYTE, void*);
#endif  /* _USE_IOCTL == 1 */

const Diskio_drvTypeDef  SD_Driver =
{
  SD_initialize,
  SD_status,
  SD_read,
#if  _USE_WRITE == 1
  SD_write,
#endif /* _USE_WRITE == 1 */

#if  _USE_IOCTL == 1
  SD_ioctl,
#endif /* _USE_IOCTL == 1 */
};

/* Private functions ---------------------------------------------------------*/
static DSTATUS SD_CheckStatus(BYTE lun)
{
  Stat = STA_NOINIT;
  if(BSP_SD_GetCardState() == MSD_OK)
  {
    Stat &= ~STA_NOINIT;
  }

  return Stat;
}

/**
  * @brief  Initializes a Drive
  * @param  lun : not used
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_initialize(BYTE lun)
{
  Stat = STA_NOINIT;
#if !defined(DISABLE_SD_INIT)

  if(BSP_SD_Init() == MSD_OK)
  {
    Stat = SD_CheckStatus(lun);
  }

#else
  Stat = SD_CheckStatus(lun);
#endif
  return Stat;
}

/**
  * @brief  Gets Disk Status
  * @param  lun : not used
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_status(BYTE lun)
{
    return SD_CheckStatus(lun);
}

#if SD_UNALIGNED_WA

#if (_MIN_SS != _MAX_SS)
#error "Unsupported mode"
#endif

static uint8_t sd_local_buf[_MAX_SS] ALIGN(4);

DRESULT SD_Uread(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
    uint8_t ret = MSD_OK;
    DWORD end = sector + count;

    for (; sector < end;) {
        ret = BSP_SD_ReadBlocks((uint32_t *)sd_local_buf,
                                (uint32_t)sector,
                                SD_BLOCK_SECTOR_CNT,
                                SD_TIMEOUT);
        if (ret == MSD_OK) {
           while(BSP_SD_GetCardState()!= MSD_OK) {} 
        } else {
           return RES_ERROR;
        }
        d_memcpy(buff, sd_local_buf, _MIN_SS);
        sector += SD_BLOCK_SECTOR_CNT;
        buff += _MIN_SS;
    }
    return RES_OK;
}

#endif /*SD_UNALIGNED_WA*/

#if SD_MODE_DMA

/**
  * @brief  Reads Sector(s)
  * @param  lun : not used
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT _SD_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
  DRESULT res = RES_ERROR;
  ReadStatus = 0;
  uint32_t timeout;
#if (ENABLE_SD_DMA_CACHE_MAINTENANCE == 1)
  uint32_t alignedAddr;
#endif

  if(BSP_SD_ReadBlocks_DMA((uint32_t*)buff,
                           (uint32_t) (sector),
                           count) == MSD_OK)
  {
    /* Wait that the reading process is completed or a timeout occurs */
    timeout = HAL_GetTick();
    while((ReadStatus == 0) && ((HAL_GetTick() - timeout) < SD_TIMEOUT))
    {
    }
    /* incase of a timeout return error */
    if (ReadStatus == 0)
    {
      res = RES_ERROR;
    }
    else
    {
      ReadStatus = 0;
      timeout = HAL_GetTick();

      while((HAL_GetTick() - timeout) < SD_TIMEOUT)
      {
        if (BSP_SD_GetCardState() == SD_TRANSFER_OK)
        {
          res = RES_OK;
#if (ENABLE_SD_DMA_CACHE_MAINTENANCE == 1)
            /*
               the SCB_InvalidateDCache_by_Addr() requires a 32-Byte aligned address,
               adjust the address and the D-Cache size to invalidate accordingly.
             */
            alignedAddr = (uint32_t)buff & ~0x1F;
            SCB_InvalidateDCache_by_Addr((uint32_t*)alignedAddr, count*BLOCKSIZE + ((uint32_t)buff - alignedAddr));
#endif
           break;
        }
      }
    }
  }
  return res;
}


#else

/**
  * @brief  Reads Sector(s)
  * @param  lun : not used
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */

DRESULT _SD_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
  DRESULT res = RES_ERROR;
  uint8_t msd_res = MSD_ERROR;
#if SD_UNALIGNED_WA

  if (((uint32_t)buff) & 0x3) {
     res = SD_Uread(lun, buff, sector, count);
  } else
#endif /*SD_UNALIGNED_WA*/
  {
    msd_res = BSP_SD_ReadBlocks((uint32_t *)buff,
                          (uint32_t)sector,
                          count,
                          SD_TIMEOUT);
    if (msd_res == MSD_OK) {
        res = RES_OK;
        while(BSP_SD_GetCardState()!= MSD_OK) {} 
    }
  }
  return res;
}

#endif /*SD_MODE_DMA*/


DRESULT SD_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
    DRESULT res;
    irqmask_t irq_flags = 0;
    hdd_led_on();

    irq_save(&irq_flags);
    res = _SD_read(lun, buff, sector, count * SD_BLOCK_SECTOR_CNT);
    irq_restore(irq_flags);

    hdd_led_off();
    return res;
}

/**
  * @brief  Writes Sector(s)
  * @param  lun : not used
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
#if SD_MODE_DMA

/**
  * @brief  Writes Sector(s)
  * @param  lun : not used
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
static DRESULT __SD_write (BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
  DRESULT res = RES_ERROR;
  WriteStatus = 0;
  uint32_t timeout;

#if (ENABLE_SD_DMA_CACHE_MAINTENANCE == 1)
  uint32_t alignedAddr;
  /*
   the SCB_CleanDCache_by_Addr() requires a 32-Byte aligned address
   adjust the address and the D-Cache size to clean accordingly.
   */
  alignedAddr = (uint32_t)buff &  ~0x1F;
  SCB_CleanDCache_by_Addr((uint32_t*)alignedAddr, count*BLOCKSIZE + ((uint32_t)buff - alignedAddr));
#endif

  if(BSP_SD_WriteBlocks_DMA((uint32_t*)buff,
                            (uint32_t)(sector),
                            count) == MSD_OK)
  {
    /* Wait that writing process is completed or a timeout occurs */

    timeout = HAL_GetTick();
    while((WriteStatus == 0) && ((HAL_GetTick() - timeout) < SD_TIMEOUT))
    {
    }
    /* incase of a timeout return error */
    if (WriteStatus == 0)
    {
      res = RES_ERROR;
    }
    else
    {
      WriteStatus = 0;
      timeout = HAL_GetTick();

      while((HAL_GetTick() - timeout) < SD_TIMEOUT)
      {
        if (BSP_SD_GetCardState() == SD_TRANSFER_OK)
        {
          res = RES_OK;
          break;
        }
      }
    }
  }
  return res;
}

#else /*SD_MODE_DMA*/

static DRESULT __SD_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
  uint8_t ret = RES_ERROR;
  DRESULT res = RES_OK;
  ret = BSP_SD_WriteBlocks((uint32_t*)buff,
                           (uint32_t)sector,
                           count,
                           SD_TIMEOUT);

  if (ret == MSD_OK) {
    while(BSP_SD_GetCardState()!= MSD_OK) {} 
  } else {
    res = RES_ERROR;
  }
  return res;
}

#endif /*SD_MODE_DMA*/

#if SD_UNALIGNED_WA

static DRESULT SD_UWrite (BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
    uint8_t ret = MSD_OK;
    DWORD end = sector + count;

    for (; sector < end;) {
        d_memcpy(sd_local_buf, buff, _MIN_SS);
        ret = BSP_SD_WriteBlocks((uint32_t *)sd_local_buf,
                                (uint32_t)sector,
                                SD_BLOCK_SECTOR_CNT,
                                SD_TIMEOUT);
        if (ret == MSD_OK) {
           while(BSP_SD_GetCardState()!= MSD_OK) {} 
        } else {
           return RES_ERROR;
        }
        
        sector += SD_BLOCK_SECTOR_CNT;
        buff += _MIN_SS;
    }
    return RES_OK;
}

#endif /*SD_UNALIGNED_WA*/

static DRESULT _SD_write (BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
    DRESULT res = RES_OK;
#if SD_UNALIGNED_WA
    if ((uint32_t)buff & 0x3) {
        res = SD_UWrite(lun, buff, sector, count);
    } else
#endif
    {
        res = __SD_write(lun, buff, sector, count);
    }
    if (res == RES_OK) {
        while(BSP_SD_GetCardState()!= MSD_OK) {} 
    }
    return res;
}

DRESULT SD_write (BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
    DRESULT res;
    irqmask_t irq_flags = 0;
    hdd_led_on();
    irq_save(&irq_flags);

    res = _SD_write(lun, buff, sector, count);

    irq_restore(irq_flags);
    hdd_led_off();
    return res;
}

#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation
  * @param  lun : not used
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff)
{
  DRESULT res = RES_ERROR;
  BSP_SD_CardInfo CardInfo;
  if (Stat & STA_NOINIT) return RES_NOTRDY;

  switch (cmd)
  {
  /* Make sure that no pending write process */
  case CTRL_SYNC :
    res = RES_OK;
    break;

  /* Get number of sectors on the disk (DWORD) */
  case GET_SECTOR_COUNT :
    BSP_SD_GetCardInfo(&CardInfo);
    *(DWORD*)buff = CardInfo.LogBlockNbr;
    res = RES_OK;
    break;

  /* Get R/W sector size (WORD) */
  case GET_SECTOR_SIZE :
    BSP_SD_GetCardInfo(&CardInfo);
    *(WORD*)buff = CardInfo.LogBlockSize;
    res = RES_OK;
    break;

  /* Get erase block size in unit of sector (DWORD) */
  case GET_BLOCK_SIZE :
    BSP_SD_GetCardInfo(&CardInfo);
    *(DWORD*)buff = CardInfo.LogBlockSize / SD_DEFAULT_BLOCK_SIZE;
	res = RES_OK;
    break;

  default:
    res = RES_PARERR;
  }

  return res;
}
#endif /* _USE_IOCTL == 1 */

/**
  * @brief BSP SD Abort callbacks
  * @retval None
  */
void BSP_SD_AbortCallback(void)
{

}
/*
   ===============================================================================
    Select the correct function signature depending on your platform.
    please refer to the file "stm32xxxx_eval_sd.h" to verify the correct function
    prototype
   ===============================================================================
  */
//void BSP_SD_WriteCpltCallback(uint32_t SdCard)
void BSP_SD_WriteCpltCallback(void)
{
#if SD_MODE_DMA
  WriteStatus = 1;
#endif
}

/**
  * @brief Rx Transfer completed callbacks
  * @param hsd: SD handle
  * @retval None
  */

  /*
   ===============================================================================
    Select the correct function signature depending on your platform.
    please refer to the file "stm32xxxx_eval_sd.h" to verify the correct function
    prototype
   ===============================================================================
  */
//void BSP_SD_ReadCpltCallback(uint32_t SdCard)
void BSP_SD_ReadCpltCallback(void)
{
#if SD_MODE_DMA
  ReadStatus = 1;
#endif
}



/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

