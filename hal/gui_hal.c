
#if defined(USE_STM32H747I_DISCO)
#include <stm32h747i_discovery_lcd.h>
#elif defined(USE_STM32H745I_DISCO)
#include <stm32h745i_discovery_lcd.h>
#elif defined(USE_STM32F769I_DISCO)
#include <stm32f769i_discovery_lcd.h>
#else
#error
#endif

#include <gui.h>

/*HAL api*/
/*=======================================================================================*/

void gui_rect_fill_HAL (dim_t *dest, dim_t *rect, rgba_t color)
{
    dim_t d = *rect;
    dim_place(&d, dest);
#if defined(USE_STM32F769I_DISCO)
    BSP_LCD_SetTextColor(color);
    BSP_LCD_FillRect(d.x, d.y, d.w, d.h);
#elif defined(USE_STM32H745I_DISCO) || defined(USE_STM32H747I_DISCO)
    BSP_LCD_FillRect(0, d.x, d.y, d.w, d.h, color);
#else
#error
#endif
    
}

void gui_com_fill_HAL (component_t *com, rgba_t color)
{
#if defined(USE_STM32F769I_DISCO)
    BSP_LCD_SetTextColor(color);
    BSP_LCD_FillRect(com->dim.x, com->dim.y, com->dim.w, com->dim.h);
#elif defined(USE_STM32H745I_DISCO) || defined(USE_STM32H747I_DISCO)
    BSP_LCD_FillRect(0, com->dim.x, com->dim.y, com->dim.w, com->dim.h, color);
#else
#error
#endif
}

int gui_draw_string_HAL (component_t *com, int line,
                              rgba_t textcolor, const char *str, int txtmode)
{
#if defined(USE_STM32F669I_DISCO)
    void *font = BSP_LCD_GetFont();
    int ret;
    dim_t dim;

    d_memcpy(&dim, &com->dim, sizeof(dim));

    if (font != com->font) {
        BSP_LCD_SetFont((sFONT *)com->font);
    }
    BSP_LCD_SetTextColor(textcolor);
    ret = BSP_LCD_DisplayStringAt(dim.x, dim.y + LINE(line) + (LINE(0) / 2),
                                  dim.w, dim.h, (uint8_t *)str, (Text_AlignModeTypdef)txtmode);
    if (font != com->font) {
        BSP_LCD_SetFont(font);
    }
    return ret;
#endif
}

void gui_get_font_prop_HAL (fontprop_t *prop, const void *_font)
{
#if defined(USE_STM32F669I_DISCO)
    const sFONT *font = (const sFONT *)_font;

    prop->w = font->Width;
    prop->h = font->Height;
#endif
}

const void *
gui_get_font_4_size_HAL (gui_t *gui, int size, int bestmatch)
{
#if defined(USE_STM32F669I_DISCO)
    int err, besterr = size, i;
    const void *best = NULL;

    static const sFONT *fonttbl[] =
        {&Font8, &Font12, &Font16, &Font20, &Font24};

    for (i = 0; i < arrlen(fonttbl); i++) {
        err = size - fonttbl[i]->Height;
        if (err == 0) {
            best = fonttbl[i];
            break;
        }
        if (bestmatch) {
            if (err < 0) {
                err = -err;
            }
            if (besterr > err) {
                besterr = err;
                best = fonttbl[i];
            }
        }
    }
    return best;
#endif
}

/*=======================================================================================*/


