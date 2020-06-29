#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

#include <config.h>

#if defined(STM32H745xx) || defined(STM32H747xx)
#include <stm32h7xx_hal.h>
#include <stm32h7xx_hal_uart.h>
#elif defined(STM32F769xx)
#include <stm32f7xx_hal.h>
#include <stm32f7xx_hal_uart.h>
#else
#error
#endif

#include <arch.h>
#include <bsp_api.h>
#include <nvic.h>
#include <tim.h>
#include <serial.h>
#include <misc_utils.h>
#include <uart_int.h>
#include <dev_io.h>
#include <heap.h>
#include <term.h>
#include <gpio.h>

static uart_hal_t *hal_tty_get_desc_4_phy (void *source);
static void hal_tty_wdog_init (uart_hal_t *uart_desc, timer_desc_t *tim);

typedef struct {
    hal_msp_init_t init, deinit;
} msp_func_tbl_t;

typedef struct {
    uart_hal_t *pool[MAX_UARTS];
    int        cnt;
} hal_usrt_pool_t;

static hal_usrt_pool_t hal_usrt_pool;
timer_desc_t uart_hal_wdog_tim;
static uart_hal_t *uart_vcom_desc;

static const UART_InitTypeDef uart_115200_8n1_txrx =
{
    115200,
    UART_WORDLENGTH_8B,
    UART_STOPBITS_1,
    UART_PARITY_NONE,
    UART_MODE_TX_RX,
    UART_HWCONTROL_NONE,
    0,
    0,
};

#if defined(STM32F769xx)

#elif defined(STM32H745xx) || defined(STM32H747xx)

#define TTY_DMA_TX_Stream DMA2_Stream7
#define TTY_DMA_RX_Stream DMA2_Stream2

#define TTY_DMA_TX_Irq DMA2_Stream7_IRQn
#define TTY_DMA_RX_Irq DMA2_Stream2_IRQn

#define TTY_DMA_TX_IRQHandler DMA2_Stream7_IRQHandler
#define TTY_DMA_RX_IRQHandler DMA2_Stream2_IRQHandler

static const struct uart_dma_conf_s {
    DMA_InitTypeDef init;
    DMA_Stream_TypeDef *inst;
    IRQn_Type irq;
    int prio, group;
} uart1_tx_dma_conf =
{
        {
            DMA_REQUEST_USART1_TX,
            DMA_MEMORY_TO_PERIPH,
            DMA_PINC_DISABLE,
            DMA_MINC_ENABLE,
            DMA_PDATAALIGN_BYTE,
            DMA_MDATAALIGN_BYTE,
            DMA_NORMAL,
            DMA_PRIORITY_HIGH,
            DMA_FIFOMODE_DISABLE,
            DMA_FIFO_THRESHOLD_FULL,
            DMA_MBURST_SINGLE,
            DMA_PBURST_SINGLE,
        },
    TTY_DMA_TX_Stream,
    TTY_DMA_TX_Irq,
    0, 1
},
uart1_rx_dma_conf = 
{
        {
            DMA_REQUEST_USART1_RX,
            DMA_PERIPH_TO_MEMORY,
            DMA_PINC_DISABLE,
            DMA_MINC_ENABLE,
            DMA_PDATAALIGN_BYTE,
            DMA_MDATAALIGN_BYTE,
            DMA_CIRCULAR,
            DMA_PRIORITY_LOW,
            DMA_MBURST_SINGLE,
            DMA_PBURST_SINGLE,
        },
    TTY_DMA_RX_Stream,
    TTY_DMA_RX_Irq,
    0, 1
};

#endif /**/

