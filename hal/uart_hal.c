
#include <dev_conf.h>

#include <stm32f7xx_hal.h>

#include <nvic.h>
#include <tim.h>
#include <uart_int.h>

#include <misc_utils.h>

streambuf_t streambuf[STREAM_BUFCNT];
timer_desc_t serial_timer;

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

static void uart1_dma_init (uart_desc_t *uart_desc)
{
#if DEBUG_SERIAL_USE_DMA
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
#endif /*DEBUG_SERIAL_USE_DMA*/

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
        fatal_error("%s() : fail\n");
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

static void _serial_deinit (uart_desc_t *uart_desc)
{
    UART_HandleTypeDef *handle = &uart_desc->handle;

    if(HAL_UART_DeInit(handle) != HAL_OK)
    {
        fatal_error();
    }
    uart_desc->dma_deinit(uart_desc);
}


#if DEBUG_SERIAL_USE_DMA

irqmask_t dma_rx_irq_mask = 0;

static inline void dma_tx_sync (uart_desc_t *uart_desc)
{
    while (uart_desc->uart_tx_ready != SET) { }
    uart_desc->uart_tx_ready = RESET;
}

void uart_hal_sync (uart_desc_t *uart_desc)
{
    while (uart_desc->uart_tx_ready != SET) { }
}

#endif /*DEBUG_SERIAL_USE_DMA*/

static void dma_rx_xfer_hanlder (struct __DMA_HandleTypeDef * hdma);
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
    serial_rx_flush_handler(0);
}

void TIM3_IRQHandler (void)
{
    tim_hal_irq_handler(&serial_timer);
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

int serial_submit_to_hw (uart_desc_t *uart_desc, const void *data, size_t cnt)
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

static void uart_handle_irq (USART_TypeDef *source)
{
    uart_desc_t *uart_desc = uart_find_desc(source);

    if (uart_desc) {
        HAL_UART_IRQHandler(&uart_desc->handle);
    }
}

void HAL_UART_RxHalfCpltCallback (UART_HandleTypeDef* huart)
{
    serial_rx_cplt_handler(0);
}

void HAL_UART_RxCpltCallback (UART_HandleTypeDef* huart)
{
    serial_rx_cplt_handler(1);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *UartHandle)
{
    uart_desc_t *uart_desc = uart_find_desc(UartHandle->Instance);
    if (!uart_desc) {
        return;
    }
    uart_desc->uart_tx_ready = SET;
}

static void dma_rx_handle_irq (const DMA_Stream_TypeDef *source)
{
#if DEBUG_SERIAL_USE_RX
    uart_desc_t *dsc       = uart_desc_pool[0],
                *dsc_end   = uart_desc_pool[uart_desc_cnt];

    for (; dsc != dsc_end; dsc++) {
        if (dsc->hdma_rx.Instance == source) {
            dma_rx_xfer_hanlder(&dsc->hdma_rx);
            break;
        }
    }
#endif /*DEBUG_SERIAL_USE_RX*/
}

void DMA2_Stream7_IRQHandler (void)
{
    dma_tx_handle_irq(DMA2_Stream7);
}

void DMA2_Stream2_IRQHandler (void)
{
    dma_rx_handle_irq(DMA2_Stream2);
}

static void dma_rx_xfer_hanlder (struct __DMA_HandleTypeDef * hdma)
{
    HAL_DMA_IRQHandler(hdma);
}

void USART1_IRQHandler (void)
{
    uart_handle_irq(USART1);
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

static void _serial_init (uart_desc_t *uart_desc)
{
    UART_HandleTypeDef *handle = &uart_desc->handle;

    handle->Instance        = uart_desc->hw;

    memcpy(&handle->Init, uart_desc->cfg, sizeof(handle->Init));

    if(HAL_UART_DeInit(handle) != HAL_OK)
    {
        fatal_error("%s() : fail\n");
    }
    if(HAL_UART_Init(handle) != HAL_OK)
    {
        fatal_error("%s() : fail\n");
    }
    uart_desc->dma_init(uart_desc);
    uart_desc->initialized = SET;
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
    tim_hal_set_hw(&serial_timer, TIM3, TIM3_IRQn);
    return hal_tim_init(&serial_timer);
}

