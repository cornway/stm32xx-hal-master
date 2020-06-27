#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


#if defined(USE_STM32H747I_DISCO)
#include <stm32h747i_discovery_lcd.h>
#include "../Utilities/Basic_GUI/basic_gui.h"
#elif defined(USE_STM32H745I_DISCO)
#include <stm32h745i_discovery_lcd.h>
#elif defined(USE_STM32F769I_DISCO)
#include <stm32f769i_discovery_lcd.h>
#else
#error
#endif

#include <arch.h>
#include <bsp_api.h>
#include <misc_utils.h>
#include <debug.h>
#include <heap.h>
#include <gui.h>
#include "bsp_cmd.h"

#if defined(USE_STM32H747I_DISCO) || defined(USE_STM32H745I_DISCO)

static int32_t LCD_GetXSize(uint32_t Instance, uint32_t *XSize)
{
    BSP_LCD_GetXSize(0, XSize);
    return BSP_ERROR_NONE;
}

static int32_t LCD_GetYSize(uint32_t Instance, uint32_t *YSize)
{
    BSP_LCD_GetYSize(0, YSize);
    return BSP_ERROR_NONE;
}

static const GUI_Drv_t LCD_GUI_Driver =
{
    BSP_LCD_DrawBitmap,
    BSP_LCD_FillRGBRect,
    BSP_LCD_DrawHLine,
    BSP_LCD_DrawVLine,
    BSP_LCD_FillRect,
    BSP_LCD_ReadPixel,
    BSP_LCD_WritePixel,
    LCD_GetXSize,
    LCD_GetYSize,
    BSP_LCD_SetActiveLayer,
    BSP_LCD_GetPixelFormat
};

#else /* defined(USE_STM32H747I_DISCO) || defined(USE_STM32H745I_DISCO) */

#define GUI_SetFuncDriver

#define GUI_GetFont             BSP_LCD_GetFont
#define GUI_SetFont             BSP_LCD_SetFont
#define GUI_DisplayStringAt     BSP_LCD_DisplayStringAt
#define GUI_FillRect(x, y, w, h, color) \
do {                                    \
    BSP_LCD_SetTextColor(color);        \
    BSP_LCD_FillRect(x, y, w, h);       \
} while (0);

#endif /* defined(USE_STM32H747I_DISCO) || defined(USE_STM32H745I_DISCO) */

/*HAL api*/
/*=======================================================================================*/

static const sFONT *fontTable[] =
{
    &Font8, &Font12, &Font16, &Font20, &Font24
};

static void __GUI_FillRect (dim_t *dest, dim_t *rect, rgba_t color)
{
    dim_t d = *rect;
    dim_place(&d, dest);
    GUI_FillRect(d.x, d.y, d.w, d.h, color);
}

static void __GUI_FillComponent (component_t *com, rgba_t color)
{
    GUI_FillRect(com->dim.x, com->dim.y, com->dim.w, com->dim.h, color);
}

static int __GUI_DisplayStringAt (component_t *com, int line, rgba_t textcolor, const char *str, int txtmode)
{
    int ret = -1;
    void *font = GUI_GetFont();

    if (font != com->font) {
        GUI_SetFont((sFONT *)com->font);
    }
    GUI_SetTextColor(textcolor);
    GUI_SetBackColor(com->bcolor);
    ret = GUI_DisplayStringAt(com->dim.x, com->dim.y + LINE(line) + (LINE(0) / 2),
                                  com->dim.w, com->dim.h, (uint8_t *)str, (Text_AlignModeTypdef)txtmode);
    if (font != com->font) {
        GUI_SetFont((sFONT *)font);
    }
    return ret;
}

static void __GUI_FontProperties (fontprop_t *prop, const void *_font)
{
    const sFONT *font = (const sFONT *)_font;

    prop->w = font->Width;
    prop->h = font->Height;
}

static const void *__GUI_Font_4_Size (gui_t *gui, int size)
{
    const void *best = NULL;
    int err, besterr = size, i;

    for (i = 0; i < arrlen(fontTable); i++) {
        err = size - fontTable[i]->Height;
        if (err == 0) {
            best = fontTable[i];
            break;
        }
        if (err < 0) {
            err = -err;
        }
        if (besterr > err) {
            besterr = err;
            best = fontTable[i];
        }
    }
    return best;
}

void gui_draw_attach (gui_bsp_api_t *api)
{
    GUI_SetFuncDriver(&LCD_GUI_Driver);

    api->fill_rect = __GUI_FillRect;
    api->fill_comp = __GUI_FillComponent;
    api->string_at = __GUI_DisplayStringAt;
    api->font_prop = __GUI_FontProperties;
    api->get_font  = __GUI_Font_4_Size;
}

/*=======================================================================================*/