static void uart1_dma_init (uart_hal_t *uart_desc)
{
    DMA_HandleTypeDef *hdma_tx, *hdma_rx;
    UART_HandleTypeDef *huart = &uart_desc->phy->handle;
    irqmask_t irqmask, irqmask2;

    __HAL_RCC_DMA2_CLK_ENABLE();

    hdma_tx = &uart_desc->phy->hdma_tx;

#if defined(STM32F769xx)
    hdma_tx->Instance                 = DMA2_Stream7;
    hdma_tx->Init.Channel             = DMA_CHANNEL_4;
    hdma_tx->Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_tx->Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_tx->Init.MemInc              = DMA_MINC_ENABLE;
    hdma_tx->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_tx->Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_tx->Init.Mode                = DMA_NORMAL;
    hdma_tx->Init.Priority            = DMA_PRIORITY_LOW;
#elif defined(STM32H745xx) || defined(STM32H747xx)
    hdma_tx->Instance                 = uart1_tx_dma_conf.inst;
    d_memcpy(&hdma_tx->Init, &uart1_tx_dma_conf.init, sizeof(hdma_tx->Init));
#else
#error
#endif
    __HAL_LINKDMA(huart, hdmatx, *hdma_tx);
    HAL_DMA_DeInit(hdma_tx);
    HAL_DMA_Init(hdma_tx);
    HAL_NVIC_SetPriority(uart1_tx_dma_conf.irq, uart1_tx_dma_conf.prio, uart1_tx_dma_conf.group);
    HAL_NVIC_EnableIRQ(uart1_tx_dma_conf.irq);

    uart_desc->phy->irq_txdma = uart1_tx_dma_conf.irq;
    hal_tty_wdog_init(uart_desc, &uart_hal_wdog_tim);

    uart_desc->tty.irqmask = uart_hal_wdog_tim.irqmask;

#if defined(STM32H745xx) || defined(STM32H747xx)
    if(HAL_UART_Receive_IT(huart, (uint8_t *)&uart_desc->tty.rxbuf.dmabuf[1], 1) != HAL_OK)
    {
        for (;;) {}
    }

#elif defined(STM32F769xx)

    hdma_rx = &uart_desc->phy->hdma_rx;
#if defined(STM32F769xx)
    hdma_rx->Instance                 = DMA2_Stream2;
    hdma_rx->Init.Channel             = DMA_CHANNEL_4;
    hdma_rx->Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_rx->Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_rx->Init.MemInc              = DMA_MINC_ENABLE;
    hdma_rx->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_rx->Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    hdma_rx->Init.Mode                = DMA_CIRCULAR;
    hdma_rx->Init.Priority            = DMA_PRIORITY_LOW;
#elif defined(STM32H745xx) || defined(STM32H747xx)
    hdma_rx->Instance                 = uart1_rx_dma_conf.inst;
    d_memcpy(&hdma_rx->Init, &uart1_rx_dma_conf.init, sizeof(hdma_rx->Init));
#else
#error
#endif
    HAL_DMA_DeInit(hdma_rx);
    HAL_DMA_Init(hdma_rx);
    __HAL_LINKDMA(huart, hdmarx, *hdma_rx);
    irq_bmap(&irqmask);
    HAL_NVIC_SetPriority(uart1_rx_dma_conf.irq, uart1_rx_dma_conf.prio, uart1_rx_dma_conf.group);
    HAL_NVIC_EnableIRQ(uart1_rx_dma_conf.irq);
    irq_bmap(&irqmask2);
    irqmask2 = irqmask2 & (~irqmask);

    uart_desc->phy->irq_rxdma = uart1_rx_dma_conf.irq;
    if(HAL_UART_Receive_DMA(huart, (uint8_t *)uart_desc->tty.rxbuf.dmabuf, sizeof(uart_desc->tty.rxbuf.dmabuf)) != HAL_OK)
    {
        for (;;) {}
    }
    uart_desc->phy->uart_irq_mask |= irqmask2;

#endif /*defined(STM32F769xx)*/
}

static void uart1_dma_deinit (uart_hal_t *uart_desc)
{
    HAL_NVIC_DisableIRQ((IRQn_Type)uart_desc->phy->irq_txdma);
    HAL_DMA_DeInit(&uart_desc->phy->hdma_tx);
    HAL_NVIC_DisableIRQ((IRQn_Type)uart_desc->phy->irq_rxdma);
    HAL_DMA_DeInit(&uart_desc->phy->hdma_rx);
    if (uart_hal_wdog_tim.flags) {
        hal_timer_deinit(&uart_hal_wdog_tim);
    }
}

static void uart1_msp_init (uart_hal_t *uart_desc)
{
    UART_HandleTypeDef *huart = &uart_desc->phy->handle;
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

    uart_desc->phy->irq_uart  = USART1_IRQn;
}

static void uart1_msp_deinit (uart_hal_t *uart_desc)
{
    HAL_NVIC_DisableIRQ((IRQn_Type)uart_desc->phy->irq_uart);
}

