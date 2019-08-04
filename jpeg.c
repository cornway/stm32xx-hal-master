
#include "int/lcd_int.h"

#include <jpeg.h>
#include <lcd_main.h>
#include <misc_utils.h>
#include <dev_io.h>
#include <heap.h>

uint32_t JpegProcessing_End = 0;

int jpeg_init (const char *conf)
{
    HAL_JPEG_UserInit();
}

void *jpeg_cache (const char *path, uint32_t *size)
{
    int f;
    void *p;

    *size = d_open(path, &f, "r");
    if (f < 0) {
        return NULL;
    }

    p = heap_alloc_shared(*size);
    if (!p) {
        d_close(f);
        return NULL;
    }
    d_read(f, p, *size);
    d_close(f);
    return p;
}

void jpeg_release (void *p)
{
    heap_free(p);
}

int jpeg_decode (jpeg_info_t *info, void *tempbuf, void *data, uint32_t size)
{
    int err;
    irqmask_t irq = ~0;

    err = JPEG_Decode_DMA(&JPEG_Handle, data, size, (uint32_t)tempbuf);

    if (err < 0) return -1;

    do
    {
      irq_save(&irq);
      JPEG_InputHandler(&JPEG_Handle);
      JpegProcessing_End = JPEG_OutputHandler(&JPEG_Handle);
      irq_restore(irq);
    } while(JpegProcessing_End == 0);
    
    JPEG_Info(info);
    return 0;
}


