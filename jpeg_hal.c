/**
  ******************************************************************************
  * @file    JPEG/JPEG_DecodingUsingFs_DMA/Src/decode_dma.c
  * @author  MCD Application Team
  * @brief   This file provides routines for JPEG decoding with DMA method.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <jpeg.h>


#include <misc_utils.h>
#include <dev_io.h>

/** @addtogroup STM32F7xx_HAL_Examples
  * @{
  */

/** @addtogroup JPEG_DecodingUsingFs_DMA
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
typedef struct
{
  uint8_t State;  
  uint8_t *DataBuffer;
  uint32_t DataBufferSize;

}JPEG_Data_BufferTypeDef;

/* Private define ------------------------------------------------------------*/

#define CHUNK_SIZE_IN  ((uint32_t)(4096)) 
#define CHUNK_SIZE_OUT ((uint32_t)(768))

#define JPEG_BUFFER_EMPTY 0
#define JPEG_BUFFER_FULL  1

#define NB_OUTPUT_DATA_BUFFERS      2
#define NB_INPUT_DATA_BUFFERS       2

JPEG_HandleTypeDef    JPEG_Handle;

static DMA2D_HandleTypeDef    DMA2D_Handle;

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
JPEG_YCbCrToRGB_Convert_Function pConvert_Function;
uint8_t *Stream = NULL;     /* pointer to File object */
int StreamPos = 0, StreamSize = 0;

uint8_t MCU_Data_OutBuffer0[CHUNK_SIZE_OUT];
uint8_t MCU_Data_OutBuffer1[CHUNK_SIZE_OUT];

JPEG_Data_BufferTypeDef Jpeg_OUT_BufferTab[NB_OUTPUT_DATA_BUFFERS] =
{
  {JPEG_BUFFER_EMPTY , MCU_Data_OutBuffer0 , 0},
  {JPEG_BUFFER_EMPTY , MCU_Data_OutBuffer1, 0}
};

JPEG_Data_BufferTypeDef Jpeg_IN_BufferTab[NB_INPUT_DATA_BUFFERS] =
{
  {JPEG_BUFFER_EMPTY , NULL, 0},
  {JPEG_BUFFER_EMPTY , NULL, 0}
};


uint32_t MCU_TotalNb = 0;

uint32_t MCU_BlockIndex = 0;
uint32_t Jpeg_HWDecodingEnd = 0;

uint32_t JPEG_OUT_Read_BufferIndex = 0;
uint32_t JPEG_OUT_Write_BufferIndex = 0;
__IO uint32_t Output_Is_Paused = 0;

uint32_t JPEG_IN_Read_BufferIndex = 0;
uint32_t JPEG_IN_Write_BufferIndex = 0;
__IO uint32_t Input_Is_Paused = 0;

uint32_t FrameBufferAddress;

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Decode_DMA
  * @param hjpeg: JPEG handle pointer
  * @param  FileName    : jpg file path for decode.
  * @param  DestAddress : ARGB destination Frame Buffer Address.
  * @retval None
  */
int JPEG_Decode_DMA(JPEG_HandleTypeDef *hjpeg, void *data, uint32_t size, uint32_t DestAddress)
{
    uint32_t i;
    uint8_t *ptr;
    HAL_StatusTypeDef status;

    Stream = data;
    StreamPos = 0;
    StreamSize = size;
    FrameBufferAddress = DestAddress;
    MCU_TotalNb = 0;
    MCU_BlockIndex = 0;
    Jpeg_HWDecodingEnd = 0;
    JPEG_OUT_Read_BufferIndex = 0;
    JPEG_IN_Read_BufferIndex = 0;
    JPEG_IN_Write_BufferIndex = 0;
    Input_Is_Paused = 0;

    for(i = 0; i < NB_INPUT_DATA_BUFFERS; i++)
    {
        Jpeg_IN_BufferTab[i].DataBuffer = Stream + StreamPos;
        StreamPos += CHUNK_SIZE_IN;
        StreamSize -= CHUNK_SIZE_IN;
        Jpeg_IN_BufferTab[i].DataBufferSize = CHUNK_SIZE_IN;
        Jpeg_IN_BufferTab[i].State = JPEG_BUFFER_FULL;
    }

    status = HAL_JPEG_Decode_DMA(hjpeg ,Jpeg_IN_BufferTab[0].DataBuffer ,Jpeg_IN_BufferTab[0].DataBufferSize ,Jpeg_OUT_BufferTab[0].DataBuffer ,CHUNK_SIZE_OUT);

    return status == HAL_OK ? 0 : -1;
}