static const msp_func_tbl_t uart1_msp_func[] =
{
    {uart1_msp_init, uart1_msp_deinit},
    {uart1_dma_init, uart1_dma_deinit},
    {NULL, NULL},
};

static int hal_tty_uart_deinit (uart_hal_t *uart_desc)
{
    if(HAL_UART_DeInit(&uart_desc->phy->handle) != HAL_OK) {
        return -1;
    }
    uart_desc->phy->dma_deinit(uart_desc);
    return 0;
}

static void hal_tty_uart_preinit (uart_hal_t *uart_desc, const msp_func_tbl_t *func)
{
    if (uart_desc->phy) {
        uart_desc->phy->dma_init              = func[1].init;
        uart_desc->phy->dma_deinit            = func[1].deinit;
        uart_desc->phy->msp_init              = func[0].init;
        uart_desc->phy->msp_deinit            = func[0].deinit;
        uart_desc->phy->tx_dma_ready          = SET;
        uart_desc->phy->handle.Instance       = USART1;
    }
    uart_desc->tty.tx_bufsize = 0;
    uart_desc->tty.initialized = 0;
}

static void hal_tty_wdog_msp_init (timer_desc_t *desc)
{
    __HAL_RCC_TIM3_CLK_ENABLE();
    HAL_NVIC_SetPriority((IRQn_Type)desc->irq, 0, 1);
    HAL_NVIC_EnableIRQ((IRQn_Type)desc->irq);
}

static void hal_tty_wdog_msp_deinit (timer_desc_t *desc)
{
    __HAL_RCC_TIM3_CLK_DISABLE();
    HAL_NVIC_DisableIRQ((IRQn_Type)desc->irq);
}

static void hal_tty_wdog_handler (timer_desc_t *desc)
{
    int i;
    for (i = 0; i < hal_usrt_pool.cnt; i++) {
        serial_tty_tx_flush_proc(&hal_usrt_pool.pool[i]->tty, d_false);
    }
}

static void hal_tty_wdog_init (uart_hal_t *uart_desc, timer_desc_t *tim)
{
    tim->flags = TIM_RUNIT;
    tim->period = 1000;
    tim->presc = 10000;
    tim->handler = hal_tty_wdog_handler;
    tim->init = hal_tty_wdog_msp_init;
    tim->deinit = hal_tty_wdog_msp_deinit;
    hal_timer_init(tim , TIM3, TIM3_IRQn);
    uart_desc->phy->uart_irq_mask = tim->irqmask;
}

int hal_tty_tx_start_poll (uart_hal_t *uart_desc, const void *data, size_t cnt)
{
    HAL_StatusTypeDef status;
    serial_led_on();
    status = HAL_UART_Transmit(&uart_desc->phy->handle, (uint8_t *)data, cnt, 1000);
    serial_led_off();
    return status == HAL_OK ? 0 : -1;
}

static inline void hal_tty_sync_dma_tx (uart_hal_t *uart_desc)
{
    while (uart_desc->phy->tx_dma_ready == RESET) {;}
    uart_desc->phy->tx_dma_ready = RESET;
}

int hal_tty_tx_start_dma (uart_hal_t *uart_desc, const void *data, size_t cnt)
{
    HAL_StatusTypeDef status;
    irqmask_t irq = 0;

    serial_led_on();

    hal_tty_sync_dma_tx(uart_desc);
    irq_save(&irq);
    status = HAL_UART_Transmit_DMA(&uart_desc->phy->handle, (uint8_t *)data, cnt);
    irq_restore(irq);

    serial_led_off();
    return status == HAL_OK ? 0 : -1;
}

static void dma_tx_handle_irq (const DMA_Stream_TypeDef *source)
{
    int i;
    DMA_HandleTypeDef *hdma;

    for (i = 0; i < hal_usrt_pool.cnt; i++) {
        hdma = &hal_usrt_pool.pool[i]->phy->hdma_tx;
        if (hdma->Instance == source) {
            HAL_DMA_IRQHandler(hdma);
            break;
        }
    }
}

void hal_tty_flush (uart_hal_t *desc)
{
    irqmask_t irq_flags = desc->phy->uart_irq_mask;
    int flushed;

    irq_save(&irq_flags);
    flushed = serial_tty_tx_flush_proc(&desc->tty, d_true);
    irq_restore(irq_flags);
}

