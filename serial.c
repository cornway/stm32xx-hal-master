#include "stdint.h"
#include "string.h"
#include "stdarg.h"
#include "debug.h"
#include "main.h"
#include "dev_conf.h"
#include "nvic.h"
#include "heap.h"
#include "misc_utils.h"
#include "int/tim_int.h"
#include <dev_io.h>
#include "stm32f7xx_it.h"

#if defined(BSP_DRIVER)

#if DEBUG_SERIAL

#define TX_FLUSH_TIMEOUT 200 /*MS*/

static void serial_fatal (void)
{
    for (;;) {}
}


#if !DEBUG_SERIAL_USE_DMA

#if DEBUG_SERIAL_BUFERIZED
#warning "DEBUG_SERIAL_BUFERIZED==true while DEBUG_SERIAL_USE_DMA==false"
#undef DEBUG_SERIAL_BUFERIZED
#define DEBUG_SERIAL_BUFERIZED 0
#endif

#ifdef DEBUG_SERIAL_USE_RX
#error "DEBUG_SERIAL_USE_RX only with DEBUG_SERIAL_USE_DMA==1"
#endif

#else

#ifndef DEBUG_SERIAL_USE_RX
#warning "DEBUG_SERIAL_USE_RX undefined, using TRUE"
#define DEBUG_SERIAL_USE_RX 1
#endif /*!DEBUG_SERIAL_USE_DMA*/

#endif /*!DEBUG_SERIAL_USE_DMA*/

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
typedef void (*uart_dma_init_t) (uart_desc_t *uart_desc);

struct uart_desc_s {
    USART_TypeDef           *hw;
    UART_HandleTypeDef      handle;
    UART_InitTypeDef  const * cfg;
    uart_msp_init_t         msp_init;
    uart_msp_init_t         msp_deinit;
#if DEBUG_SERIAL_USE_DMA
    uart_dma_init_t         dma_init;
    uart_dma_init_t         dma_deinit;
    DMA_HandleTypeDef       hdma_tx;
    FlagStatus              uart_tx_ready;
    IRQn_Type               irq_txdma, irq_uart;
#if DEBUG_SERIAL_USE_RX
    DMA_HandleTypeDef       hdma_rx;
    IRQn_Type               irq_rxdma;
    void                    (*rx_handler) (DMA_HandleTypeDef *);
#endif
#endif
    serial_type_t           type;
#if DEBUG_SERIAL_BUFERIZED
    int                     active_stream;
#endif
    FlagStatus              initialized;
};

#if DEBUG_SERIAL_BUFERIZED

#define STREAM_BUFSIZE 512
#define STREAM_BUFCNT 2
#define STREAM_BUFCNT_MS (STREAM_BUFCNT - 1)

typedef struct {
    char data[STREAM_BUFSIZE + 1];
    int  bufposition;
    uint32_t timestamp;
} streambuf_t;

static streambuf_t streambuf[STREAM_BUFCNT];

#endif /*DEBUG_SERIAL_BUFERIZED*/

#if DEBUG_SERIAL_USE_RX

/*As far as i want to keep 'console' interactive,
  each received symbol must be immediately accepted, e.g. -
  any string will be processed char by char with cr(and\or)lf at the end,
  i see only one way to do this in more sophisticated way - flush rx buffer within timer.
  TODO : extend rx buffer size to gain more perf.
*/
#define DMA_RX_SIZE (1 << 1)
#define DMA_RX_FIFO_SIZE (1 << 8)

typedef struct {
    uint16_t fifoidx;
    uint16_t fifordidx;
    uint16_t eof;
    char dmabuf[DMA_RX_SIZE];
    char fifo[DMA_RX_FIFO_SIZE];
} rxstream_t;

static timer_desc_t serial_timer;

static rxstream_t rxstream;

#endif /*DEBUG_SERIAL_USE_RX*/

int32_t g_serial_rx_eof = '\n';

static void serial_flush_handler (int force);

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

static const UART_InitTypeDef uart_115200_8n1_txrx =
{
    .BaudRate       = 115200,
    .WordLength     = UART_WORDLENGTH_8B,
    .StopBits       = UART_STOPBITS_1,
    .Parity         = UART_PARITY_NONE,
    .Mode           = UART_MODE_TX_RX,
    .HwFlowCtl      = UART_HWCONTROL_NONE,
    .OverSampling   = 0,
    .OneBitSampling = 0,
};

