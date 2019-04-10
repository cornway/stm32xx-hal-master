/**
  ******************************************************************************
  * @file    adv7533.c
  * @author  MCD Application Team
  * @brief   This file provides the ADV7533 DSI to HDMI bridge driver 
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "adv7533.h"
#include "hdmi_pub.h"
#include "debug.h"
#include "main.h"

/** @addtogroup BSP
  * @{
  */
  
/** @addtogroup Components
  * @{
  */ 

/** @defgroup ADV7533 ADV7533
  * @brief     This file provides a set of functions needed to drive the 
  *            adv7533 DSI-HDMI bridge.
  * @{
  */
    
/* Private types -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private constants ---------------------------------------------------------*/
/** @defgroup ADV7533_Private_Constants ADV7533 Private Constants
  * @{
  */

/**
  * @}
  */

/* Private macros ------------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/** @defgroup ADV7533_Exported_Variables
  * @{
  */

AUDIO_DrvTypeDef adv7533_drv = 
{
  adv7533_AudioInit,
  adv7533_DeInit,
  adv7533_ReadID,
  adv7533_Play,
  adv7533_Pause,
  adv7533_Resume,
  adv7533_Stop,  
  adv7533_SetFrequency,
  adv7533_SetVolume, /* Not supported, added for compatibility */
  adv7533_SetMute,  
  adv7533_SetOutputMode, /* Not supported, added for compatibility */ 
  adv7533_Reset /* Not supported, added for compatibility */
};

/**
  * @}
  */
   
/* Exported functions --------------------------------------------------------*/
/** @defgroup ADV7533_Exported_Functions ADV7533 Exported Functions
  * @{
  */

#define RDY_INTR_BP 2
#define HPD_INTR_BP 7
#define MON_INTR_BP 6

#define TXPWR_CTL_BP 6

extern void HDMI_IO_Read_Buf (uint8_t Addr, uint8_t Regm, uint8_t *buf, size_t size);

static uint8_t __read_group (uint8_t addr, uint8_t reg, uint8_t shift, uint8_t mask)
{
    uint8_t tmp;

    tmp = HDMI_IO_Read(addr, reg);
    return (tmp >> shift) & mask;
}

static uint8_t __read_val (uint8_t addr, uint8_t reg)
{
    return __read_group(addr, reg, 0, ~0);
}

static int __write_group (uint8_t addr, uint8_t reg, uint8_t shift, uint8_t mask, uint8_t value, int timeout)
{
    uint8_t tmp;

    tmp = __read_group(addr, reg, 0, ~0);

    value = value & mask;
    tmp = tmp & ~(mask << shift);
    tmp = tmp | (value << shift);

    HDMI_IO_Write(addr, reg, tmp);
    tmp = (tmp >> shift) & mask;
    while ((timeout--) && (tmp != value)) {
        HAL_Delay(1);
        tmp = __read_group(addr, reg, shift, mask);
    }
    if (timeout == 0) {
        return -1;
    }
    return 0;
}

static int __write_val (uint8_t addr, uint8_t reg, uint8_t val, int timeout)
{
    __write_group(addr, reg, 0, ~0, val, timeout);
    return 0;
}

static uint8_t __read_bit (uint8_t addr, uint8_t reg, uint8_t bit)
{
    return __read_group(addr, reg, bit, 0x1);
}

static int __write_bit (uint8_t addr, uint8_t reg, uint8_t bit, uint8_t value, int timeout)
{
    return __write_group(addr, reg, bit, 0x1, value, timeout);
}

static void adv7533_set_intr_ena (int value, uint8_t intr)
{
    __write_bit(ADV7533_MAIN_I2C_ADDR, 0x94, intr, value, -1);
}

static int adv7533_get_intr_ena (uint8_t intr)
{
    return __read_bit(ADV7533_MAIN_I2C_ADDR, 0x94, intr);
}

static void adv7533_set_intr_stat (int value, uint8_t intr)
{
    __write_bit(ADV7533_MAIN_I2C_ADDR, 0x96, intr, value, -1);
}

