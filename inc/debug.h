#ifndef _SERIAL_DEBUG_H_
#define _SERIAL_DEBUG_H_


#define PRINTF_SERIAL  1
#define DEBUG_SERIAL 1

#include "stdarg.h"

#if DEBUG_SERIAL

void serial_irq_save (int *irq);
void serial_irq_restore (int irq);

void serial_init (void);
void serial_putc (char c);
char serial_getc (void);
void serial_send_buf (void *data, size_t cnt);

void dprintf (char *fmt, ...);
void dvprintf (char *fmt, va_list argptr);

#else /*DEBUG_SERIAL*/

static inline void serial_irq_save (int *irq) {*irq = 0;}
static inline void serial_irq_restore (int irq) {}

static inline void serial_init (void) {}
static inline void serial_putc (char c) {}
static inline char serial_getc (void){return 0;}
static inline void serial_send_buf (void *data, size_t cnt){}

static inline void dprintf (char *fmt, ...){}
static inline void dvprintf (char *fmt, va_list argptr) {}

#endif /*DEBUG_SERIAL*/

#endif /*_SERIAL_DEBUG_H_*/