#if DEBUG_SERIAL_USE_RX
const UART_InitTypeDef *uart_def_ptr = &uart_115200_8n1_txrx;
#else
const UART_InitTypeDef *uart_def_ptr = &uart_115200_8n1_tx;
#endif

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

#if DEBUG_SERIAL_USE_DMA

static irqmask_t dma_rx_irq_mask = 0;

static inline void dma_tx_sync (uart_desc_t *uart_desc)
{
    while (uart_desc->uart_tx_ready != SET) { }
    uart_desc->uart_tx_ready = RESET;
}

static inline void dma_tx_waitflush (uart_desc_t *uart_desc)
{
    while (uart_desc->uart_tx_ready != SET) { }
}

#endif /*DEBUG_SERIAL_USE_DMA*/

static void dma_rx_xfer_hanlder (struct __DMA_HandleTypeDef * hdma);

#if DEBUG_SERIAL_USE_DMA
static void uart1_dma_init (uart_desc_t *uart_desc)
{
    static DMA_HandleTypeDef *hdma_tx, *hdma_rx;
    UART_HandleTypeDef *huart = &uart_desc->handle;
    irqmask_t irqmask;

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

    HAL_DMA_DeInit(hdma_tx);
    HAL_DMA_Init(hdma_tx);

    /* Associate the initialized DMA handle to the UART handle */
    __HAL_LINKDMA(huart, hdmatx, *hdma_tx);

    HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 0, 1);
    HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);

    uart_desc->irq_txdma = DMA2_Stream7_IRQn;
#if DEBUG_SERIAL_USE_RX
    hdma_rx = &uart_desc->hdma_rx;

    hdma_rx->Instance                 = DMA2_Stream2;
    hdma_rx->Init.Channel             = DMA_CHANNEL_4;
    hdma_rx->Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_rx->Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_rx->Init.MemInc              = DMA_MINC_ENABLE;
    hdma_rx->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_rx->Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_rx->Init.Mode                = DMA_CIRCULAR;
    hdma_rx->Init.Priority            = DMA_PRIORITY_LOW;
    uart_desc->rx_handler            = dma_rx_xfer_hanlder;

    HAL_DMA_DeInit(hdma_rx);
    HAL_DMA_Init(hdma_rx);

    __HAL_LINKDMA(huart, hdmarx, *hdma_rx);

    irq_bmap(&irqmask);
    HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 0, 1);
    HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);
    irq_bmap(&dma_rx_irq_mask);
    dma_rx_irq_mask = dma_rx_irq_mask & (~irqmask);

    uart_desc->irq_rxdma = DMA2_Stream2_IRQn;

    if(HAL_UART_Receive_DMA(huart, (uint8_t *)rxstream.dmabuf, sizeof(rxstream.dmabuf)) != HAL_OK)
    {
        serial_fatal();
    }
#else
    UNUSED(hdma_rx);
#endif
    UNUSED(hdma_tx);
}

static void uart1_dma_deinit (uart_desc_t *uart_desc)
{
    HAL_NVIC_DisableIRQ(uart_desc->irq_txdma);
    HAL_DMA_DeInit(&uart_desc->hdma_tx);
#if DEBUG_SERIAL_USE_RX
    HAL_NVIC_DisableIRQ(uart_desc->irq_rxdma);
    HAL_DMA_DeInit(&uart_desc->hdma_rx);
#endif
}

#endif /*DEBUG_SERIAL_USE_DMA*/

static void uart1_msp_init (uart_desc_t *uart_desc)
{
    UART_HandleTypeDef *huart = &uart_desc->handle;
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
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 1);
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    uart_desc->irq_uart  = USART1_IRQn;
}

static void uart1_msp_deinit (uart_desc_t *uart_desc)
{
    HAL_NVIC_DisableIRQ(uart_desc->irq_uart);
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
    uart_desc->dma_init(uart_desc);
    uart_desc->initialized = SET;
}