static int adv7533_get_intr_stat (uint8_t intr)
{
    return __read_bit(ADV7533_MAIN_I2C_ADDR, 0x96, intr);
}

static int adv7533_wait_intr (int timeout, uint8_t intr)
{
    adv7533_set_intr_ena(1, intr);

    while ((timeout--) && !adv7533_get_intr_stat(intr)) {
        HAL_Delay(1);
    }

    adv7533_set_intr_ena(0, intr);
    return timeout ? 0 : -1;
}

#define floor2(v, a) ((v) & ~(a))

#define _dump_hex(name, buf, len) __dump_hex(__func__, name, buf, len)

void __dump_hex (const char *func, const char *name, uint8_t *buf, int len)
{
    int i;

    dprintf("%s() : DUMP [0-%d] :\n", func, len);
    for (i = 0; i < floor2(len, 4); i += 4) {
        dprintf("%s[%02x-%02x] %02x %02x %02x %02x\n",
        name,
        i, i + 3,
        buf[i], buf[i + 1],
        buf[i + 2], buf[i + 3]);
    }
    if (i >= len) {
        return;
    }
    dprintf("%s[%02x-%02x]", name, i, len);
    for (; i < len; i++) {
        dprintf("%02x ", buf[i]);
    }
    dprintf("\nDUMP Exit\n");
}

#define _dump_i2c_hex(name, addr, off, len) __dump_i2c_hex(__func__, name, addr, off, len)

void __dump_i2c_hex (const char *func, const char *name, uint8_t addr, uint8_t offset, uint8_t len)
{
    int reg, i;
    uint8_t buf[256];
    len = len & 0xff;

    for (i = 0, reg = offset; i < len; i++, reg++) {
        buf[i] = __read_val(addr, reg);
    }
    __dump_hex(func, name, buf, len);
}

/*FIXME !!! : handle interrupt with care, remove delay(s)*/
static int ADV7533_EDID_Read_Begin (void)
{
    /* initiate edid read in adv7533 */
    int ret;

    //adv7533_wait_intr(-1, HPD_INTR_BP);

    ADV7533_PowerOn();

    adv7533_wait_intr(-1, RDY_INTR_BP);

    ret = __write_group(ADV7533_MAIN_I2C_ADDR, 0xC9, 0, 0x13, 0x13, -1);

    return ret;
}

static int ADV7533_EDID_Read_End (void)
{
    __write_group(ADV7533_MAIN_I2C_ADDR, 0xC9, 0, 0x13, 0, -1);

    ADV7533_PowerDown();

    return 0;
}

static int ADV7533_Read_EDID (uint32_t size, uint8_t *edid_buf)
{
    uint8_t edid_addr;
    if (!edid_buf)
        return -1;

    ADV7533_EDID_Read_Begin();
    HAL_Delay(500);
    dprintf("%s: size %d\n", __func__, size);

    edid_addr = HDMI_IO_Read(ADV7533_MAIN_I2C_ADDR, 0x43);

    dprintf("%s: edid address 0x%x\n", __func__, edid_addr);

    HDMI_IO_Read_Buf(edid_addr, 0x00, edid_buf, size);

    _dump_hex("EDID", edid_buf, size);

    ADV7533_EDID_Read_End();

    return size;
}

void ADV7533_DumpRegs (void)
{
    _dump_i2c_hex("MAIN_I2C_ADDR", ADV7533_MAIN_I2C_ADDR, 0, 0xff);
    _dump_i2c_hex("DSI_I2C_ADDR", ADV7533_CEC_DSI_I2C_ADDR, 0, 0xff);
}

int ADV7533_EDID_Size (void)
{
    return EDID_SEG_SIZE;
}

int ADV7533_Get_EDID (hdmi_edid_seg_t *edid, int size)
{
    if (size < 0) {
        size = ADV7533_EDID_Size();
    }

    return ADV7533_Read_EDID(size, edid->raw);
}

/**
  * @brief  Initializes the ADV7533 bridge.
  * @param  None
  * @retval Status
  */
