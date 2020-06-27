#ifndef __UART_H__
#define __UART_H__

typedef enum {
    SERIAL_DEBUG,
    SERIAL_REGULAR,
} serial_type_t;

typedef struct uart_hal_s uart_hal_t;
typedef struct uart_phy_s uart_phy_t;

typedef void (*hal_msp_init_t) (uart_hal_t *);

struct uart_phy_s {
    hal_msp_init_t          msp_init;
    hal_msp_init_t          msp_deinit;
    irqmask_t               uart_irq_mask;
    irqn_t                  irq_uart;

    hal_msp_init_t          dma_init;
    hal_msp_init_t          dma_deinit;
    DMA_HandleTypeDef       hdma_tx;
    irqn_t                  irq_txdma;
    DMA_HandleTypeDef       hdma_rx;
    irqn_t                  irq_rxdma;
    unsigned int            tx_dma_ready;

    UART_HandleTypeDef      handle;
};

struct uart_hal_s {
    serial_tty_t            tty;
    serial_type_t           type;
    uart_phy_t              *phy;
};

int serial_tty_tx_flush_proc (serial_tty_t *tty, d_bool force);

extern void USART1_IRQHandler (void);

#endif /*__UART_H__*/
