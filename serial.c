#include "stdint.h"
#include "string.h"
#include "stdarg.h"
#include "debug.h"
#include "main.h"
#include "dev_conf.h"
#include "stm32f7xx_it.h"
#include "nvic.h"
#include "heap.h"
#include "misc_utils.h"

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
    
#if DEBUG_SERIAL_USE_DMA
    uart_dma_init_t         dma_init;
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
    char data[STREAM_BUFSIZE];
    int  bufposition;
    uint32_t timestamp;
} streambuf_t;

static streambuf_t streambuf[STREAM_BUFCNT] DTCM;

#endif /*DEBUG_SERIAL_BUFERIZED*/

#if DEBUG_SERIAL_USE_RX

/*As far as i want to keep 'console' interactive,
  each received symbol must be immediately accepted, e.g. -
  any string will be processed char by char with cr(and\or)lf at the end,
  i see only one way to do this in more sophisticated way - flush rx buffer within timer.
  TODO : extend rx buffer size to gain more perf.
*/
#define DMA_RX_SIZE 1
#define DMA_RX_FIFO_SIZE (1 << 8)

typedef struct {
    uint16_t cnt;
    uint16_t crlf;
    char buf[DMA_RX_SIZE * 2];
    char fifo[DMA_RX_FIFO_SIZE];
} rxstream_t;

static rxstream_t rxstream DTCM;

#endif /*DEBUG_SERIAL_USE_RX*/

static void serial_timer_init (void);
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

static irqmask_t timer_irq_mask = 0;
static irqmask_t dma_rx_irq_mask = 0;