uint8_t ADV7533_Init(void)
{
  HDMI_IO_Init();

  /* Configure the IC2 address for CEC_DSI interface */
  __write_group(ADV7533_MAIN_I2C_ADDR, 0xE1, 0, ~0, ADV7533_CEC_DSI_I2C_ADDR, -1);
  return 0;
}

/**
  * @brief  Power on the ADV7533 bridge.
  * @param  None
  * @retval None
  */
void ADV7533_PowerOn(void)
{
  /* Power on */
  __write_bit(ADV7533_MAIN_I2C_ADDR, 0x41, TXPWR_CTL_BP, 0, -1);
}

/**
  * @brief  Power off the ADV7533 bridge.
  * @param  None
  * @retval None
  */
void ADV7533_PowerDown(void)
{
   /* Power down */
   __write_bit(ADV7533_MAIN_I2C_ADDR, 0x41, TXPWR_CTL_BP, 1, -1);
}

/**
  * @brief  Configure the DSI-HDMI ADV7533 bridge for video.
  * @param config : pointer to adv7533ConfigTypeDef that contains the
  *                 video configuration parameters
  * @retval None
  */
void ADV7533_Configure(uint8_t dsi_lanes)
{
  uint8_t tmp;
  
  /* Sequence from Section 3 - Quick Start Guide */
  
  /* ADV7533 Power Settings */
  /* Power down */
  tmp = HDMI_IO_Read(ADV7533_MAIN_I2C_ADDR, 0x41);
  tmp &= ~0x40;
  HDMI_IO_Write(ADV7533_MAIN_I2C_ADDR, 0x41, tmp);
  /* HPD Override */
  tmp = HDMI_IO_Read(ADV7533_MAIN_I2C_ADDR, 0xD6);
  tmp |= 0x40;
  HDMI_IO_Write(ADV7533_MAIN_I2C_ADDR, 0xD6, tmp);
  /* Gate DSI LP Oscillator and DSI Bias Clock Powerdown */
  tmp = HDMI_IO_Read(ADV7533_CEC_DSI_I2C_ADDR, 0x03);
  tmp &= ~0x02;
  HDMI_IO_Write(ADV7533_CEC_DSI_I2C_ADDR, 0x03, tmp);
  
  /* Fixed registers that must be set on power-up */
  tmp = HDMI_IO_Read(ADV7533_MAIN_I2C_ADDR, 0x16);
  tmp &= ~0x3E;
  tmp |= 0x20; 
  HDMI_IO_Write(ADV7533_MAIN_I2C_ADDR, 0x16, tmp);
  HDMI_IO_Write(ADV7533_MAIN_I2C_ADDR, 0x9A, 0xE0);
  tmp = HDMI_IO_Read(ADV7533_MAIN_I2C_ADDR, 0xBA);
  tmp &= ~0xF8;
  tmp |= 0x70; 
  HDMI_IO_Write(ADV7533_MAIN_I2C_ADDR, 0xBA, tmp);
  HDMI_IO_Write(ADV7533_MAIN_I2C_ADDR, 0xDE, 0x82);
  
  tmp = HDMI_IO_Read(ADV7533_MAIN_I2C_ADDR, 0xE4); 
  tmp |= 0x40;
  HDMI_IO_Write(ADV7533_MAIN_I2C_ADDR, 0xE4, tmp);
  HDMI_IO_Write(ADV7533_MAIN_I2C_ADDR, 0xE5, 0x80);
  
  tmp = HDMI_IO_Read(ADV7533_CEC_DSI_I2C_ADDR, 0x15);
  tmp &= ~0x30;
  tmp |= 0x10; 
  tmp = HDMI_IO_Read(ADV7533_CEC_DSI_I2C_ADDR, 0x17);
  tmp &= ~0xF0;
  tmp |= 0xD0; 
  HDMI_IO_Write(ADV7533_CEC_DSI_I2C_ADDR, 0x17, tmp);
  tmp = HDMI_IO_Read(ADV7533_CEC_DSI_I2C_ADDR, 0x24);
  tmp &= ~0x10;
  HDMI_IO_Write(ADV7533_CEC_DSI_I2C_ADDR, 0x24, tmp);
  tmp = HDMI_IO_Read(ADV7533_CEC_DSI_I2C_ADDR, 0x57);
  tmp |= 0x01;
  tmp |= 0x10;
  HDMI_IO_Write(ADV7533_CEC_DSI_I2C_ADDR, 0x57, tmp);
  
  /* Configure the number of DSI lanes */
  HDMI_IO_Write(ADV7533_CEC_DSI_I2C_ADDR, 0x1C, (dsi_lanes << 4));
  
  /* Setup video output mode */
  /* Select HDMI mode */
  tmp = HDMI_IO_Read(ADV7533_MAIN_I2C_ADDR, 0xAF);
  tmp |= 0x02;
  HDMI_IO_Write(ADV7533_MAIN_I2C_ADDR, 0xAF, tmp); 
  /* HDMI Output Enable */
  tmp = HDMI_IO_Read(ADV7533_CEC_DSI_I2C_ADDR, 0x03);
  tmp |= 0x80;
  HDMI_IO_Write(ADV7533_CEC_DSI_I2C_ADDR, 0x03, tmp);

  /* GC packet enable */
  tmp = HDMI_IO_Read(ADV7533_MAIN_I2C_ADDR, 0x40);
  tmp |= 0x80;
  HDMI_IO_Write(ADV7533_MAIN_I2C_ADDR, 0x40, tmp);
  /* Input color depth 24-bit per pixel */
  tmp = HDMI_IO_Read(ADV7533_MAIN_I2C_ADDR, 0x4C);
  tmp &= ~0x0F;
  tmp |= 0x03;
  HDMI_IO_Write(ADV7533_MAIN_I2C_ADDR, 0x4C, tmp);
  /* Down dither output color depth */
  HDMI_IO_Write(ADV7533_MAIN_I2C_ADDR, 0x49, 0xfc);
  
  /* Internal timing disabled */
  tmp = HDMI_IO_Read(ADV7533_CEC_DSI_I2C_ADDR, 0x27);
  tmp &= ~0x80;
  HDMI_IO_Write(ADV7533_CEC_DSI_I2C_ADDR, 0x27, tmp);
}


