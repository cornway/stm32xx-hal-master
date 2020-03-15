#ifndef __UART_H__
#define __UART_H__

#include <config.h>

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

int uart_hal_tty_init (void);

#endif /*__UART_H__*/
