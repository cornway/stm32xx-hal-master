//******************************************************************************
// Hosting of stdio functionality through ITM/SWV
//******************************************************************************
#ifdef HAVE_SEMIHOSTING
#include <stdio.h>
#include <rt_misc.h>
#include "../MDK-ARM/RTE/_STM32F769I_Discovery/RTE_Components.h"

#if defined(RTE_Compiler_IO_STDERR) || \
    defined(RTE_Compiler_IO_STDOUT) || \
    defined(RTE_Compiler_IO_STDIN) || \
    defined(RTE_Compiler_IO_TTY)
#define DEFINE_USER_IO 0
#else
#define DEFINE_USER_IO 1
#endif

#if DEFINE_USER_IO

#include "stm32f7xx.h"
#include "debug.h"

#ifndef PRINTF_NO_RETARGET
#define PRINTF_NO_RETARGET 1
#endif

#pragma import(__use_no_semihosting_swi)

struct __FILE { int handle; /* Add whatever you need here */ };
FILE __stdout;
FILE __stdin;

int fputc(int ch, FILE *f)
{
#if PRINTF_NO_RETARGET
  serial_putc(ch);
#else
  ITM_SendChar(ch);
#endif
  return(ch);
}

int fgetc(FILE *f)
{
  char ch = 0;
#if PRINTF_NO_RETARGET
  ch = serial_getc();
#endif
  return((int)ch);
}

int ferror(FILE *f)
{
  /* Your implementation of ferror */
  return EOF;
}

void _ttywrch(int ch)
{
#if PRINTF_NO_RETARGET
  serial_putc(ch);
#else
  ITM_SendChar(ch);
#endif
}

void _sys_exit(int return_code)
{
label:  goto label;  /* endless loop */
}

#endif /*HAVE_SEMIHOSTING*/

#endif /*DEFINE_USER_IO*/

//******************************************************************************

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

