#include <config.h>

#include <stm32f7xx_hal.h>

#include <nvic.h>
#include <tim.h>
#include <uart_int.h>

#include <misc_utils.h>

#if SERIAL_TTY_HAS_DMA

#if SERIAL_TTY_HAS_DMA

#endif /*SERIAL_TTY_HAS_DMA*/

#define TX_FLUSH_TIMEOUT 200 /*MS*/
timer_desc_t uart_hal_wdog_tim;
static rxstream_t rxstream;
#endif

static int          uart_desc_cnt = 0;
static uart_desc_t *uart_desc_pool[MAX_UARTS];

static void dma_rx_xfer_hanlder (struct __DMA_HandleTypeDef * hdma);


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

typedef UART_InitTypeDef hal_uart_ini_t;

uart_desc_t *uart_get_stdio_port (void)
{
    int i;

    for (i = 0; i < uart_desc_cnt; i++) {

        if (uart_desc_pool[i]->initialized &&
            (uart_desc_pool[i]->type == SERIAL_DEBUG)) {

            return uart_desc_pool[i];
        }
    }
    fatal_error("%s() : fail\n");
    return NULL;
}

#if SERIAL_TTY_HAS_DMA

static void hal_tx_wdog_timer_init (uart_desc_t *uart_desc, timer_desc_t *tim);

static void uart1_dma_init (uart_desc_t *uart_desc)
{
    static DMA_HandleTypeDef *hdma_tx, *hdma_rx;
    UART_HandleTypeDef *huart = &uart_desc->handle;
    irqmask_t irqmask, irqmask2;

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

    hal_tx_wdog_timer_init(uart_desc, &uart_hal_wdog_tim);

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
    irq_bmap(&irqmask2);
    irqmask2 = irqmask2 & (~irqmask);

    uart_desc->irq_rxdma = DMA2_Stream2_IRQn;

    if(HAL_UART_Receive_DMA(huart, (uint8_t *)rxstream.dmabuf, sizeof(rxstream.dmabuf)) != HAL_OK)
    {
        fatal_error("%s() : fail\n");
    }
    uart_desc->uart_irq_mask |= irqmask2;
#endif /*DEBUG_SERIAL_USE_RX*/
}

static void uart1_dma_deinit (uart_desc_t *uart_desc)
{
    HAL_NVIC_DisableIRQ(uart_desc->irq_txdma);
    HAL_DMA_DeInit(&uart_desc->hdma_tx);
#if DEBUG_SERIAL_USE_RX
    HAL_NVIC_DisableIRQ(uart_desc->irq_rxdma);
    HAL_DMA_DeInit(&uart_desc->hdma_rx);
#endif
    if (uart_hal_wdog_tim.flags) {
        hal_tim_deinit(&uart_hal_wdog_tim);
    }
}
#endif /*SERIAL_TTY_HAS_DMA*/

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

static void hal_msp_dummy (void)
{
}

typedef struct {
    hal_msp_init_t init, deinit;
} msp_func_tbl_t;

static const msp_func_tbl_t uart1_msp_func[] =
{
    {uart1_msp_init, uart1_msp_deinit},
#if SERIAL_TTY_HAS_DMA
    {uart1_dma_init, uart1_dma_deinit},
#else
    {hal_msp_dummy, hal_msp_dummy},
#endif
    {NULL, NULL},
};

static void uart_hal_if_deinit (uart_desc_t *uart_desc)
{
    UART_HandleTypeDef *handle = &uart_desc->handle;

    if(HAL_UART_DeInit(handle) != HAL_OK)
    {
        fatal_error();
    }
    uart_desc->dma_deinit(uart_desc);
}

static uart_desc_t uart1_desc;

static void uart_hal_if_setup (uart_desc_t *uart_desc, msp_func_tbl_t *func, hal_uart_ini_t *ini)
{
    uart_desc->dma_init = func[1].init;
    uart_desc->dma_deinit = func[1].deinit;
    uart_desc->tx_id = 0;
    uart_desc->tx_allowed = SET;
    uart_desc->hw       = USART1;
    uart_desc->ini      = ini;
    uart_desc->msp_init = func[0].init;
    uart_desc->msp_deinit = func[0].deinit;
    uart_desc->type     = SERIAL_DEBUG;
}

