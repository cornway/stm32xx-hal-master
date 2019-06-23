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

#if BSP_INDIR_API

#define serial_init   g_bspapi->dbg.init
#define serial_putc   g_bspapi->dbg.putc
#define serial_getc   g_bspapi->dbg.getc
#define serial_send_buf   g_bspapi->dbg.send
#define serial_flush   g_bspapi->dbg.flush
#define term_register_handler   g_bspapi->dbg.reg_clbk
#define term_unregister_handler   g_bspapi->dbg.unreg_clbk
#define serial_tickle   g_bspapi->dbg.tickle
#define dprintf g_bspapi->dbg.dprintf

#else
void serial_init (void);
void serial_putc (char c);
char serial_getc (void);
void serial_send_buf (const void *data, size_t cnt);
void serial_flush (void);
void term_register_handler (serial_rx_clbk_t);
void term_unregister_handler (serial_rx_clbk_t);
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
