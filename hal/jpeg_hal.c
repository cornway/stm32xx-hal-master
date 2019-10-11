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
#include <heap.h>
#include <lcd_main.h>

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

} JPEG_Data_BufferTypeDef;

/* Private define ------------------------------------------------------------*/

#define CHUNK_SIZE_IN  ((uint32_t)(4096)) 
#define CHUNK_SIZE_OUT ((uint32_t)(768))

#define JPEG_BUFFER_EMPTY 0
#define JPEG_BUFFER_FULL  1

#define NB_OUTPUT_DATA_BUFFERS      2
#define NB_INPUT_DATA_BUFFERS       2

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

typedef struct {
    uint8_t *ptr;
    uint32_t pos;
    uint32_t size;
} byte_stream_t;

typedef struct {
    JPEG_HandleTypeDef    hal_jpeg;

    void *framebuf;
    byte_stream_t instream;

    JPEG_Data_BufferTypeDef intab[NB_INPUT_DATA_BUFFERS];
    JPEG_Data_BufferTypeDef outtab[NB_OUTPUT_DATA_BUFFERS];

    JPEG_YCbCrToRGB_Convert_Function convert_func;

    uint32_t mcuidx;
    uint32_t mcunum;
    uint8_t inwrite_idx;
    uint8_t inread_idx;
    uint8_t outwrite_idx;
    uint8_t outread_idx;
    uint32_t input_paused: 1,
             output_paused: 1,
             hw_end: 1,
             reserved: 29;
} jpeg_hal_ctxt_t;

static jpeg_hal_ctxt_t jpeg_hal_ctxt = {0};

/* Private function prototypes -----------------------------------------------*/
int JPEG_Decode_DMA(JPEG_HandleTypeDef *hjpeg, void *data, uint32_t size, uint32_t DestAddress);
uint32_t JPEG_OutputHandler(JPEG_HandleTypeDef *hjpeg);
void JPEG_InputHandler(JPEG_HandleTypeDef *hjpeg);

static void *bytestream_move (byte_stream_t *stream, uint32_t *len)
{
    uint32_t oldpos = stream->pos;
    uint32_t left = stream->size - stream->pos;

    if (*len > left) {
        *len = left;
    }
    stream->pos += *len;
    return stream->ptr + oldpos;
}

int JPEG_Decode_HAL (jpeg_info_t *info, void *tempbuf, void *data, uint32_t size)
{
    int err, done;

    err = JPEG_Decode_DMA(&jpeg_hal_ctxt.hal_jpeg, data, size, (uint32_t)tempbuf);

    if (err < 0) return -1;

    do
    {
      JPEG_InputHandler(&jpeg_hal_ctxt.hal_jpeg);
      done = JPEG_OutputHandler(&jpeg_hal_ctxt.hal_jpeg);
    } while(done == 0);

    heap_free(jpeg_hal_ctxt.outtab[0].DataBuffer);

    JPEG_Info_HAL(info);
    JPEG_Abort(&jpeg_hal_ctxt.hal_jpeg);
    return 0;
}


/**
  * @brief  Decode_DMA
  * @param hjpeg: JPEG handle pointer
  * @param  FileName    : jpg file path for decode.
  * @param  DestAddress : ARGB destination Frame Buffer Address.
  * @retval None
  */
