#include "stdint.h"
#include "string.h"
#include "stdarg.h"
#include "debug.h"
#include "main.h"
#include "stm32f7xx_it.h"

#if DEBUG_SERIAL

#define TX_STREAM_USE_DMA 1
#define MAX_UARTS 1
#define TX_STREAM_BUFERIZED 1
#define TX_FLUSH_TIMEOUT 200 /*MS*/

static void serial_fatal (void)
{
    for (;;) {}
}


#if !TX_STREAM_USE_DMA
#undef TX_STREAM_BUFERIZED
#define TX_STREAM_BUFERIZED 0
#endif /*!TX_STREAM_USE_DMA*/

#ifndef USE_STM32F769I_DISCO
#error "Not supported"
#endif

extern void serial_led_on (void);
extern void serial_led_off (void);

typedef enum {
    SERIAL_DEBUG,
    SERIAL_REGULAR,
} serial_type_t;

typedef struct uart_desc_s uart_desc_t;

typedef void (*uart_msp_init_t) (uart_desc_t *uart_desc);

struct uart_desc_s {
    USART_TypeDef           *hw;
    UART_HandleTypeDef      handle;
    UART_InitTypeDef  const * cfg;
    uart_msp_init_t         msp_init;
#if TX_STREAM_USE_DMA
    DMA_HandleTypeDef       hdma_tx;
    FlagStatus              uart_tx_ready;
    IRQn_Type               irq_txdma, irq_uart;
#endif
    serial_type_t           type;
#if TX_STREAM_BUFERIZED
    int                     active_stream;
#endif
    FlagStatus              initialized;
};

#if TX_STREAM_BUFERIZED

#define STREAM_BUFSIZE 512

typedef struct {
    char data[STREAM_BUFSIZE];
    int  bufposition;
    uint32_t timestamp;
} streambuf_t;

static streambuf_t streambuf[2];

static void serial_tim_irq_save (void);
static void serial_tim_irq_restore (void);
static void serial_timer_init (void);

#else /*TX_STREAM_BUFERIZED*/

#define serial_tim_irq_save()
#define serial_tim_irq_restore()
#define serial_timer_init()

#endif /*TX_STREAM_BUFERIZED*/


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

static uart_desc_t *debug_port (void)
{
    int i;

    for (i = 0; i < uart_desc_cnt; i++) {

        if (uart_desc_pool[i]->initialized &&
            (uart_desc_pool[i]->type == SERIAL_DEBUG)) {

            return uart_desc_pool[i];
        }
    }
    serial_fatal();
    return NULL;
}

#if TX_STREAM_USE_DMA

static inline void dma_tx_sync (uart_desc_t *uart_desc)
{
    while (uart_desc->uart_tx_ready != SET)
    {
    }
}

#define SERIAL_IRQ_MAGIC 0x77ff
volatile static int serial_irq_saved = 0;

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
        serial_tim_irq_save();
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
        serial_tim_irq_restore();
        serial_irq_saved = 0;
    } else {
        irq = 0;
    }
}

#else /*TX_STREAM_USE_DMA*/

void serial_irq_save (int *irq)
{
    UNUSED(irq);
}

void serial_irq_restore (int irq)
{
    UNUSED(irq);
}

#endif /*TX_STREAM_USE_DMA*/

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
#if TX_STREAM_USE_DMA
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
        serial_fatal();
    }
    if(HAL_UART_Init(handle) != HAL_OK)
    {
        serial_fatal();
    }
    uart_desc->initialized = SET;
}

static uart_desc_t uart1_desc;

static void _serial_debug_setup (uart_desc_t *uart_desc)
{
#if TX_STREAM_BUFERIZED
    memset(streambuf, 0, sizeof(streambuf));
    uart_desc->active_stream = 0;
#endif
    uart_desc->hw       = USART1;
    uart_desc->cfg      = &uart_115200_8n1_tx;
    uart_desc->msp_init = uart1_msp_init;
    uart_desc->type     = SERIAL_DEBUG;
    uart_desc->uart_tx_ready = SET;
}