/**
  * @brief  JPEG Ouput Data BackGround Postprocessing .
  * @param hjpeg: JPEG handle pointer
  * @retval 1 : if JPEG processing has finiched, 0 : if JPEG processing still ongoing
  */
uint32_t JPEG_OutputHandler(JPEG_HandleTypeDef *hjpeg)
{
  uint32_t ConvertedDataCount;
  
  if(Jpeg_OUT_BufferTab[JPEG_OUT_Read_BufferIndex].State == JPEG_BUFFER_FULL)
  {  
    MCU_BlockIndex += pConvert_Function(Jpeg_OUT_BufferTab[JPEG_OUT_Read_BufferIndex].DataBuffer, (uint8_t *)FrameBufferAddress, MCU_BlockIndex, Jpeg_OUT_BufferTab[JPEG_OUT_Read_BufferIndex].DataBufferSize, &ConvertedDataCount);   
    
    Jpeg_OUT_BufferTab[JPEG_OUT_Read_BufferIndex].State = JPEG_BUFFER_EMPTY;
    Jpeg_OUT_BufferTab[JPEG_OUT_Read_BufferIndex].DataBufferSize = 0;
    
    JPEG_OUT_Read_BufferIndex++;
    if(JPEG_OUT_Read_BufferIndex >= NB_OUTPUT_DATA_BUFFERS)
    {
      JPEG_OUT_Read_BufferIndex = 0;
    }
    
    if(MCU_BlockIndex == MCU_TotalNb)
    {
      return 1;
    }
  }
  else if((Output_Is_Paused == 1) && \
          (JPEG_OUT_Write_BufferIndex == JPEG_OUT_Read_BufferIndex) && \
          (Jpeg_OUT_BufferTab[JPEG_OUT_Read_BufferIndex].State == JPEG_BUFFER_EMPTY))
  {
    Output_Is_Paused = 0;
    HAL_JPEG_Resume(hjpeg, JPEG_PAUSE_RESUME_OUTPUT);
  }

  return 0;  
}

/**
  * @brief  JPEG Input Data BackGround processing .
  * @param hjpeg: JPEG handle pointer
  * @retval None
  */
void JPEG_InputHandler(JPEG_HandleTypeDef *hjpeg)
{
  if(Jpeg_IN_BufferTab[JPEG_IN_Write_BufferIndex].State == JPEG_BUFFER_EMPTY)
  {
    int bufsize = CHUNK_SIZE_IN;

    Jpeg_IN_BufferTab[JPEG_IN_Write_BufferIndex].DataBuffer = Stream + StreamPos;
    StreamPos += bufsize;
    StreamSize -= bufsize;
    if (StreamSize < 0) {
        StreamPos += StreamSize;
        StreamSize = bufsize + StreamSize;
        if (StreamSize == 0) {
            return;
        }
        bufsize = StreamSize;
    }
    Jpeg_IN_BufferTab[JPEG_IN_Write_BufferIndex].DataBufferSize = bufsize;

    Jpeg_IN_BufferTab[JPEG_IN_Write_BufferIndex].State = JPEG_BUFFER_FULL;

    if((Input_Is_Paused == 1) && (JPEG_IN_Write_BufferIndex == JPEG_IN_Read_BufferIndex))
    {
      Input_Is_Paused = 0;
      HAL_JPEG_ConfigInputBuffer(hjpeg,Jpeg_IN_BufferTab[JPEG_IN_Read_BufferIndex].DataBuffer, Jpeg_IN_BufferTab[JPEG_IN_Read_BufferIndex].DataBufferSize);    
  
      HAL_JPEG_Resume(hjpeg, JPEG_PAUSE_RESUME_INPUT); 
    }
    
    JPEG_IN_Write_BufferIndex++;
    if(JPEG_IN_Write_BufferIndex >= NB_INPUT_DATA_BUFFERS)
    {
      JPEG_IN_Write_BufferIndex = 0;
    }            
  }
}

/**
  * @brief  JPEG Info ready callback
  * @param hjpeg: JPEG handle pointer
  * @param pInfo: JPEG Info Struct pointer
  * @retval None
  */
