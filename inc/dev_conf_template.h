#ifndef __DEV_CONF_H__
#define __DEV_CONF_H__

#define NVIC_IRQ_MAX                (32)

#define DEBUG_SERIAL                (1)
#define DEBUG_SERIAL_USE_DMA        (1)
#define MAX_UARTS                    (1)
#define DEBUG_SERIAL_BUFERIZED      (1)
#define SERIAL_TSF                   (1)

#define AUDIO_MODULE_PRESENT        (1)
#define MUSIC_MODULE_PRESENT        (1)

#define GFX_COLOR_MODE GFX_COLOR_MODE_CLUT

#define HEAP_CACHE_SIZE             (0)

#define AUDIO_SAMPLE_RATE           (22050U)

#define SDRAM_VOL_START             (0xC0000000)
#define SDRAM_VOL_END               (0xC1000000)
#define HEAP_MALLOC_MARGIN          (0x1000)

#define DEVIO_READONLY              (0)
#define MAX_HANDLES                 (3)

#endif /*__DEV_CONF_H__*/
