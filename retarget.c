//******************************************************************************
// Hosting of stdio functionality through ITM/SWV
//******************************************************************************

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

#pragma import(__use_no_semihosting_swi)

struct __FILE { int handle; /* Add whatever you need here */ };
FILE __stdout;
FILE __stdin;

int fputc(int ch, FILE *f)
{
  ITM_SendChar(ch);

  return(ch);
}

int fgetc(FILE *f)
{
  char ch = 0;

  return((int)ch);
}

int ferror(FILE *f)
{
  /* Your implementation of ferror */
  return EOF;
}

void _ttywrch(int ch)
{
  ITM_SendChar(ch);
}

void _sys_exit(int return_code)
{
label:  goto label;  /* endless loop */
}

#endif /*DEFINE_USER_IO*/

//******************************************************************************

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

