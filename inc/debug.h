#ifndef _SERIAL_DEBUG_H_
#define _SERIAL_DEBUG_H_

#include <stddef.h>
#include <stdarg.h>
#include <dev_conf.h>
#include <stdint.h>
#include <bsp_api.h>
#ifndef DEBUG_SERIAL
#warning "DEBUG_SERIAL undefined, using TRUE"
#define DEBUG_SERIAL 1
#endif

#if DEBUG_SERIAL
#define PRINTF_SERIAL  1
#else
#define PRINTF_SERIAL  0
#endif

#define __func__ __FUNCTION__
#define PRINTF __attribute__((format(printf, 1, 2)))
#ifndef PRINTF_ATTR
#define PRINTF_ATTR(a, b) __attribute__((format(printf, a, b)))
#endif

#if DEBUG_SERIAL

#ifndef SERIAL_TSF
#warning "SERIAL_TSF undefined, using TRUE"
#define SERIAL_TSF 1
#endif

typedef int (*serial_rx_clbk_t) (const char *buf, int len);

typedef struct bsp_debug_api_s {
    bspdev_t dev;
    void (*putc) (char);
    char (*getc) (void);
    void (*send) (const void *, unsigned int);
    void (*flush) (void);
    void (*reg_clbk) (void *);
    void (*unreg_clbk) (void *);
    void (*tickle) (void);
    void (*dprintf) (const char *, ...);
} bsp_debug_api_t;

#define BSP_DBG_API(func) ((bsp_debug_api_t *)(g_bspapi->dbg))->func

#if BSP_INDIR_API

#define serial_init             BSP_DBG_API(dev.init)
#define serial_deinit          BSP_DBG_API(dev.deinit)
#define serial_conf            BSP_DBG_API(dev.conf)
#define serial_info            BSP_DBG_API(dev.info)
#define serial_priv            BSP_DBG_API(dev.priv)
#define serial_putc             BSP_DBG_API(putc)
#define serial_getc             BSP_DBG_API(getc)
#define serial_send_buf         BSP_DBG_API(send)
#define serial_flush            BSP_DBG_API(flush)
#define debug_add_rx_handler   BSP_DBG_API(reg_clbk)
#define debug_rm_rx_handler    BSP_DBG_API(unreg_clbk)
#define serial_tickle           BSP_DBG_API(tickle)
#define dprintf                 BSP_DBG_API(dprintf)

#else
void serial_init (void);
void serial_putc (char c);
char serial_getc (void);
void serial_send_buf (const void *data, size_t cnt);
void serial_flush (void);
void debug_add_rx_handler (serial_rx_clbk_t);
void debug_rm_rx_handler (serial_rx_clbk_t);
void serial_tickle (void);
void dprintf (const char *fmt, ...) PRINTF;

#endif

#else /*DEBUG_SERIAL*/

static inline void serial_init (void) {}
static inline void serial_putc (char c) {}
static inline char serial_getc (void) {return 0;}
static inline void serial_send_buf (const void *data, size_t cnt){}
static inline void serial_flush (void){}

static inline void dprintf (const char *fmt, ...){}
static inline void dvprintf (const char *fmt, va_list argptr) {}
void hexdump (const uint8_t *data, int len, int rowlength) {}

#endif /*DEBUG_SERIAL*/

void dvprintf (const char *fmt, va_list argptr);
void hexdump (const uint8_t *data, int len, int rowlength);

#endif /*_SERIAL_DEBUG_H_*/