void HAL_JPEG_InfoReadyCallback(JPEG_HandleTypeDef *hjpeg, JPEG_ConfTypeDef *pInfo)
{
  if(JPEG_GetDecodeColorConvertFunc(pInfo, &pConvert_Function, &MCU_TotalNb) != HAL_OK)
  {
    assert(0);
  }
}

/**
  * @brief  JPEG Get Data callback
  * @param hjpeg: JPEG handle pointer
  * @param NbDecodedData: Number of decoded (consummed) bytes from input buffer
  * @retval None
  */
void HAL_JPEG_GetDataCallback(JPEG_HandleTypeDef *hjpeg, uint32_t NbDecodedData)
{
  if(NbDecodedData == Jpeg_IN_BufferTab[JPEG_IN_Read_BufferIndex].DataBufferSize)
  {  
    Jpeg_IN_BufferTab[JPEG_IN_Read_BufferIndex].State = JPEG_BUFFER_EMPTY;
    Jpeg_IN_BufferTab[JPEG_IN_Read_BufferIndex].DataBufferSize = 0;
  
    JPEG_IN_Read_BufferIndex++;
    if(JPEG_IN_Read_BufferIndex >= NB_INPUT_DATA_BUFFERS)
    {
      JPEG_IN_Read_BufferIndex = 0;
    }
  
    if(Jpeg_IN_BufferTab[JPEG_IN_Read_BufferIndex].State == JPEG_BUFFER_EMPTY)
    {
      HAL_JPEG_Pause(hjpeg, JPEG_PAUSE_RESUME_INPUT);
      Input_Is_Paused = 1;
    }
    else
    {    
      HAL_JPEG_ConfigInputBuffer(hjpeg,Jpeg_IN_BufferTab[JPEG_IN_Read_BufferIndex].DataBuffer, Jpeg_IN_BufferTab[JPEG_IN_Read_BufferIndex].DataBufferSize);    
    }
  }
  else
  {
    HAL_JPEG_ConfigInputBuffer(hjpeg,Jpeg_IN_BufferTab[JPEG_IN_Read_BufferIndex].DataBuffer + NbDecodedData, Jpeg_IN_BufferTab[JPEG_IN_Read_BufferIndex].DataBufferSize - NbDecodedData);      
  }
}

/**
  * @brief  JPEG Data Ready callback
  * @param hjpeg: JPEG handle pointer
  * @param pDataOut: pointer to the output data buffer
  * @param OutDataLength: length of output buffer in bytes
  * @retval None
  */
void HAL_JPEG_DataReadyCallback (JPEG_HandleTypeDef *hjpeg, uint8_t *pDataOut, uint32_t OutDataLength)
{
  Jpeg_OUT_BufferTab[JPEG_OUT_Write_BufferIndex].State = JPEG_BUFFER_FULL;
  Jpeg_OUT_BufferTab[JPEG_OUT_Write_BufferIndex].DataBufferSize = OutDataLength;
    
  JPEG_OUT_Write_BufferIndex++;
  if(JPEG_OUT_Write_BufferIndex >= NB_OUTPUT_DATA_BUFFERS)
  {
    JPEG_OUT_Write_BufferIndex = 0;
  }

  if(Jpeg_OUT_BufferTab[JPEG_OUT_Write_BufferIndex].State != JPEG_BUFFER_EMPTY)
  {
    HAL_JPEG_Pause(hjpeg, JPEG_PAUSE_RESUME_OUTPUT);
    Output_Is_Paused = 1;
  }
  HAL_JPEG_ConfigOutputBuffer(hjpeg, Jpeg_OUT_BufferTab[JPEG_OUT_Write_BufferIndex].DataBuffer, CHUNK_SIZE_OUT); 
}

/**
  * @brief  JPEG Error callback
  * @param hjpeg: JPEG handle pointer
  * @retval None
  */
void HAL_JPEG_ErrorCallback(JPEG_HandleTypeDef *hjpeg)
{
  assert(0);
}

/**
  * @brief  JPEG Decode complete callback
  * @param hjpeg: JPEG handle pointer
  * @retval None
  */
void HAL_JPEG_DecodeCpltCallback(JPEG_HandleTypeDef *hjpeg)
{    
  Jpeg_HWDecodingEnd = 1; 
}
/**
  * @}
  */

/**
  * @}
  */