int uart_hal_submit_tx_direct (uart_desc_t *uart_desc, const void *data, size_t cnt)
{
    HAL_StatusTypeDef status;
    cnt = __tty_append_crlf(data, cnt);
    serial_led_on();
    status = HAL_UART_Transmit(&uart_desc->handle, (uint8_t *)data, cnt, 1000);
    serial_led_off();
    return status == HAL_OK ? 0 : -1;
}

#if SERIAL_TTY_HAS_DMA

static void serial_tx_flush_handler (uart_desc_t *uart_desc, int force)
{
    uint32_t time, tstamp;
    int bufpos;

    serial_hal_get_tx_buf(uart_desc, &tstamp, &bufpos);
    if (SET != uart_desc->tx_allowed) {
        return;
    }

    if (0 != bufpos) {

        time = d_time();

        if ((uint32_t)(time - tstamp) > TX_FLUSH_TIMEOUT ||
            force) {

            serial_submit_tx_data(uart_desc, NULL, 0, d_true, time);
            if (force) {
                uart_hal_sync(uart_desc);
            }
        }
    }
}

void uart_hal_sync (uart_desc_t *uart_desc)
{
    volatile FlagStatus *_ready = &uart_desc->tx_allowed;
    while (*_ready != SET) { }
}

static inline void dma_tx_sync (uart_desc_t *uart_desc)
{
    uart_hal_sync(uart_desc);
    uart_desc->tx_allowed = RESET;
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
    int i;
    for (i = 0; i < uart_desc_cnt; i++) {
        serial_tx_flush_handler(uart_desc_pool[i], 0);
    }
}

static void hal_tx_wdog_timer_init (uart_desc_t *uart_desc, timer_desc_t *tim)
{
    tim->flags = TIM_RUNIT;
    tim->period = 1000;
    tim->presc = 10000;
    tim->handler = serial_timer_handler;
    tim->init = serial_timer_msp_init;
    tim->deinit = serial_timer_msp_deinit;
    hal_tim_init(tim , TIM3, TIM3_IRQn);
    uart_desc->uart_irq_mask = tim->irqmask;
}

int uart_hal_submit_tx_data (uart_desc_t *uart_desc, const void *data, size_t cnt)
{
    HAL_StatusTypeDef status;
    irqmask_t irq = 0;

    serial_led_on();

    dma_tx_sync(uart_desc);
    irq_save(&irq);
    status = HAL_UART_Transmit_DMA(&uart_desc->handle, (uint8_t *)data, cnt);
    irq_restore(irq);

    serial_led_off();
    return status == HAL_OK ? 0 : -1;
}

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

void uart_hal_tx_flush (uart_desc_t *desc)
{
    irqmask_t irq_flags = desc->uart_irq_mask;

    irq_save(&irq_flags);
    serial_tx_flush_handler(desc, 1);
    irq_restore(irq_flags);
}

#else /*SERIAL_TTY_HAS_DMA*/

int uart_hal_submit_tx_data (uart_desc_t *uart_desc, const void *data, size_t cnt)
{
    uart_hal_submit_tx_direct(uart_desc, data, cnt);
}

#endif /*SERIAL_TTY_HAS_DMA*/


#if DEBUG_SERIAL_USE_RX

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

void serial_rx_cplt_handler (uint8_t full)
{
extern int32_t g_serial_rxtx_eol_sens;
    int dmacplt = sizeof(rxstream.dmabuf) / 2;
    int cnt = dmacplt;
    char *src;

    src = &rxstream.dmabuf[full * dmacplt];

    while (cnt) {
        rxstream.fifo[rxstream.fifoidx++ & (DMA_RX_FIFO_SIZE - 1)] = *src;
        rxstream.eof |= __tty_is_crlf_char(*src);
        src++; cnt--;
    }
    if (!g_serial_rxtx_eol_sens) {
        rxstream.eof = 3; /*\r & \n met*/
    }
}