static void _serial_deinit (uart_desc_t *uart_desc)
{
    UART_HandleTypeDef *handle = &uart_desc->handle;

    if(HAL_UART_DeInit(handle) != HAL_OK)
    {
        serial_fatal();
    }
    uart_desc->dma_deinit(uart_desc);
}


static uart_desc_t uart1_desc;

static void _serial_debug_setup (uart_desc_t *uart_desc)
{
#if DEBUG_SERIAL_BUFERIZED
    memset(streambuf, 0, sizeof(streambuf));
    uart_desc->active_stream = 0;
#endif
    uart_desc->hw       = USART1;
    uart_desc->cfg      = uart_def_ptr;
    uart_desc->dma_init = uart1_dma_init;
    uart_desc->dma_deinit = uart1_dma_deinit;
    uart_desc->msp_init = uart1_msp_init;
    uart_desc->msp_deinit = uart1_msp_deinit;
    uart_desc->type     = SERIAL_DEBUG;
    uart_desc->uart_tx_ready = SET;
}

static void serial_timer_msp_init (timer_desc_t *desc)
{
    __HAL_RCC_TIM3_CLK_ENABLE();
    HAL_NVIC_SetPriority(desc->irq, 0, 1);
    HAL_NVIC_EnableIRQ(desc->irq);
}

static void serial_timer_msp_deinit (timer_desc_t *desc)
{
    __HAL_RCC_TIM3_CLK_DISABLE();
    HAL_NVIC_DisableIRQ(desc->irq);
}

static void serial_timer_handler (timer_desc_t *desc)
{
    serial_flush_handler(0);
}

void TIM3_IRQHandler (void)
{
    HAL_TIM_IRQHandler(&serial_timer.handle);
}

int serial_init (void)
{
    if (uart_desc_cnt >= MAX_UARTS) {
        return -1;
    }
    uart_desc_pool[uart_desc_cnt++] = &uart1_desc;

    _serial_debug_setup(&uart1_desc);
    _serial_init(&uart1_desc);

    serial_timer.flags = TIM_RUNIT;
    serial_timer.period = 1000;
    serial_timer.presc = 10000;
    serial_timer.handler = serial_timer_handler;
    serial_timer.init = serial_timer_msp_init;
    serial_timer.deinit = serial_timer_msp_deinit;
    serial_timer.hw = TIM3;
    serial_timer.irq = TIM3_IRQn;
    return hal_tim_init(&serial_timer);
}

void serial_deinit (void)
{
    int i = 0;
    dprintf("%s() :\n", __func__);
    serial_flush();
    hal_tim_deinit(&serial_timer);
    for (i = 0; i < uart_desc_cnt; i++) {
        _serial_deinit(uart_desc_pool[i]);
    }
    uart_desc_cnt = 0;
}

static HAL_StatusTypeDef serial_submit_to_hw (uart_desc_t *uart_desc, const void *data, size_t cnt)
{
    irqmask_t irq = 0;
    HAL_StatusTypeDef status;
    serial_led_on();

#if DEBUG_SERIAL_USE_DMA
    dma_tx_sync(uart_desc);
    irq_save(&irq);
    status = HAL_UART_Transmit_DMA(&uart_desc->handle, (uint8_t *)data, cnt);
    irq_restore(irq);
#else
    status = HAL_UART_Transmit(&uart_desc->handle, (uint8_t *)data, cnt, 1000);
#endif /*DEBUG_SERIAL_USE_DMA*/

    serial_led_off();
    return status;
}

#if SERIAL_TSF

#define MSEC 1000

static int prev_putc = -1;

static void inline __proc_tsf (const char *str, int size)
{
    if (!str || !size) {
        return;
    }
    str = str + size - 1;
    while (*str) {
        prev_putc = *str;
        if (*str == '\n') {
            break;
        }
        str--;
    }
}

static inline int __insert_tsf (const char *fmt, char *buf, int max)
{
    uint32_t msec, sec;

    if (prev_putc < 0 ||
        prev_putc != '\n') {
        return 0;
    }
    msec = HAL_GetTick();
    sec = msec / MSEC;
    return snprintf(buf, max, "[%4d.%03d] ", sec, msec % MSEC);
}

#endif /*SERIAL_TSF*/

#if DEBUG_SERIAL_BUFERIZED