void serial_init (void)
{
    if (uart_desc_cnt >= MAX_UARTS) {
        return;
    }
    uart_desc_pool[uart_desc_cnt++] = &uart1_desc;

    _serial_debug_setup(&uart1_desc);
    _serial_init(&uart1_desc);
    serial_timer_init();
}

static HAL_StatusTypeDef serial_submit_to_hw (uart_desc_t *uart_desc, void *data, size_t cnt)
{
    HAL_StatusTypeDef status;
    serial_led_on();

#if TX_STREAM_USE_DMA
    uart_desc->uart_tx_ready = RESET;
    status = HAL_UART_Transmit_DMA(&uart_desc->handle, (uint8_t *)data, cnt);

#else
    status = HAL_UART_Transmit(&uart_desc->handle, (uint8_t *)data, cnt, 1000);
#endif /*TX_STREAM_USE_DMA*/

    serial_led_off();
    return status;
}

#if TX_STREAM_BUFERIZED

static void dbgstream_send (uart_desc_t *uart_desc, streambuf_t *stbuf)
{
    HAL_StatusTypeDef status;

    status = serial_submit_to_hw(uart_desc, stbuf->data, stbuf->bufposition);

    if (status != HAL_OK) {
        serial_fatal();
    }
    stbuf->bufposition = 0;
    stbuf->timestamp = 0;
}

static void dbgstream_apend_data (streambuf_t *stbuf, void *data, size_t size)
{
    char *p = stbuf->data + stbuf->bufposition;
    memcpy(p, data, size);
    if (stbuf->bufposition == 0) {
        stbuf->timestamp = HAL_GetTick();
    }
    stbuf->bufposition += size;
}

static inline int dbgstream_bufferize (uart_desc_t *uart_desc, void *data, size_t size)
{
    streambuf_t *active_stream = &streambuf[uart_desc->active_stream];

    if (size >= STREAM_BUFSIZE) {
        return 0;
    }

    if (size >= (STREAM_BUFSIZE - active_stream->bufposition)) {
        dma_tx_sync(uart_desc);
        dbgstream_send(uart_desc, active_stream);
        uart_desc->active_stream = 1 - uart_desc->active_stream;
        active_stream = &streambuf[uart_desc->active_stream];
    }

    if (size >= (STREAM_BUFSIZE - active_stream->bufposition)) {
        serial_fatal();
    }
    dbgstream_apend_data(active_stream, data, size);
    return size;
}

#else /*TX_STREAM_BUFERIZED*/

static inline void dbgstream_bufferize (uart_desc_t *uart_desc, void *data, size_t size)
{
    UNUSED(uart_desc);
    UNUSED(data);
    UNUSED(size);
}

#endif /*TX_STREAM_BUFERIZED*/

static HAL_StatusTypeDef _serial_send (void *data, size_t cnt)
{
    HAL_StatusTypeDef status = HAL_OK;
    uart_desc_t *uart_desc = debug_port();

    serial_tim_irq_save();

    if (dbgstream_bufferize(uart_desc, data, cnt) <= 0) {
        dma_tx_sync(uart_desc);
        status = serial_submit_to_hw(uart_desc, data, cnt);
    }

    serial_tim_irq_restore();

    return status;
}

void serial_putc (char c)
{
    HAL_StatusTypeDef status;
    uint8_t buf[1] = {c};

    status = _serial_send(buf, sizeof(buf));

    if (status != HAL_OK){
        serial_fatal();
    }
}

char serial_getc (void)
{
    return 0;
}

void serial_send_buf (void *data, size_t cnt)
{
    HAL_StatusTypeDef status;

    status = _serial_send(data, cnt);

    if (status != HAL_OK){
        serial_fatal();
    }
}

void dprintf (char *fmt, ...)
{
    va_list         argptr;
    char            string[1024];
    int size;

    va_start (argptr, fmt);
    size = vsnprintf (string, sizeof(string), fmt, argptr);
    va_end (argptr);

    serial_send_buf(string, size);
}