int uart_hal_rx_flush (uart_desc_t *uart_desc, char *dest, int *pcnt)
{
    irqmask_t irq = uart_desc->uart_irq_mask;
    rxstream_t *rx = &rxstream;

    int cnt = (int)(rx->fifoidx - rx->fifordidx);
    int bytesleft = 0;

    /*TODO : remove 0x3*/
    if (rx->eof != 0x3) {
        *pcnt = 0;
        return 0;
    }

    irq_save(&irq);

    if (cnt < 0) {
        assert(0);
    } else if (*pcnt < cnt) {
        bytesleft = cnt - *pcnt;
        cnt = cnt - bytesleft;
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
    irq_restore(irq);
    return bytesleft;
}

static void dma_rx_xfer_hanlder (struct __DMA_HandleTypeDef * hdma)
{
    HAL_DMA_IRQHandler(hdma);
}
#endif /*DEBUG_SERIAL_USE_RX*/


static void uart_handle_irq (USART_TypeDef *source)
{
    uart_desc_t *uart_desc = uart_find_desc(source);

    if (uart_desc) {
        HAL_UART_IRQHandler(&uart_desc->handle);
    }
}

uart_desc_t *uart_find_desc (void *source)
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

void uart_if_deinit (void)
{
    int i = 0;
    dprintf("%s() :\n", __func__);
    serial_flush();
    for (i = 0; i < uart_desc_cnt; i++) {
        uart_hal_if_deinit(uart_desc_pool[i]);
    }
    uart_desc_cnt = 0;
}

static void uart_hal_if_init (uart_desc_t *uart_desc)
{
    UART_HandleTypeDef *handle = &uart_desc->handle;
    handle->Instance        = uart_desc->hw;

    d_memcpy(&handle->Init, uart_desc->ini, sizeof(handle->Init));

    if(HAL_UART_DeInit(handle) != HAL_OK)
    {
        fatal_error("HAL_UART_DeInit : fail\n");
    }
    if(HAL_UART_Init(handle) != HAL_OK)
    {
        fatal_error("HAL_UART_Init : fail\n");
    }
#if SERIAL_TTY_HAS_DMA
    uart_desc->dma_init(uart_desc);
#endif
    uart_desc->initialized = SET;
}

int uart_hal_tty_init (void)
{
#if DEBUG_SERIAL_USE_RX
    const hal_uart_ini_t *def_ini = &uart_115200_8n1_txrx;
#else
    const hal_uart_ini_t *def_ini = &uart_115200_8n1_tx;
#endif
    if (uart_desc_cnt >= MAX_UARTS) {
        return -1;
    }
    uart_desc_pool[uart_desc_cnt++] = &uart1_desc;

    uart_hal_if_setup(&uart1_desc, uart1_msp_func, def_ini);
    uart_hal_if_init(&uart1_desc);
}

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

void USART1_IRQHandler (void)
{
    uart_handle_irq(USART1);
}

#if SERIAL_TTY_HAS_DMA

void TIM3_IRQHandler (void)
{
    tim_hal_irq_handler(&uart_hal_wdog_tim);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *UartHandle)
{
    uart_desc_t *uart_desc = uart_find_desc(UartHandle->Instance);
    if (!uart_desc) {
        return;
    }
    uart_desc->tx_allowed = SET;
}

void DMA2_Stream7_IRQHandler (void)
{
    dma_tx_handle_irq(DMA2_Stream7);
}

void DMA2_Stream2_IRQHandler (void)
{
    dma_rx_handle_irq(DMA2_Stream2);
}

#endif /*SERIAL_TTY_HAS_DMA*/

#if DEBUG_SERIAL_USE_RX
void HAL_UART_RxHalfCpltCallback (UART_HandleTypeDef* huart)
{
    serial_rx_cplt_handler(0);
}

void HAL_UART_RxCpltCallback (UART_HandleTypeDef* huart)
{
    serial_rx_cplt_handler(1);
}

#endif

