#include "stdint.h"
#include "string.h"
#include "stdarg.h"
#include "debug.h"
#include "main.h"
#include "stm32f7xx_it.h"

#if DEBUG_SERIAL

#define UART_DMA_TX 1
#define MAX_UARTS 1

#ifndef arrlen
#define arrlen(a) sizeof(a) / sizeof(a[0])
#endif

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#ifndef container_of
#define container_of(ptr, type, member)     \
    (type *)((uint8_t *)(ptr) - offsetof(type,member))
#endif

#ifndef USE_STM32F769I_DISCO
#error "Not supported"
#endif

extern void serial_led_on (void);
extern void serial_led_off (void);

typedef struct uart_desc_s uart_desc_t;

typedef void (*uart_msp_init_t) (uart_desc_t *uart_desc);

struct uart_desc_s {
    USART_TypeDef           *hw;
    UART_HandleTypeDef      handle;
    UART_InitTypeDef  const * cfg;
    uart_msp_init_t         msp_init;
#if UART_DMA_TX
    DMA_HandleTypeDef       hdma_tx;
    FlagStatus              uart_tx_ready;
    IRQn_Type               irq_txdma, irq_uart;
#endif
};

static int          uart_desc_cnt = 0;
static uart_desc_t *uart_desc_pool[MAX_UARTS];

static const UART_InitTypeDef uart_115200_8n1_tx =
{
    .BaudRate       = 115200,
    .WordLength     = UART_WORDLENGTH_8B,
    .StopBits       = UART_STOPBITS_1,
    .Parity         = UART_PARITY_NONE,
    .Mode           = UART_MODE_TX,
    .HwFlowCtl      = UART_HWCONTROL_NONE,
    .OverSampling   = 0,
    .OneBitSampling = 0,
};

#if UART_DMA_TX


#define SERIAL_IRQ_MAGIC 0x77ff
static int serial_irq_saved = 0;

void serial_irq_save (int *irq)
{
    int i;
    uart_desc_t *uart_desc;

    if (serial_irq_saved != SERIAL_IRQ_MAGIC) {

        for (i = 0; i < uart_desc_cnt; i++) {

            uart_desc = uart_desc_pool[i];

            HAL_NVIC_DisableIRQ(uart_desc->irq_txdma);
            HAL_NVIC_DisableIRQ(uart_desc->irq_uart);
        }
        *irq = SERIAL_IRQ_MAGIC;
    } else {
        *irq = 0;
    }
    serial_irq_saved = SERIAL_IRQ_MAGIC;
}

void serial_irq_restore (int irq)
{
    int i;
    uart_desc_t *uart_desc;

    if (irq == SERIAL_IRQ_MAGIC) {


        for (i = 0; i < uart_desc_cnt; i++) {

            uart_desc = uart_desc_pool[i];

            HAL_NVIC_EnableIRQ(uart_desc->irq_txdma);
            HAL_NVIC_EnableIRQ(uart_desc->irq_uart);
        }
    }
}

#else /*UART_DMA_TX*/

void serial_irq_save (int *irq)
{
    UNUSED(irq);
}

void serial_irq_restore (int irq)
{
    UNUSED(irq);
}

#endif /*UART_DMA_TX*/

static void uart1_msp_init (uart_desc_t *uart_desc)
{
    UART_HandleTypeDef *huart = &uart_desc->handle;
    static DMA_HandleTypeDef *hdma_tx;
    GPIO_InitTypeDef  GPIO_InitStruct;

    __GPIOA_CLK_ENABLE();
    __USART1_CLK_ENABLE();

    GPIO_InitStruct.Pin       = GPIO_PIN_9; /*TX*/
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;

    if (huart->Init.Mode & UART_MODE_TX) {
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }

    GPIO_InitStruct.Pin = GPIO_PIN_10;

    if (huart->Init.Mode & UART_MODE_RX) {
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
#if UART_DMA_TX
    /*DMA2_Stream7*/
    __HAL_RCC_DMA2_CLK_ENABLE();

    hdma_tx = &uart_desc->hdma_tx;

    hdma_tx->Instance                 = DMA2_Stream7;
    hdma_tx->Init.Channel             = DMA_CHANNEL_4;
    hdma_tx->Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_tx->Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_tx->Init.MemInc              = DMA_MINC_ENABLE;
    hdma_tx->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_tx->Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_tx->Init.Mode                = DMA_NORMAL;
    hdma_tx->Init.Priority            = DMA_PRIORITY_LOW;

    HAL_DMA_Init(hdma_tx);

    /* Associate the initialized DMA handle to the UART handle */
    __HAL_LINKDMA(huart, hdmatx, *hdma_tx);

    HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 0, 1);
    HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);

    HAL_NVIC_SetPriority(USART1_IRQn, 0, 1);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    uart_desc->irq_txdma = DMA2_Stream7_IRQn;
    uart_desc->irq_uart  = USART1_IRQn;