int JPEG_Decode_DMA(JPEG_HandleTypeDef *hjpeg, void *data, uint32_t size, uint32_t DestAddress)
{
    uint32_t i, len;
    uint8_t *ptr;
    HAL_StatusTypeDef status;

    jpeg_hal_ctxt.convert_func = NULL;
    jpeg_hal_ctxt.framebuf = NULL;
    jpeg_hal_ctxt.hw_end = 0;
    jpeg_hal_ctxt.inread_idx = 0;
    jpeg_hal_ctxt.inwrite_idx = 0;
    jpeg_hal_ctxt.mcuidx = 0;
    jpeg_hal_ctxt.mcunum = 0;
    jpeg_hal_ctxt.output_paused = 0;
    jpeg_hal_ctxt.outread_idx = 0;
    jpeg_hal_ctxt.outwrite_idx = 0;

    jpeg_hal_ctxt.instream.pos = 0;
    jpeg_hal_ctxt.instream.ptr = data;
    jpeg_hal_ctxt.instream.size = size;
    jpeg_hal_ctxt.framebuf = (void *)DestAddress;

    len = CHUNK_SIZE_IN;
    for(i = 0; i < NB_INPUT_DATA_BUFFERS && len; i++)
    {
        jpeg_hal_ctxt.intab[i].DataBuffer = bytestream_move(&jpeg_hal_ctxt.instream, &len);
        jpeg_hal_ctxt.intab[i].DataBufferSize = len;
        jpeg_hal_ctxt.intab[i].State = (len == CHUNK_SIZE_IN) ? JPEG_BUFFER_FULL : JPEG_BUFFER_EMPTY;
    }

    ptr = heap_alloc_shared(NB_OUTPUT_DATA_BUFFERS * CHUNK_SIZE_OUT);
    d_memset(ptr, 0, NB_OUTPUT_DATA_BUFFERS * CHUNK_SIZE_OUT);

    for (i = 0; i < NB_OUTPUT_DATA_BUFFERS; i++)
    {
        jpeg_hal_ctxt.outtab[i].DataBuffer = ptr;
        jpeg_hal_ctxt.outtab[i].DataBufferSize = 0;
        jpeg_hal_ctxt.outtab[i].State = JPEG_BUFFER_EMPTY;
        ptr += CHUNK_SIZE_OUT;
    }

    status = HAL_JPEG_Decode_DMA(hjpeg , jpeg_hal_ctxt.intab[0].DataBuffer ,jpeg_hal_ctxt.intab[0].DataBufferSize,
                                    jpeg_hal_ctxt.outtab[0].DataBuffer, CHUNK_SIZE_OUT);

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
  
  if(jpeg_hal_ctxt.outtab[jpeg_hal_ctxt.outread_idx].State == JPEG_BUFFER_FULL)
  {  
    jpeg_hal_ctxt.mcuidx += jpeg_hal_ctxt.convert_func(jpeg_hal_ctxt.outtab[jpeg_hal_ctxt.outread_idx].DataBuffer, (uint8_t *)jpeg_hal_ctxt.framebuf,
                            jpeg_hal_ctxt.mcuidx, jpeg_hal_ctxt.outtab[jpeg_hal_ctxt.outread_idx].DataBufferSize, &ConvertedDataCount);
    
    jpeg_hal_ctxt.outtab[jpeg_hal_ctxt.outread_idx].State = JPEG_BUFFER_EMPTY;
    jpeg_hal_ctxt.outtab[jpeg_hal_ctxt.outread_idx].DataBufferSize = 0;
    
    jpeg_hal_ctxt.outread_idx++;
    if(jpeg_hal_ctxt.outread_idx >= NB_OUTPUT_DATA_BUFFERS)
    {
      jpeg_hal_ctxt.outread_idx = 0;
    }
    
    if(jpeg_hal_ctxt.mcuidx == jpeg_hal_ctxt.mcunum)
    {
      return 1;
    }
  }
  else if((jpeg_hal_ctxt.output_paused == 1) && \
          (jpeg_hal_ctxt.outwrite_idx == jpeg_hal_ctxt.outread_idx) && \
          (jpeg_hal_ctxt.outtab[jpeg_hal_ctxt.outread_idx].State == JPEG_BUFFER_EMPTY))
  {
    jpeg_hal_ctxt.output_paused = 0;
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
  if(jpeg_hal_ctxt.intab[jpeg_hal_ctxt.inwrite_idx].State == JPEG_BUFFER_EMPTY)
  {
    uint32_t bufsize = CHUNK_SIZE_IN;

    jpeg_hal_ctxt.intab[jpeg_hal_ctxt.inwrite_idx].DataBuffer =
                                                bytestream_move(&jpeg_hal_ctxt.instream, &bufsize);
    if (bufsize == 0) {
        return;
    }
    jpeg_hal_ctxt.intab[jpeg_hal_ctxt.inwrite_idx].DataBufferSize = bufsize;

    jpeg_hal_ctxt.intab[jpeg_hal_ctxt.inwrite_idx].State = JPEG_BUFFER_FULL;

    if((jpeg_hal_ctxt.input_paused == 1) && (jpeg_hal_ctxt.inwrite_idx == jpeg_hal_ctxt.inread_idx))
    {
      jpeg_hal_ctxt.input_paused = 0;
      HAL_JPEG_ConfigInputBuffer(hjpeg,jpeg_hal_ctxt.intab[jpeg_hal_ctxt.inread_idx].DataBuffer,
                                jpeg_hal_ctxt.intab[jpeg_hal_ctxt.inread_idx].DataBufferSize);
  
      HAL_JPEG_Resume(hjpeg, JPEG_PAUSE_RESUME_INPUT); 
    }
    
    jpeg_hal_ctxt.inwrite_idx++;
    if(jpeg_hal_ctxt.inwrite_idx >= NB_INPUT_DATA_BUFFERS)
    {
      jpeg_hal_ctxt.inwrite_idx = 0;
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
  if(JPEG_GetDecodeColorConvertFunc(pInfo, &jpeg_hal_ctxt.convert_func, &jpeg_hal_ctxt.mcunum) != HAL_OK)
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
  if(NbDecodedData == jpeg_hal_ctxt.intab[jpeg_hal_ctxt.inread_idx].DataBufferSize)
  {  
    jpeg_hal_ctxt.intab[jpeg_hal_ctxt.inread_idx].State = JPEG_BUFFER_EMPTY;
    jpeg_hal_ctxt.intab[jpeg_hal_ctxt.inread_idx].DataBufferSize = 0;
  
    jpeg_hal_ctxt.inread_idx++;
    if(jpeg_hal_ctxt.inread_idx >= NB_INPUT_DATA_BUFFERS)
    {
      jpeg_hal_ctxt.inread_idx = 0;
    }
  
    if(jpeg_hal_ctxt.intab[jpeg_hal_ctxt.inread_idx].State == JPEG_BUFFER_EMPTY)
    {
      HAL_JPEG_Pause(hjpeg, JPEG_PAUSE_RESUME_INPUT);
      jpeg_hal_ctxt.input_paused = 1;
    }
    else
    {    
      HAL_JPEG_ConfigInputBuffer(hjpeg,jpeg_hal_ctxt.intab[jpeg_hal_ctxt.inread_idx].DataBuffer,
                                jpeg_hal_ctxt.intab[jpeg_hal_ctxt.inread_idx].DataBufferSize);
    }
  }
  else
  {
    HAL_JPEG_ConfigInputBuffer(hjpeg,jpeg_hal_ctxt.intab[jpeg_hal_ctxt.inread_idx].DataBuffer + NbDecodedData,
                            jpeg_hal_ctxt.intab[jpeg_hal_ctxt.inread_idx].DataBufferSize - NbDecodedData);
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
  jpeg_hal_ctxt.outtab[jpeg_hal_ctxt.outwrite_idx].State = JPEG_BUFFER_FULL;
  jpeg_hal_ctxt.outtab[jpeg_hal_ctxt.outwrite_idx].DataBufferSize = OutDataLength;
    
  jpeg_hal_ctxt.outwrite_idx++;
  if(jpeg_hal_ctxt.outwrite_idx >= NB_OUTPUT_DATA_BUFFERS)
  {
    jpeg_hal_ctxt.outwrite_idx = 0;
  }

  if(jpeg_hal_ctxt.outtab[jpeg_hal_ctxt.outwrite_idx].State != JPEG_BUFFER_EMPTY)
  {
    HAL_JPEG_Pause(hjpeg, JPEG_PAUSE_RESUME_OUTPUT);
    jpeg_hal_ctxt.output_paused = 1;
  }
  HAL_JPEG_ConfigOutputBuffer(hjpeg, jpeg_hal_ctxt.outtab[jpeg_hal_ctxt.outwrite_idx].DataBuffer, CHUNK_SIZE_OUT); 
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
  jpeg_hal_ctxt.hw_end = 1; 
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

int JPEG_UserInit_HAL (void)
{
    JPEG_InitColorTables();

    /* Init the HAL JPEG driver */
    jpeg_hal_ctxt.hal_jpeg.Instance = JPEG;
    return HAL_JPEG_Init(&jpeg_hal_ctxt.hal_jpeg) == HAL_OK ? 0 : -1;
}

int JPEG_Info_HAL (jpeg_info_t *info)
{
    JPEG_ConfTypeDef JPEG_InfoHandle;

    HAL_JPEG_GetInfo(&jpeg_hal_ctxt.hal_jpeg, &JPEG_InfoHandle);
    info->w = JPEG_InfoHandle.ImageWidth;
    info->h = JPEG_InfoHandle.ImageHeight;
    info->colormode = JPEG_InfoHandle.ColorSpace;
    info->flags = 0;
    return 0;
}

int JPEG_Abort (JPEG_HandleTypeDef *hjpeg)
{
    HAL_JPEG_Abort(hjpeg);
    return 0;
}

void JPEG_IRQHandler(void)
{
  HAL_JPEG_IRQHandler(&jpeg_hal_ctxt.hal_jpeg);
}

void DMA2_Stream3_IRQHandler(void)
{
  HAL_DMA_IRQHandler(jpeg_hal_ctxt.hal_jpeg.hdmain);
}

void DMA2_Stream4_IRQHandler(void)
{
  HAL_DMA_IRQHandler(jpeg_hal_ctxt.hal_jpeg.hdmaout);
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/