/**
  * @brief  Enable video pattern generation.
  * @param  None
  * @retval None
  */
void ADV7533_PatternEnable(void)
{
  /* Timing generator enable */
  HDMI_IO_Write(ADV7533_CEC_DSI_I2C_ADDR, 0x55, 0x80); /* Color bar */
  HDMI_IO_Write(ADV7533_CEC_DSI_I2C_ADDR, 0x55, 0xA0); /* Color ramp */
  
  HDMI_IO_Write(ADV7533_CEC_DSI_I2C_ADDR, 0x03, 0x89);
  HDMI_IO_Write(ADV7533_CEC_DSI_I2C_ADDR, 0xAF, 0x16);
}

/**
  * @brief  Disable video pattern generation.
  * @param  none
  * @retval none
  */
void ADV7533_PatternDisable(void)
{
  /* Timing generator enable */
  HDMI_IO_Write(ADV7533_CEC_DSI_I2C_ADDR, 0x55, 0x00);
}

/**
  * @brief Initializes the ADV7533 audio  interface.
  * @param DeviceAddr: Device address on communication Bus.   
  * @param OutputDevice: Not used (for compatiblity only).   
  * @param Volume: Not used (for compatiblity only).   
  * @param AudioFreq: Audio Frequency 
  * @retval 0 if correct communication, else wrong communication
  */