void HAL_JPEG_MspInit(JPEG_HandleTypeDef *hjpeg)
{
  static DMA_HandleTypeDef   hdmaIn;
  static DMA_HandleTypeDef   hdmaOut;
  
  /* Enable JPEG clock */
  __HAL_RCC_JPEG_CLK_ENABLE();
  
    /* Enable DMA clock */
  __HAL_RCC_DMA2_CLK_ENABLE();

  HAL_NVIC_SetPriority(JPEG_IRQn, 0x06, 0x0F);
  HAL_NVIC_EnableIRQ(JPEG_IRQn);
  
  /* Input DMA */    
  /* Set the parameters to be configured */
  hdmaIn.Init.Channel = DMA_CHANNEL_9;
  hdmaIn.Init.Direction = DMA_MEMORY_TO_PERIPH;
  hdmaIn.Init.PeriphInc = DMA_PINC_DISABLE;
  hdmaIn.Init.MemInc = DMA_MINC_ENABLE;
  hdmaIn.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  hdmaIn.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
  hdmaIn.Init.Mode = DMA_NORMAL;
  hdmaIn.Init.Priority = DMA_PRIORITY_HIGH;
  hdmaIn.Init.FIFOMode = DMA_FIFOMODE_ENABLE;         
  hdmaIn.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
  hdmaIn.Init.MemBurst = DMA_MBURST_INC4;
  hdmaIn.Init.PeriphBurst = DMA_PBURST_INC4;      
  
  hdmaIn.Instance = DMA2_Stream3;
  
  /* Associate the DMA handle */
  __HAL_LINKDMA(hjpeg, hdmain, hdmaIn);
  
  HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 0x07, 0x0F);
  HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);    
  
  /* DeInitialize the DMA Stream */
  HAL_DMA_DeInit(&hdmaIn);  
  /* Initialize the DMA stream */
  HAL_DMA_Init(&hdmaIn);
  
  
  /* Output DMA */
  /* Set the parameters to be configured */ 
  hdmaOut.Init.Channel = DMA_CHANNEL_9;
  hdmaOut.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdmaOut.Init.PeriphInc = DMA_PINC_DISABLE;
  hdmaOut.Init.MemInc = DMA_MINC_ENABLE;
  hdmaOut.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
  hdmaOut.Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
  hdmaOut.Init.Mode = DMA_NORMAL;
  hdmaOut.Init.Priority = DMA_PRIORITY_VERY_HIGH;
  hdmaOut.Init.FIFOMode = DMA_FIFOMODE_ENABLE;         
  hdmaOut.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
  hdmaOut.Init.MemBurst = DMA_MBURST_INC4;
  hdmaOut.Init.PeriphBurst = DMA_PBURST_INC4;

  
  hdmaOut.Instance = DMA2_Stream4;
  /* DeInitialize the DMA Stream */
  HAL_DMA_DeInit(&hdmaOut);  
  /* Initialize the DMA stream */
  HAL_DMA_Init(&hdmaOut);

  /* Associate the DMA handle */
  __HAL_LINKDMA(hjpeg, hdmaout, hdmaOut);
  
  HAL_NVIC_SetPriority(DMA2_Stream4_IRQn, 0x07, 0x0F);
  HAL_NVIC_EnableIRQ(DMA2_Stream4_IRQn);   
    
}

int HAL_JPEG_UserInit (void)
{
    JPEG_InitColorTables();

    /* Init the HAL JPEG driver */
    JPEG_Handle.Instance = JPEG;
    HAL_JPEG_Init(&JPEG_Handle);
}

int JPEG_Info (jpeg_info_t *info)
{
    JPEG_ConfTypeDef JPEG_InfoHandle;

    HAL_JPEG_GetInfo(&JPEG_Handle, &JPEG_InfoHandle);
    info->w = JPEG_InfoHandle.ImageWidth;
    info->h = JPEG_InfoHandle.ImageHeight;
    info->colormode = JPEG_InfoHandle.ColorSpace;
    info->flags = 0;
}

int JPEG_Abort (JPEG_HandleTypeDef *hjpeg)
{
    HAL_JPEG_Abort(hjpeg);
    return 0;
}

void JPEG_IRQHandler(void)
{
  HAL_JPEG_IRQHandler(&JPEG_Handle);
}

void DMA2_Stream3_IRQHandler(void)
{
  HAL_DMA_IRQHandler(JPEG_Handle.hdmain);
}

void DMA2_Stream4_IRQHandler(void)
{
  HAL_DMA_IRQHandler(JPEG_Handle.hdmaout);
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/