static void _dbgstream_submit (uart_desc_t *uart_desc, const void *data, size_t cnt)
{
    HAL_StatusTypeDef status;

    status = serial_submit_to_hw(uart_desc, data, cnt);

    if (status != HAL_OK) {
        serial_fatal();
    }
}

static void __dbgstream_send (uart_desc_t *uart_desc, streambuf_t *stbuf)
{
    _dbgstream_submit(uart_desc, stbuf->data, stbuf->bufposition);
    stbuf->bufposition = 0;
    stbuf->timestamp = 0;
}

static void dbgstream_apend_data (streambuf_t *stbuf, const void *data, size_t size)
{
    char *p = stbuf->data + stbuf->bufposition;
    memcpy(p, data, size);
    if (stbuf->bufposition == 0) {
        stbuf->timestamp = HAL_GetTick();
    }
    stbuf->bufposition += size;
}

static inline int
dbgstream_submit (uart_desc_t *uart_desc, const void *data, size_t size, d_bool flush)

{
    streambuf_t *active_stream = &streambuf[uart_desc->active_stream & STREAM_BUFCNT_MS];

    if (size > STREAM_BUFSIZE) {
        size = STREAM_BUFSIZE;
    }

#if SERIAL_TSF
    __proc_tsf((const char *)data, size);
#endif

    if (flush || size >= (STREAM_BUFSIZE - active_stream->bufposition)) {
        __dbgstream_send(uart_desc, active_stream);
        active_stream = &streambuf[(++uart_desc->active_stream) & STREAM_BUFCNT_MS];
    }
    if (size >= (STREAM_BUFSIZE - active_stream->bufposition)) {
        serial_fatal();
    }
    if (size) {
        dbgstream_apend_data(active_stream, data, size);
    }
    return size;
}

#else /*DEBUG_SERIAL_BUFERIZED*/

static inline int
dbgstream_submit (uart_desc_t *uart_desc, const void *data, size_t size, d_bool flush)
{
    UNUSED(uart_desc);
    UNUSED(data);
    UNUSED(size);
}

#endif /*DEBUG_SERIAL_BUFERIZED*/

int bsp_serial_send (const void *data, size_t cnt)
{
    irqmask_t irq_flags = serial_timer.irqmask;
    uart_desc_t *uart_desc = debug_port();
    int ret = 0;

    if (inout_early_clbk) {
        inout_early_clbk(data, cnt, '>');
    }
    bsp_inout_forward(data, cnt, '>');

    irq_save(&irq_flags);

    ret = dbgstream_submit(uart_desc, data, cnt, d_false);
    if (ret <= 0) {
        ret = dbgstream_submit(uart_desc, data, cnt, d_true);
    }

    irq_restore(irq_flags);

    return ret;
}

void serial_putc (char c)
{
    dprintf("%c", c);
}

char serial_getc (void)
{
    /*TODO : Use Rx from USART */
    return 0;
}

void serial_flush (void)
{
    irqmask_t irq_flags = serial_timer.irqmask;

    irq_save(&irq_flags);
    serial_flush_handler(1);
    irq_restore(irq_flags);
}

int dvprintf (const char *fmt, va_list argptr)
{
    /*TODO : use local buf*/
    char            string[1024];
    int size = 0;
#if SERIAL_TSF
    size = __insert_tsf(fmt, string, sizeof(string));
#endif
    size += vsnprintf(string + size, sizeof(string) - size, fmt, argptr);
    bsp_serial_send(string, size);
    return size;
}

int dprintf (const char *fmt, ...)
{
    va_list         argptr;
    int size;

    va_start (argptr, fmt);
    size = dvprintf(fmt, argptr);
    va_end (argptr);
    return size;
}

#if DEBUG_SERIAL_USE_DMA

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

static uint8_t __check_rx_crlf (char c)
{
    if (!g_serial_rx_eof) {
        return 0;
    }
    if (c == '\r') {
        return 0x1;
    }
    if (c == '\n') {
        return 0x3;
    }
    return 0;
}

