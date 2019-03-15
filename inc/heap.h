
#ifdef DATA_IN_ExtSDRAM

#define ALIGN(x) __attribute__((aligned(x)))

#define CACHE_STATIC 1

#define SDRAM __attribute__ ((section ("cache")))
#define DTCM __attribute__ ((section ("dtcm")))
#define IRAM __attribute__ ((section ("iram")))
#define IRAM2 __attribute__ ((section ("iram2")))

#else

#define CACHE_STATIC 0

#define SDRAM
#define DTCM
#define IRAM
#define IRAM2

#endif