static int hal_tty_tx_start_hw (serial_tty_t *tty, tty_txbuf_t *txbuf)
{
    uart_hal_t *uart_desc = container_of(tty, uart_hal_t, tty);
    if (uart_desc->phy == NULL) {
        return -1;
    }
    tty->txbuf_pending = txbuf;
    if (!tty->tx_bufsize) {
        hal_tty_tx_start_poll(uart_desc, txbuf->data, txbuf->data_cnt);
        tty->txbuf_pending = NULL;
    } else {
        hal_tty_tx_start_dma(uart_desc, txbuf->data, txbuf->data_cnt);
    }
    return 0;
}

static void dma_rx_handle_irq (const DMA_Stream_TypeDef *source)
{
    uart_hal_t *dsc       = hal_usrt_pool.pool[0],
               *dsc_end   = hal_usrt_pool.pool[hal_usrt_pool.cnt];

    for (; dsc != dsc_end; dsc++) {
        if (dsc->phy->hdma_rx.Instance == source) {
            HAL_DMA_IRQHandler(&dsc->phy->hdma_rx);
            break;
        }
    }
}

static void hal_tty_rx_cplt_hander (uart_hal_t *uart_desc, uint8_t full)
{
    int dmacplt = sizeof(uart_desc->tty.rxbuf.dmabuf) / 2;
    int cnt = dmacplt;
    char *src;

    src = &uart_desc->tty.rxbuf.dmabuf[full * dmacplt];

    while (cnt) {
        uart_desc->tty.rxbuf.fifo[uart_desc->tty.rxbuf.fifoidx++ & (DMA_RX_FIFO_SIZE - 1)] = *src;
        uart_desc->tty.rxbuf.eof |= __tty_is_crlf_char(*src);
        src++; cnt--;
    }
}

void hal_tty_rx_cplt_any (uint8_t full)
{
    int i;
    for (i = 0; i < hal_usrt_pool.cnt; i++) {
        hal_tty_rx_cplt_hander(hal_usrt_pool.pool[i], full);
    }
}

static int hal_tty_rx_poll (serial_tty_t *tty, char *dest, int *pcnt)
{
    uart_hal_t *uart_desc = container_of(tty, uart_hal_t, tty);
    irqmask_t irq = uart_desc->phy->uart_irq_mask;
    tty_rxbuf_t *rx = &tty->rxbuf;
    int cnt = (int)(rx->fifoidx - rx->fifordidx), bytesleft = 0;

    /*TODO : remove 0x3*/
    if (rx->eof != 0x3) {
        *pcnt = 0;
        return 0;
    }
    irq_save(&irq);
    if (*pcnt < cnt) {
        bytesleft = cnt - *pcnt;
        cnt = cnt - bytesleft;
    }
    *pcnt = cnt;
    while (cnt > 0) {
        *dest = rx->fifo[rx->fifordidx++ & (DMA_RX_FIFO_SIZE - 1)];
        cnt--; dest++;
    }
    *dest = 0;
    rx->eof = 0;
    irq_restore(irq);
    return bytesleft;
}

static void hal_tty_irq_handler (USART_TypeDef *source)
{
    uart_hal_t *uart_desc = hal_tty_get_desc_4_phy(source);

    if (uart_desc) {
        HAL_UART_IRQHandler(&uart_desc->phy->handle);
    }
}

void hal_tty_destroy_any (void)
{
    int i = 0;
    uart_hal_t *hal;
    for (i = 0; i < hal_usrt_pool.cnt; i++) {
        hal = hal_usrt_pool.pool[i];
        if (hal->phy) {
            hal_tty_flush(hal_usrt_pool.pool[i]);
            hal_tty_uart_deinit(hal_usrt_pool.pool[i]);
        }
        heap_free(hal);
    }
    hal_usrt_pool.cnt = 0;
}

static int hal_tty_if_init (uart_hal_t *uart_desc, const UART_InitTypeDef *config)
{
    UART_HandleTypeDef *handle = &uart_desc->phy->handle;

    d_memcpy(&handle->Init, config, sizeof(handle->Init));
    if(HAL_UART_DeInit(handle) != HAL_OK) {
        return -1;
    }
    if(HAL_UART_Init(handle) != HAL_OK) {
        return -1;
    }
    uart_desc->phy->dma_init(uart_desc);
    return 0;
}

