#include <stdint.h>

#define ALIGN(x) __attribute__((aligned(x)))

#define SDRAM __attribute__ ((section ("dram")))
#define DTCM __attribute__ ((section ("dtcm")))
#define IRAM __attribute__ ((section ("iram")))
#define IRAM2 __attribute__ ((section ("iram2")))


void Sys_AllocInit (void);
void *Sys_AllocShared (int *size);
void *Sys_AllocVideo (int *size);
int Sys_AllocBytesLeft (void);
void *Sys_Malloc (int size);
void *Sys_Realloc (void *x, int32_t size);
void *Sys_Calloc (int32_t size);
void Sys_Free (void *p);