static inline void dma_tx_sync (uart_desc_t *uart_desc)
{
    while (uart_desc->uart_tx_ready != SET) { }
    uart_desc->uart_tx_ready = RESET;
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

    if(HAL_UART_Receive_DMA(huart, (uint8_t *)rxstream.buf, sizeof(rxstream.buf)) != HAL_OK)
    {
        serial_fatal();
    }
#else
    UNUSED(hdma_rx);
#endif
    UNUSED(hdma_tx);
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

#if DEBUG_SERIAL_BUFERIZED

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

static void dbgstream_apend_data (streambuf_t *stbuf, const void *data, size_t size)
{
    char *p = stbuf->data + stbuf->bufposition;
    memcpy(p, data, size);
    if (stbuf->bufposition == 0) {
        stbuf->timestamp = HAL_GetTick();
    }
    stbuf->bufposition += size;
}

static inline int dbgstream_bufferize (uart_desc_t *uart_desc, const void *data, size_t size)
{
    streambuf_t *active_stream = &streambuf[uart_desc->active_stream & STREAM_BUFCNT_MS];

    if (size >= STREAM_BUFSIZE) {
        return 0;
    }

    if (size >= (STREAM_BUFSIZE - active_stream->bufposition)) {
        dbgstream_send(uart_desc, active_stream);
        active_stream = &streambuf[(++uart_desc->active_stream) & STREAM_BUFCNT_MS];
    }

    if (size >= (STREAM_BUFSIZE - active_stream->bufposition)) {
        serial_fatal();
    }
    dbgstream_apend_data(active_stream, data, size);
    return size;
}

#else /*DEBUG_SERIAL_BUFERIZED*/

static inline int dbgstream_bufferize (uart_desc_t *uart_desc, const void *data, size_t size)
{
    UNUSED(uart_desc);
    UNUSED(data);
    UNUSED(size);
}

#endif /*DEBUG_SERIAL_BUFERIZED*/

static HAL_StatusTypeDef _serial_send (const void *data, size_t cnt)
{
    irqmask_t irq_flags = timer_irq_mask;
    HAL_StatusTypeDef status = HAL_OK;
    uart_desc_t *uart_desc = debug_port();

    irq_save(&irq_flags);

    if (dbgstream_bufferize(uart_desc, data, cnt) <= 0) {
        status = serial_submit_to_hw(uart_desc, data, cnt);
    }

    irq_restore(irq_flags);

    return status;
}

#if SERIAL_TSF
static int16_t prev_putc = 0xff;

static inline d_bool __newline_char (char c)
{
    if ('\n' == c ||
        '\r' == c) {

        return d_true;
    }
    return d_false;
}
#endif

void serial_putc (char c)
{
    HAL_StatusTypeDef status = HAL_OK;
    uint8_t buf[1] = {c};

#if SERIAL_TSF
    if (prev_putc < 0 ||
        __newline_char((char)prev_putc)) {

        dprintf("%c", c);
    } else {
        status = _serial_send(buf, sizeof(buf));
    }
    prev_putc = c;
#else
    status = _serial_send(buf, sizeof(buf));
#endif
    if (status != HAL_OK){
        serial_fatal();
    }
}

char serial_getc (void)
{
    /*TODO : Use Rx from USART */
    return 0;
}

void serial_send_buf (const void *data, size_t cnt)
{
    HAL_StatusTypeDef status;

    status = _serial_send(data, cnt);

    if (status != HAL_OK){
        serial_fatal();
    }
}

void serial_flush (void)
{
    irqmask_t irq_flags = timer_irq_mask;

    irq_save(&irq_flags);
    serial_flush_handler(1);
    irq_restore(irq_flags);
}

#if SERIAL_TSF

#define MSEC 1000

static void inline __set_newline (const char *str)
{
    prev_putc = 0;
    while (*str) {
        if (__newline_char(*str)) {
            prev_putc = *str;
            break;
        }
        str++;
    }
}

static inline int __insert_tsf (const char *fmt, char *buf, int max)
{
    uint32_t msec, sec;

    if (prev_putc < 0 &&
        !__newline_char(prev_putc)) {

        return 0;
    }
    msec = HAL_GetTick();
    sec = msec / MSEC;
    return snprintf(buf, max, "[%10d.%03d] ", sec, msec % MSEC);
}

void dprintf (const char *fmt, ...)
{
    va_list         argptr;
    char            string[1024];
    int size, max = sizeof(string);

    va_start (argptr, fmt);
    size = __insert_tsf(fmt, string, max);
    size += vsnprintf (string + size, max - size, fmt, argptr);
    va_end (argptr);

    assert(size < arrlen(string));
    serial_send_buf(string, size);
    __set_newline(fmt);
}

void dvprintf (const char *fmt, va_list argptr)
{
    char            string[1024];
    int size, max = sizeof(string);

    size = __insert_tsf(fmt, string, sizeof(string));
    size += vsnprintf (string + size, max - size, fmt, argptr);

    assert(size < arrlen(string));
    serial_send_buf(string, size);
    __set_newline(fmt);
}


#else /*SERIAL_TSF*/

void dprintf (const char *fmt, ...)
{
    va_list         argptr;
    /*TODO : use local buf*/
    static char            string[1024];
    int size;

    va_start (argptr, fmt);
    size = vsnprintf (string, sizeof(string), fmt, argptr);
    va_end (argptr);

    serial_send_buf(string, size);
}

void dvprintf (const char *fmt, va_list argptr)
{
    char            string[1024];
    int size;

    size = vsnprintf (string, sizeof(string), fmt, argptr);

    serial_send_buf(string, size);
}

#endif /*SERIAL_TSF*/

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

static int _is_crlf (char c)
{
    if (c == '\n' || c == '\r') {
        return 1;
    }
    return 0;
}

static void _dma_rx_xfer_cplt (uint8_t full)
{
    uint16_t wridx;
    int cnt = DMA_RX_SIZE, total;
    char *src, *dst;

    if (rxstream.cnt >= DMA_RX_FIFO_SIZE) {
        if (!rxstream.crlf) {
            /*Too long string, reset it*/
            rxstream.cnt = 0;
        }
        return;
        /*Full, exit*/
    }
    src = &rxstream.buf[full * DMA_RX_SIZE];
    wridx = rxstream.cnt;
    if (wridx + DMA_RX_SIZE + 1 > DMA_RX_FIFO_SIZE) {
        cnt = DMA_RX_FIFO_SIZE - wridx - 1;
    }
    total = rxstream.cnt + cnt;
    dst = rxstream.fifo + wridx;
    while (cnt) {
        *dst = *src;
        if (_is_crlf(*dst)) {
            rxstream.crlf = cnt;
            *dst = 0;
            break;
        }
        src++; dst++; cnt--;
    }
    rxstream.cnt = total - cnt;
}

static void dma_fifo_flush (rxstream_t *rx, char *dest, int *pcnt)
{
    uint16_t cnt = rx->cnt;
    char *d = dest, *s = rx->fifo;

    while (cnt) {
        *d = *s;
        d++; s++; cnt--;
    }
    *d = 0;
    /*TODO : Move next line after crlf to the begining,
      to handle multiple lines.
    */
    *pcnt = rx->cnt;
    rx->crlf = 0;
    rx->cnt = 0;
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

#define DEBUG_SERIAL_MAX_CLBK 4

static serial_rx_clbk_t serial_rx_clbk[DEBUG_SERIAL_MAX_CLBK] = {NULL};
static serial_rx_clbk_t *last_rx_clbk = &serial_rx_clbk[0];

void serial_rx_callback (serial_rx_clbk_t clbk)
{
    if (last_rx_clbk == &serial_rx_clbk[DEBUG_SERIAL_MAX_CLBK]) {
        return;
    }
    *last_rx_clbk++ = clbk;
}

void serial_tickle (void)
{
    irqmask_t irq = dma_rx_irq_mask;
    serial_rx_clbk_t *clbk = &serial_rx_clbk[0];
    char buf[DMA_RX_FIFO_SIZE + 1];
    char *pbuf = buf;
    int cnt, prev_cnt, idx;
    int attemption = 0;

    irq_save(&irq);
    if (!rxstream.crlf || *clbk == NULL) {
        irq_restore(irq);
        return;
    }

    dma_fifo_flush(&rxstream, pbuf, &cnt);
    irq_restore(irq);

    prev_cnt = cnt;
doparse:
    while (cnt > 0 && *pbuf && clbk != last_rx_clbk) {

        idx = (*clbk)(pbuf, cnt);
        if (idx < 0 || idx >= cnt) {
            return;
            /*handled*/
        }
        if (idx > 0) {
            pbuf += idx;
            cnt -= idx;
        }
        clbk++;
        attemption++;
    }
    if (cnt > 0) {
        /*Try until all text will be parsed*/
        if (prev_cnt == cnt) {
            dprintf("Cannot parse text \'%s\'\n", pbuf);
            dprintf("Attemptions : %i\n", attemption);
            return;
        }
        prev_cnt = cnt;
        clbk = &serial_rx_clbk[0];
        goto doparse;
    }
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

#if DEBUG_SERIAL_BUFERIZED

extern uint32_t SystemCoreClock;

typedef struct {
    TIM_TypeDef *hw;
    TIM_HandleTypeDef handle;
    IRQn_Type irq;
} timer_desc_t;

static timer_desc_t serial_timer;

static void serial_flush_handler (int force)
{
    uart_desc_t *uart_desc = debug_port();
    streambuf_t *stbuf = &streambuf[0], *stbuflast = &streambuf[STREAM_BUFCNT - 1];
    uint32_t time;

    if (!uart_desc->uart_tx_ready) {
        return;
    }

    time = HAL_GetTick();

    while (1) {
        if (0 != stbuf->bufposition) {

            if ((stbuf->timestamp && (time - stbuf->timestamp) > TX_FLUSH_TIMEOUT) ||
                force) {

                dbgstream_send(uart_desc, stbuf);
                if (stbuf->bufposition == 0) {
                    break;
                }
            }
        }
        if (stbuf == stbuflast) {
            break;
        }
        stbuf++;
    }
}

static void serial_timer_init (void)
{
    TIM_HandleTypeDef *handle = &serial_timer.handle;
    uint32_t uwPrescalerValue = (uint32_t)((SystemCoreClock / 2) / 10000) - 1;
    irqmask_t irq_flags;
    serial_timer.hw = TIM3;
    serial_timer.irq = TIM3_IRQn;

    handle->Instance = serial_timer.hw;

    handle->Init.Period            = 1000 - 1;
    handle->Init.Prescaler         = uwPrescalerValue;
    handle->Init.ClockDivision     = 0;
    handle->Init.CounterMode       = TIM_COUNTERMODE_UP;
    handle->Init.RepetitionCounter = 0;
    handle->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    irq_bmap(&irq_flags);
    if (HAL_TIM_Base_Init(handle) != HAL_OK)
    {
        serial_fatal();
    }
    irq_bmap(&timer_irq_mask);
    timer_irq_mask = timer_irq_mask & (~irq_flags);

    if (HAL_TIM_Base_Start_IT(handle) != HAL_OK)
    {
        serial_fatal();
    }
}

/*TODO : move this outside*/
extern TIM_HandleTypeDef profile_timer_handle;

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (&serial_timer.handle == htim) {
        __HAL_RCC_TIM3_CLK_ENABLE();


        HAL_NVIC_SetPriority(serial_timer.irq, 0, 1);

        HAL_NVIC_EnableIRQ(serial_timer.irq);
    } else if (&profile_timer_handle == htim) {
        __HAL_RCC_TIM2_CLK_ENABLE();
    }
}

void TIM3_IRQHandler (void)
{
    HAL_TIM_IRQHandler(&serial_timer.handle);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (&serial_timer.handle == htim) {

        serial_flush_handler(0);
    }
}


#endif /*DEBUG_SERIAL_BUFERIZED*/

void hexdump (const uint8_t *data, int len, int rowlength)
{
    int x, y, xn;
    dprintf("%s :\n", __func__);
    for (y = 0; y <= len / rowlength; y++) {
        xn = len < rowlength ? len : rowlength;
        dprintf("%d:%d    ", y, y + xn);
        for (x = 0; x < xn; x++) {
            dprintf("0x%02x, ", data[x + y * rowlength]);
        }
        dprintf("\n");
    }
}

#endif /*DEBUG_SERIAL*/

