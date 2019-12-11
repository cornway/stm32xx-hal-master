#ifndef __UART_H__
#define __UART_H__

#include <dev_conf.h>

#ifndef SERIAL_TTY_HAS_DMA
#define SERIAL_TTY_HAS_DMA 0
#endif

#ifndef DEBUG_SERIAL_USE_RX
#define DEBUG_SERIAL_USE_RX (SERIAL_TTY_HAS_DMA)
#endif

#if SERIAL_TTY_HAS_DMA
#define SERIAL_TX_BUFFERIZED 1
#else
#define SERIAL_TX_BUFFERIZED 0
#endif /*SERIAL_TTY_HAS_DMA*/

typedef enum {
    SERIAL_DEBUG,
    SERIAL_REGULAR,
} serial_type_t;

#define DMA_RX_SIZE (1 << 1)
#define DMA_RX_FIFO_SIZE (1 << 8)

typedef struct {
    uint16_t fifoidx;
    uint16_t fifordidx;
    uint16_t eof;
    char dmabuf[DMA_RX_SIZE];
    char fifo[DMA_RX_FIFO_SIZE];
} rxstream_t;

typedef struct uart_desc_s uart_desc_t;

typedef void (*hal_msp_init_t) (uart_desc_t *uart_desc);

struct uart_desc_s {
    USART_TypeDef           *hw;
    UART_HandleTypeDef      handle;
    UART_InitTypeDef        const * ini;
    hal_msp_init_t          msp_init;
    hal_msp_init_t          msp_deinit;
    irqmask_t               uart_irq_mask;
    hal_msp_init_t          dma_init;
    hal_msp_init_t          dma_deinit;
    irqn_t                  irq_uart;
    serial_type_t           type;
    int                     tx_id;
    FlagStatus              initialized;
    FlagStatus              tx_allowed;
#if SERIAL_TTY_HAS_DMA
    DMA_HandleTypeDef       hdma_tx;
    irqn_t                  irq_txdma;
#endif
#if DEBUG_SERIAL_USE_RX
    DMA_HandleTypeDef       hdma_rx;
    irqn_t                  irq_rxdma;
    void                    (*rx_handler) (DMA_HandleTypeDef *);
#endif
};

uart_desc_t *uart_find_desc (void *source);

int serial_init (void);

#endif /*__UART_H__*/