static void _dma_rx_xfer_cplt (uint8_t full)
{
    int dmacplt = sizeof(rxstream.dmabuf) / 2;
    int cnt = dmacplt;
    char *src;

    src = &rxstream.dmabuf[full * dmacplt];

    while (cnt) {
        rxstream.fifo[rxstream.fifoidx++ & (DMA_RX_FIFO_SIZE - 1)] = *src;
        rxstream.eof |= __check_rx_crlf(*src);
        src++; cnt--;
    }
    if (!g_serial_rx_eof) {
        rxstream.eof = 3; /*\r & \n met*/
    }
}

static void dma_fifo_flush (rxstream_t *rx, char *dest, int *pcnt)
{
    int16_t cnt = (int16_t)(rx->fifoidx - rx->fifordidx);

    if (cnt < 0) {
        cnt = -cnt;
    }
    *pcnt = cnt;
    while (cnt > 0) {
        *dest = rx->fifo[rx->fifordidx++ & (DMA_RX_FIFO_SIZE - 1)];
        cnt--;
        dest++;
    }
    *dest = 0;
    /*TODO : Move next line after crlf to the begining,
      to handle multiple lines.
    */
    rx->eof = 0;
}

static void uart_handle_irq (USART_TypeDef *source)
{
    uart_desc_t *uart_desc = uart_find_desc(source);

    if (uart_desc) {
        HAL_UART_IRQHandler(&uart_desc->handle);
    }
}

void HAL_UART_RxHalfCpltCallback (UART_HandleTypeDef* huart)
{
    _dma_rx_xfer_cplt(0);
}

void HAL_UART_RxCpltCallback (UART_HandleTypeDef* huart)
{
    _dma_rx_xfer_cplt(1);
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

#if DEBUG_SERIAL_USE_RX

void serial_tickle (void)
{
    irqmask_t irq = dma_rx_irq_mask;
    char buf[DMA_RX_FIFO_SIZE + 1];
    int cnt;

    if (rxstream.eof != 0x3) {
        return;
    }

    irq_save(&irq);
    dma_fifo_flush(&rxstream, buf, &cnt);
    irq_restore(irq);

    if (inout_early_clbk) {
        inout_early_clbk(buf, cnt, '<');
    }
    bsp_inout_forward(buf, cnt, '<');
}

static void dma_rx_xfer_hanlder (struct __DMA_HandleTypeDef * hdma)
{
    HAL_DMA_IRQHandler(hdma);
}

static void dma_rx_handle_irq (const DMA_Stream_TypeDef *source)
{
    uart_desc_t *dsc       = uart_desc_pool[0],
                *dsc_end   = uart_desc_pool[uart_desc_cnt];

    for (; dsc != dsc_end; dsc++) {
        if (dsc->hdma_rx.Instance == source) {
            dma_rx_xfer_hanlder(&dsc->hdma_rx);
            break;
        }
    }
}

void DMA2_Stream2_IRQHandler (void)
{
    dma_rx_handle_irq(DMA2_Stream2);
}

#endif

void USART1_IRQHandler (void)
{
    uart_handle_irq(USART1);
}

#endif /*DEBUG_SERIAL_USE_DMA*/

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    uart_desc_t *uart_desc = uart_find_desc(huart->Instance);

    if (uart_desc && uart_desc->msp_init) {
        uart_desc->msp_init(uart_desc);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    uart_desc_t *uart_desc = uart_find_desc(huart->Instance);

    if (uart_desc && uart_desc->msp_init) {
        uart_desc->msp_deinit(uart_desc);
    }
}


#if DEBUG_SERIAL_BUFERIZED

static void serial_flush_handler (int force)
{
    uart_desc_t *uart_desc = debug_port();
    streambuf_t *active_stream = &streambuf[uart_desc->active_stream & STREAM_BUFCNT_MS];
    uint32_t time;

    if (!uart_desc->uart_tx_ready) {
        return;
    }

    if (0 != active_stream->bufposition) {

        time = HAL_GetTick();

        if ((time - active_stream->timestamp) > TX_FLUSH_TIMEOUT ||
            force) {

            active_stream->timestamp = time;
            active_stream->data[active_stream->bufposition++] = '\n';
            dbgstream_submit(uart_desc, NULL, 0, d_true);
            if (force) {
                dma_tx_waitflush(uart_desc);
            }
        }
    }
}

#endif /*DEBUG_SERIAL_BUFERIZED*/

#endif /*DEBUG_SERIAL*/

#endif
