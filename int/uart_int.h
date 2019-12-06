#ifndef __UART_H__
#define __UART_H__

#include <dev_conf.h>

#ifndef DEBUG_SERIAL_USE_DMA
#define DEBUG_SERIAL_USE_DMA 0
#endif

#ifndef DEBUG_SERIAL_BUFERIZED
#define DEBUG_SERIAL_BUFERIZED 0
#endif

#ifndef DEBUG_SERIAL_USE_RX
#define DEBUG_SERIAL_USE_RX 0
#endif

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
    irqn_t                  irq_txdma, irq_uart;
#if DEBUG_SERIAL_USE_RX
    DMA_HandleTypeDef       hdma_rx;
    irqn_t                  irq_rxdma;
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

#define DMA_RX_SIZE (1 << 1)
#define DMA_RX_FIFO_SIZE (1 << 8)

typedef struct {
    uint16_t fifoidx;
    uint16_t fifordidx;
    uint16_t eof;
    char dmabuf[DMA_RX_SIZE];
    char fifo[DMA_RX_FIFO_SIZE];
} rxstream_t;

#endif /*DEBUG_SERIAL_BUFERIZED*/

uart_desc_t *uart_find_desc (void *source);

int serial_init (void);

#endif /*__UART_H__*/