void dvprintf (char *fmt, va_list argptr)
{
    char            string[1024];
    int size;

    size = vsnprintf (string, sizeof(string), fmt, argptr);

    serial_send_buf(string, size);
}


#if TX_STREAM_USE_DMA

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

static uart_desc_t *uart_find_desc (USART_TypeDef *source)
{
    int i;
    uart_desc_t *uart_desc;

    for (i = 0; i < uart_desc_cnt; i++) {
        uart_desc = uart_desc_pool[i];

        if (uart_desc->handle.Instance == source) {
            return uart_desc;
        }
    }
    return NULL;
}

static void uart_handle_irq (USART_TypeDef *source)
{
    uart_desc_t *uart_desc = uart_find_desc(source);

    if (uart_desc) {
        HAL_UART_IRQHandler(&uart_desc->handle);
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *UartHandle)
{
    uart_desc_t *uart_desc = uart_find_desc(UartHandle->Instance);
    if (!uart_desc) {
        return;
    }
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

#endif /*TX_STREAM_USE_DMA*/

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    uart_desc_t *uart_desc = uart_find_desc(huart->Instance);

    if (uart_desc && uart_desc->msp_init) {
        uart_desc->msp_init(uart_desc);
    }
}

#if TX_STREAM_BUFERIZED

extern uint32_t SystemCoreClock;

typedef struct {
    TIM_TypeDef *hw;
    TIM_HandleTypeDef handle;
    IRQn_Type irq;
} timer_desc_t;

static timer_desc_t serial_timer;

static void serial_flush_handler (void)
{
    uart_desc_t *uart_desc = debug_port();
    streambuf_t *stbuf = &streambuf[0], *stbufnext = &streambuf[1];
    uint32_t time;

    if (!uart_desc->uart_tx_ready) {
        return;
    }

    time = HAL_GetTick();

    while (1) {
        if (stbuf->timestamp &&
            ((time - stbuf->timestamp) > TX_FLUSH_TIMEOUT) &&
            (0 != stbuf->bufposition)) {

            dbgstream_send(uart_desc, stbuf);
        }
        if (stbuf == stbufnext) {
            break;
        }
        stbuf = stbufnext;
    }
}

static void serial_tim_irq_save (void)
{
    HAL_NVIC_DisableIRQ(serial_timer.irq);
}

static void serial_tim_irq_restore (void)
{
    HAL_NVIC_EnableIRQ(serial_timer.irq);
}

static void serial_timer_init (void)
{
    TIM_HandleTypeDef *handle = &serial_timer.handle;
    uint32_t uwPrescalerValue = (uint32_t)((SystemCoreClock / 2) / 10000) - 1;
    serial_timer.hw = TIM3;
    serial_timer.irq = TIM3_IRQn;

    handle->Instance = serial_timer.hw;

    handle->Init.Period            = 1000 - 1;
    handle->Init.Prescaler         = uwPrescalerValue;
    handle->Init.ClockDivision     = 0;
    handle->Init.CounterMode       = TIM_COUNTERMODE_UP;
    handle->Init.RepetitionCounter = 0;
    handle->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(handle) != HAL_OK)
    {
        serial_fatal();
    }

    if (HAL_TIM_Base_Start_IT(handle) != HAL_OK)
    {
        serial_fatal();
    }
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (&serial_timer.handle == htim) {
        __HAL_RCC_TIM3_CLK_ENABLE();


        HAL_NVIC_SetPriority(serial_timer.irq, 0, 1);

        HAL_NVIC_EnableIRQ(serial_timer.irq);
    }
}

void TIM3_IRQHandler (void)
{
    HAL_TIM_IRQHandler(&serial_timer.handle);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (&serial_timer.handle == htim) {

        serial_flush_handler();
    }
}


#endif /*TX_STREAM_BUFERIZED*/

#endif /*DEBUG_SERIAL*/