uint32_t adv7533_AudioInit(uint16_t DeviceAddr, uint16_t OutputDevice, uint8_t Volume,uint32_t AudioFreq)
{
  uint32_t val = 4096;
  uint8_t  tmp = 0;

  /* Audio data enable*/
  tmp = HDMI_IO_Read(ADV7533_CEC_DSI_I2C_ADDR, 0x05);
  tmp &= ~0x20;
  HDMI_IO_Write(ADV7533_CEC_DSI_I2C_ADDR, 0x05, tmp);

  /* HDMI statup */
  tmp= (uint8_t)((val & 0xF0000)>>16);
  HDMI_IO_Write(DeviceAddr, 0x01, tmp);

  tmp= (uint8_t)((val & 0xFF00)>>8);
  HDMI_IO_Write(DeviceAddr, 0x02, tmp);

  tmp= (uint8_t)((val & 0xFF));
  HDMI_IO_Write(DeviceAddr, 0x03, tmp);

  /* Enable spdif */
  tmp = HDMI_IO_Read(DeviceAddr, 0x0B);
  tmp |= 0x80;
  HDMI_IO_Write(DeviceAddr, 0x0B, tmp);

  /* Enable I2S */
  tmp = HDMI_IO_Read(DeviceAddr, 0x0C);
  tmp |=0x04;
  HDMI_IO_Write(DeviceAddr, 0x0C, tmp);

  /* Set audio sampling frequency */
  adv7533_SetFrequency(DeviceAddr, AudioFreq);

  /* Select SPDIF is 0x10 , I2S=0x00  */
  tmp = HDMI_IO_Read(ADV7533_MAIN_I2C_ADDR, 0x0A);
  tmp &=~ 0x10;
  HDMI_IO_Write(DeviceAddr, 0x0A, tmp);

  /* Set v1P2 enable */
  tmp = HDMI_IO_Read(DeviceAddr, 0xE4);
  tmp |= 0x80;
  HDMI_IO_Write(DeviceAddr, 0xE4, tmp);
 
  return 0;
}

/**
  * @brief  Deinitializes the adv7533
  * @param  None
  * @retval  None
  */
void adv7533_DeInit(void)
{
  /* Deinitialize Audio adv7533 interface */
  AUDIO_IO_DeInit();
}

/**
  * @brief  Get the adv7533 ID.
  * @param DeviceAddr: Device address on communication Bus.
  * @retval The adv7533 ID 
  */
uint32_t adv7533_ReadID(uint16_t DeviceAddr)
{
  uint32_t  tmp = 0;
  
  tmp = HDMI_IO_Read(DeviceAddr, ADV7533_CHIPID_ADDR0);
  tmp = (tmp<<8);
  tmp |= HDMI_IO_Read(DeviceAddr, ADV7533_CHIPID_ADDR1);
  
  return(tmp);
}

/**
  * @brief Pauses playing on the audio hdmi
  * @param DeviceAddr: Device address on communication Bus. 
  * @retval 0 if correct communication, else wrong communication
  */
uint32_t adv7533_Pause(uint16_t DeviceAddr)
{ 
  return(adv7533_SetMute(DeviceAddr,AUDIO_MUTE_ON));
}       
            
/**
  * @brief Resumes playing on the audio hdmi.
  * @param DeviceAddr: Device address on communication Bus. 
  * @retval 0 if correct communication, else wrong communication
  */   
uint32_t adv7533_Resume(uint16_t DeviceAddr)
{ 
  return(adv7533_SetMute(DeviceAddr,AUDIO_MUTE_OFF));
} 

/**
  * @brief Start the audio hdmi play feature.
  * @note  For this codec no Play options are required.
  * @param DeviceAddr: Device address on communication Bus.   
  * @retval 0 if correct communication, else wrong communication
  */
uint32_t adv7533_Play(uint16_t DeviceAddr ,uint16_t* pBuffer  ,uint16_t Size)
{
  return(adv7533_SetMute(DeviceAddr,AUDIO_MUTE_OFF));
}
            
/**
  * @brief Stop playing on the audio hdmi
  * @param DeviceAddr: Device address on communication Bus. 
  * @retval 0 if correct communication, else wrong communication
  */
uint32_t adv7533_Stop(uint16_t DeviceAddr,uint32_t cmd)
{ 
  return(adv7533_SetMute(DeviceAddr,AUDIO_MUTE_ON));
}               
            