#else
    UNUSED(hdma_tx);
#endif
}


static void _serial_init (uart_desc_t *uart_desc)
{
    UART_HandleTypeDef *handle = &uart_desc->handle;

    handle->Instance        = uart_desc->hw;

    memcpy(&handle->Init, uart_desc->cfg, sizeof(handle->Init));

    if(HAL_UART_DeInit(handle) != HAL_OK)
    {
        fatal_error("");
    }  
    if(HAL_UART_Init(handle) != HAL_OK)
    {
        fatal_error("");
    }
}

static uart_desc_t uart1_desc;

void serial_init (void)
{
    if (uart_desc_cnt < MAX_UARTS) {
        uart_desc_pool[uart_desc_cnt++] = &uart1_desc;
    }

    uart1_desc.hw = USART1;
    uart1_desc.cfg = &uart_115200_8n1_tx;
    uart1_desc.msp_init = uart1_msp_init;
    uart1_desc.uart_tx_ready = SET;

    _serial_init(&uart1_desc);
}

static HAL_StatusTypeDef _serial_send (uart_desc_t *uart_desc, void *data, size_t cnt)
{
    HAL_StatusTypeDef status;
    serial_led_on();
#if UART_DMA_TX
    while (uart_desc->uart_tx_ready != SET)
    {
    }
    uart_desc->uart_tx_ready = RESET;

    status = HAL_UART_Transmit_DMA(&uart_desc->handle, (uint8_t *)data, cnt);
#else
    status = HAL_UART_Transmit(&uart_desc->handle, (uint8_t *)data, cnt, 1000);
#endif

    serial_led_off();
    return status;
}

void serial_putc (char c)
{
    HAL_StatusTypeDef status;
    uint8_t buf[1] = {c};

    status = _serial_send(&uart1_desc, buf, sizeof(buf));

    if (status != HAL_OK){
        fatal_error("");
    }
}

char serial_getc (void)
{
    return 0;
}

void serial_send_buf (void *data, size_t cnt)
{
    HAL_StatusTypeDef status;

    status = _serial_send(&uart1_desc, data, cnt);

    if (status != HAL_OK){
        fatal_error("");
    }
}

void dprint (char *fmt, ...)
{
    va_list         argptr;
    char            string[1024];
    int size;

    va_start (argptr, fmt);
    size = vsnprintf (string, sizeof(string), fmt, argptr);
    va_end (argptr);

    serial_send_buf(string, size);
}

void dvprint (char *fmt, va_list argptr)
{
    char            string[1024];
    int size;

    size = vsnprintf (string, sizeof(string), fmt, argptr);

    serial_send_buf(string, size);
}


#if UART_DMA_TX

static void dma_tx_handle_irq (const DMA_Stream_TypeDef *source)
{
    int i;
    DMA_HandleTypeDef *hdma;

    for (i = 0; i < uart_desc_cnt; i++) {
        hdma = &uart_desc_pool[i]->hdma_tx;
        if (hdma->Instance == source) {
            HAL_DMA_IRQHandler(hdma);
            break;
        }
    }
}

static void uart_handle_irq (USART_TypeDef *source)
{
    int i;
    UART_HandleTypeDef *huart;

    for (i = 0; i < uart_desc_cnt; i++) {
        huart = &uart_desc_pool[i]->handle;
        if (huart->Instance == source) {
            HAL_UART_IRQHandler(huart);
            break;
        }
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *UartHandle)
{
    uart_desc_t *uart_desc = container_of(UartHandle, uart_desc_t, handle);
    uart_desc->uart_tx_ready = SET;
}

void DMA2_Stream7_IRQHandler (void)
{
    dma_tx_handle_irq(DMA2_Stream7);
}

void USART1_IRQHandler (void)
{
    uart_handle_irq(USART1);
}

#endif /*UART_DMA_TX*/

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    uart_desc_t *uart_desc = container_of(huart, uart_desc_t, handle);

    if (uart_desc->msp_init) {
        uart_desc->msp_init(uart_desc);
    }
}

#endif /*DEBUG_SERIAL*/

