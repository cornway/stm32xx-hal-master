#ifndef __HDMI_PUB_H__
#define __HDMI_PUB_H__

#if defined(USE_LCD_HDMI)

#include <stdint.h>

#define EDID_SEG_SIZE 0x100

#pragma anon_unions

typedef struct {
    union {
        uint8_t raw[EDID_SEG_SIZE];
    };
} hdmi_edid_seg_t;

typedef struct {
    int xres, yres;
    float rate_hz;
    float pclk_mhz;
    int interlaced;
    int hres, hstart, hend, htotal;
    int vres, vstart, vend, vtotal;
    char hpol, vpol;
} hdmi_timing_t;

int hdmi_parse_edid (hdmi_timing_t *timing, hdmi_edid_seg_t *edid, int size);

#endif /*defined(USE_LCD_HDMI)*/

#endif /*__HDMI_PUB_H__*/