/**
  * @brief Enables or disables the mute feature on the audio hdmi.
  * @param DeviceAddr: Device address on communication Bus.   
  * @param Cmd: AUDIO_MUTE_ON to enable the mute or AUDIO_MUTE_OFF to disable the
  *             mute mode.
  * @retval 0 if correct communication, else wrong communication
  */
uint32_t adv7533_SetMute(uint16_t DeviceAddr, uint32_t Cmd)
{
  uint8_t tmp = 0;
  
  tmp = HDMI_IO_Read(DeviceAddr, 0x0D);
  if (Cmd == AUDIO_MUTE_ON)  
  {
    /* enable audio mute*/ 
    tmp |= 0x40;
    HDMI_IO_Write(DeviceAddr, 0x0D, tmp);
  }
  else
  {
    /*audio mute off disable the mute */
    tmp &= ~0x40;
    HDMI_IO_Write(DeviceAddr, 0x0D, tmp);
  }
  return 0;
}

/**
  * @brief Sets output mode.
  * @param DeviceAddr: Device address on communication Bus.
  * @param Output : hdmi output.
  * @retval 0 if correct communication, else wrong communication
  */
uint32_t adv7533_SetOutputMode(uint16_t DeviceAddr, uint8_t Output)
{
  return 0;
}    
            
/**
  * @brief Sets volumee.
  * @param DeviceAddr: Device address on communication Bus.
  * @param Volume : volume value.
  * @retval 0 if correct communication, else wrong communication
  */           
uint32_t adv7533_SetVolume(uint16_t DeviceAddr, uint8_t Volume)
{
 return 0;
}
            
/**
  * @brief Resets adv7533 registers.
  * @param DeviceAddr: Device address on communication Bus. 
  * @retval 0 if correct communication, else wrong communication
  */
uint32_t adv7533_Reset(uint16_t DeviceAddr)
{
  return 0;
}

/**
  * @brief Sets new frequency.
  * @param DeviceAddr: Device address on communication Bus.
  * @param AudioFreq: Audio frequency used to play the audio stream.
  * @retval 0 if correct communication, else wrong communication
  */
uint32_t adv7533_SetFrequency(uint16_t DeviceAddr, uint32_t AudioFreq)
{
  uint8_t tmp = 0;

  tmp = HDMI_IO_Read(DeviceAddr, 0x15);
  tmp &= (~0xF0);
  /*  Clock Configurations */
  switch (AudioFreq)
  {
  case  AUDIO_FREQUENCY_32K:
    /* Sampling Frequency =32 KHZ*/
    tmp |= 0x30;
    HDMI_IO_Write(DeviceAddr, 0x15, tmp);
    break;
  case  AUDIO_FREQUENCY_44K: 
    /* Sampling Frequency =44,1 KHZ*/
    tmp |= 0x00;
    HDMI_IO_Write(DeviceAddr, 0x15, tmp);
    break;
    
  case  AUDIO_FREQUENCY_48K: 
    /* Sampling Frequency =48KHZ*/
    tmp |= 0x20;
    HDMI_IO_Write(DeviceAddr, 0x15, tmp);
    break;
    
  case  AUDIO_FREQUENCY_96K: 
    /* Sampling Frequency =96 KHZ*/
    tmp |= 0xA0;
    HDMI_IO_Write(DeviceAddr, 0x15, tmp);
    break;
    
  case  AUDIO_FREQUENCY_88K: 
    /* Sampling Frequency =88,2 KHZ*/
    tmp |= 0x80;
    HDMI_IO_Write(DeviceAddr, 0x15, tmp);
    break;
    
  case  AUDIO_FREQUENCY_176K: 
    /* Sampling Frequency =176,4 KHZ*/
    tmp |= 0xC0;
    HDMI_IO_Write(DeviceAddr, 0x15, tmp);
    break;
    
  case  AUDIO_FREQUENCY_192K: 
    /* Sampling Frequency =192KHZ*/
    tmp |= 0xE0;
    HDMI_IO_Write(DeviceAddr, 0x15, tmp);
    break;
  }
  return 0;
}

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