static void hal_tty_attach_callbacks (uart_hal_t *uart_desc)
{
    uart_desc->tty.tx_start = hal_tty_tx_start_hw;
    uart_desc->tty.rx_poll = hal_tty_rx_poll;
    uart_desc->tty.inout_hook = bsp_inout_forward;
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    uart_hal_t *uart_desc = hal_tty_get_desc_4_phy(huart->Instance);

    if (uart_desc && uart_desc->phy->msp_init) {
        uart_desc->phy->msp_init(uart_desc);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    uart_hal_t *uart_desc = hal_tty_get_desc_4_phy(huart->Instance);

    if (uart_desc && uart_desc->phy->msp_init) {
        uart_desc->phy->msp_deinit(uart_desc);
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *UartHandle)
{
    uart_hal_t *uart_desc = hal_tty_get_desc_4_phy(UartHandle->Instance);
    if (!uart_desc) {
        return;
    }
    uart_desc->phy->tx_dma_ready = SET;
    if (uart_desc->tty.txbuf_pending) {
        uart_desc->tty.txbuf_pending = NULL;
    }
}

void HAL_UART_RxHalfCpltCallback (UART_HandleTypeDef* huart)
{
    hal_tty_rx_cplt_any(0);
}

void HAL_UART_RxCpltCallback (UART_HandleTypeDef* huart)
{
    uart_hal_t *hal = hal_tty_get_desc_4_phy(huart->Instance);
    hal_tty_rx_cplt_any(1);
#if defined(STM32H745xx) || defined(STM32H747xx)
    if(HAL_UART_Receive_IT(huart, (uint8_t *)&hal->tty.rxbuf.dmabuf[1], 1) != HAL_OK)
    {
        for (;;) {}
    }
#endif
}

static uart_hal_t *hal_tty_get_desc_4_phy (void *source)
{
    int i;
    uart_hal_t *uart_desc;

    for (i = 0; i < hal_usrt_pool.cnt; i++) {
        uart_desc = hal_usrt_pool.pool[i];
        if (uart_desc->phy->handle.Instance == source) {
            return uart_desc;
        }
    }
    return NULL;
}

static int hal_tty_atach (uart_hal_t *uart_desc, const msp_func_tbl_t *msp)
{
    int err = -1, i;

    if (hal_usrt_pool.cnt >= arrlen(hal_usrt_pool.pool)) {
        return err;
    }
    hal_usrt_pool.pool[hal_usrt_pool.cnt++] = uart_desc;

    _serial_tty_preinit(&uart_desc->tty);

    hal_tty_uart_preinit(uart_desc, msp);
    if (uart_desc->phy) {
        err = hal_tty_if_init(uart_desc, &uart_115200_8n1_txrx);
    }
    if (err < 0) {
        return err;
    }
    hal_tty_attach_callbacks(uart_desc);

    uart_desc->tty.tx_bufsize = 512;
    uart_desc->tty.initialized = SET;
    uart_desc->type     = SERIAL_DEBUG;

    return 0;
}


/* Public API */

int hal_tty_vcom_attach (void)
{
    uart_hal_t *hal;
    hal = (uart_hal_t *)heap_calloc(sizeof(uart_hal_t) + sizeof(uart_phy_t));
    if (!hal) {
        return -1;
    }
    uart_vcom_desc = hal;
    uart_vcom_desc->phy = (uart_phy_t *)(hal + 1);
    return hal_tty_atach(hal, uart1_msp_func);
}

serial_tty_t *hal_tty_get_vcom_port (void)
{
    int i;
    for (i = 0; i < hal_usrt_pool.cnt; i++) {
        if (hal_usrt_pool.pool[i]->tty.initialized && (hal_usrt_pool.pool[i]->type == SERIAL_DEBUG)) {
            return &hal_usrt_pool.pool[i]->tty;
        }
    }
    return NULL;
}

void hal_tty_flush_any (void)
{
    int i = 0;

    for (i = 0; i < hal_usrt_pool.cnt; i++) {
        hal_tty_flush(hal_usrt_pool.pool[i]);
    }
}

/* Interrupt handlers */

void TTY_DMA_TX_IRQHandler (void)
{
    dma_tx_handle_irq(uart1_tx_dma_conf.inst);
}

void TTY_DMA_RX_IRQHandler (void)
{
    dma_rx_handle_irq(uart1_rx_dma_conf.inst);
}

void USART1_IRQHandler (void)
{
    hal_tty_irq_handler(USART1);
}

