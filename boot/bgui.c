#include <stm32f769i_discovery_lcd.h>
#include <gui.h>
#include <misc_utils.h>
#include <lcd_main.h>
#include <dev_io.h>
#include <heap.h>
#include <bsp_cmd.h>

#define gui_error(fmt, args ...) \
    if (gui->dbglvl >= DBG_ERR) {dprintf("%s() [fatal] : "fmt, __func__, args);}

#define gui_warn(args ...) \
    if (gui->dbglvl >= DBG_WARN) {dprintf(args);}

#define gui_info(fmt, args ...) \
    if (gui->dbglvl >= DBG_INFO) {dprintf("%s() : "fmt, __func__, args);}


#define G_TEXT_MAX 256

d_bool dim_check (const dim_t *d, const point_t *p)
{
    int x0 = p->x - d->x;
    if (x0 < 0) {
        return d_false;
    }
    if (x0 > d->w) {
        return d_false;
    }
    int y0 = p->y - d->y;
    if (y0 < 0) {
        return d_false;
    }
    if (y0 > d->h) {
        return d_false;
    }
    return d_true;
}

static d_bool dim_check_invis (const dim_t *d, const dim_t *mask)
{
    if (mask->x > d->x) {
        return d_false;
    }
    if (mask->w + mask->x < d->w + d->x) {
        return d_false;
    }
    if (mask->y > d->y) {
        return d_false;
    }
    if (mask->h + mask->y < d->h + d->y) {
        return d_false;
    }
    return d_true;
}

static void dim_trunc (dim_t *d, const dim_t *s)
{
    if (d->x > s->w) {
          d->x = s->w;
    }
    if (d->x + d->w > s->w) {
          d->w = s->w - d->x;
    }
    if (d->y > s->h) {
          d->y = s->h;
    }
    if (d->y + d->h > s->h) {
          d->h = s->h - d->y;
    }
}

void dim_place (dim_t *d, const dim_t *s)
{
    dim_t tmp = {0, 0, s->w, s->h};
    dim_trunc(d, &tmp);
    d->x = d->x + s->x;
    d->y = d->y + s->y;
}

void dim_tolocal (dim_t *d, const dim_t *s)
{
    d->x = d->x - s->x;
    d->y = d->y - s->y;
    dim_trunc(d, s);
}

void dim_tolocal_p (point_t *d, const dim_t *s)
{
    d->x = d->x - s->x;
    d->y = d->y - s->y;
}

void dim_get_origin (point_t *d, const dim_t *s)
{
    d->x = s->x + (s->w / 2);
    d->y = s->y + (s->h / 2);
}

void dim_set_origin (dim_t *d, const point_t *s)
{
    d->x = s->x - d->w / 2;
    d->y = s->y - d->h / 2;
}

void dim_set_right (dim_t *d, dim_t *s)
{
    int x = s->x - d->w;
    int y = s->y + (s->h / 2);

    d->x = x;
    d->y = y - (d->h / 2);
}

void dim_set_left (dim_t *d, dim_t *s)
{
    int x = s->x + s->w;
    int y = s->y + (s->h / 2);

    d->x = x;
    d->y = y - (d->h / 2);
}

void dim_set_top (dim_t *d, dim_t *s)
{
    int x = s->x + (s->w / 2);
    int y = s->y + s->h;

    d->x = x - (d->w / 2);
    d->y = y;
}

void dim_set_bottom (dim_t *d, dim_t *s)
{
    int x = s->x + (s->w / 2);
    int y = s->y - d->h;

    d->x = x - (d->w / 2);
    d->y = y;
}

static component_t *gui_iterate_all (gui_t *gui, void *user, int (*h) (component_t *com, void *user))
{
    if (h == NULL) {
        return NULL;
    }
    pane_t *pane = gui->head;
    component_t *com;

    if (!pane) {
        return NULL;
    }
    while (pane) {
        com = pane->head;
        while (com) {
            if (h(com, user) > 0) {
                return com;
            }
            com = com->next;
        }
        pane = pane->next;
    }
    return NULL;
}

void gui_init (gui_t *gui, const char *name, uint8_t framerate,
                dim_t *dim, gui_bsp_api_t *bspapi)
{
    char temp[GUI_MAX_NAME];

    d_memset(gui, 0, sizeof(*gui));

    gui->font = gui_get_font_4_size(gui, 20, 1);

    gui->dim.x = dim->x;
    gui->dim.y = dim->y;
    gui->dim.w = dim->w;
    gui->dim.h = dim->h;
    gui->framerate = framerate;
    gui->dbglvl = DBG_WARN;
    snprintf(gui->name, sizeof(gui->name), "%s", name);

    snprintf(temp, sizeof(temp), "%s_%s", name, "fps");
    cmd_register_i32(&gui->framerate, temp);
    
    snprintf(temp, sizeof(temp), "%s_%s", name, "dbglvl");
    cmd_register_i32(&gui->dbglvl, temp);

    d_memcpy(&gui->bspapi, bspapi, sizeof(gui->bspapi));

    if (0) {
        int err;
        arch_word_t argv[3] =
        {
            (arch_word_t)heap_alloc_shared,
            0, 0
        };
        err = vid_priv_ctl(VCTL_VRAM_ALLOC, argv);
        if (err < 0) {
            return;
        }
        gui->directmem = (void *)argv[1];
    }
}

void gui_destroy (gui_t *gui)
{
    pane_t *pane = gui->head;
    component_t *com;

    while (pane) {
        com = pane->head;
        while (com) {
            heap_free(com);
            com = com->next;
        }
        heap_free(pane);
        pane = pane->next;
    }
    gui->destroy = 0;
}

void gui_get_font_prop (fontprop_t *prop, const void *_font)
{
    const sFONT *font = (const sFONT *)_font;

    prop->w = font->Width;
    prop->h = font->Height;
}

const void *gui_get_font_4_size (gui_t *gui, int size, int bestmatch)
{
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
}

pane_t *gui_get_pane (gui_t *gui, const char *name, int extra)
{
    int namelen = strlen(name);
    int allocsize;
    pane_t *pane = NULL;

    allocsize = sizeof(*pane) + namelen + 1;
    if (extra > 0) {
        allocsize += extra;
    }
    allocsize = ROUND_UP(allocsize, 4);
    if (gui_bsp_alloc(gui)) {
        pane = gui_bsp_alloc(gui)(allocsize);
    }
    assert(pane);
    memset(pane, 0, allocsize);
    snprintf(pane->name, namelen + 1, "%s", name);
    return pane;
}

void __gui_set_panexy (gui_t *gui, pane_t *pane, int x, int y, int w, int h)
{
    pane->next = NULL;
    if (gui->head == NULL) {
        gui->head = pane;
        gui->tail = pane;
    } else {
        pane->next = gui->head;
        gui->head = pane;
    }
    pane->dim.x = x;
    pane->dim.y = y;
    pane->dim.w = w;
    pane->dim.h = h;
    pane->parent = gui;
    pane->selectable = 1;
}


void gui_set_pane (gui_t *gui, pane_t *pane)
{
    __gui_set_panexy(gui, pane, gui->dim.x, gui->dim.y, gui->dim.w, gui->dim.h);
}

void gui_set_panexy (gui_t *gui, pane_t *pane, int x, int y, int w, int h)
{
    __gui_set_panexy(gui, pane, x, y, w, h);
    dim_place(&pane->dim, &gui->dim);
}

static void __gui_set_comp_def_sfx (component_t *com)
{
    gui_sfx_t *sfx = &com->sfx;

    sfx->sfx_cantdothis = -1;
    sfx->sfx_close = -1;
    sfx->sfx_open = -1;
    sfx->sfx_other = -1;
}

component_t *gui_get_comp (gui_t *gui, const char *name, const char *text)
{
    int namelen = strlen(name);
    int textlen = text ? strlen(text) : G_TEXT_MAX;
    int allocsize;
    component_t *com;

    allocsize = sizeof(*com) + namelen + 1 + textlen + 1;
    allocsize = ROUND_UP(allocsize, sizeof(uint32_t));
    if (gui_bsp_alloc(gui)) {
        com = gui_bsp_alloc(gui)(allocsize);
    }
    assert(com);
    d_memset(com, 0, allocsize);
    assert(name);
    snprintf(com->name, sizeof(com->name), "%s", name);
    com->text_size = textlen + 1;
    if (text && text[0]) {
        gui_print(com, "%s", text);
    }
    __gui_set_comp_def_sfx(com);
    return com;

}

void gui_set_comp (pane_t *pane, component_t *c, int x, int y, int w, int h)
{
    c->next = NULL;
    if (pane->head == NULL) {
        pane->head = c;
        pane->tail = c;
    } else {
        c->next = pane->head;
        pane->head = c;
        
    }
    c->dim.x = x;
    c->dim.y = y;
    c->dim.w = w;
    c->dim.h = h;
    c->parent = pane;
    c->font = pane->parent->font;
    dim_place(&c->dim, &pane->dim);
}

void gui_set_prop (component_t *c, prop_t *prop)
{
    c->fcolor = prop->fcolor;
    c->bcolor = prop->bcolor;
    c->ispad = prop->ispad;
    c->userdraw = prop->user_draw;
    c->sfx = prop->sfx;
    if (prop->fontprop.font) {
        c->font = prop->fontprop.font;
    }
}

static inline rgba_t
gui_com_select_color (component_t *com)
{
#define APPLY_GLOW(color, glow) \
    ((color) | (((glow) << 24) | ((glow) << 16) | ((glow) << 8) | ((glow) << 0)))

    rgba_t color = com->bcolor;

    if (com->glow && com == com->parent->onfocus) {
        color = APPLY_GLOW(color, com->glow);
    }
    return color;
#undef APPLY_GLOW
}

void gui_rect_fill (component_t *com, dim_t *rect, rgba_t color)
{
    dim_t d = *rect;
    dim_place(&d, &com->dim);
    BSP_LCD_SetTextColor(color);
    BSP_LCD_FillRect(d.x, d.y, d.w, d.h);
}

void
gui_com_fill (component_t *com, rgba_t color)
{
    BSP_LCD_SetTextColor(color);
    BSP_LCD_FillRect(com->dim.x, com->dim.y, com->dim.w, com->dim.h);
}

void gui_com_clear (component_t *com)
{
    gui_com_fill(com, com->bcolor);
}

static inline void
__gui_text (component_t *com, char *buf, int bufsize, const char *text, int x, int y)
{
    snprintf(buf, bufsize, "%s", text);
    com->repaint = 1;
}

void gui_text (component_t *com, const char *text, int x, int y)
{
    __gui_text(com, com->text, com->text_size, text, x, y);
}

static uint32_t __gui_printxy
                    (component_t *com, char *buf, int bufsize,
                    const char *fmt, va_list argptr)
{
    int n;
    n = vsnprintf (buf, bufsize, fmt, argptr);
    com->repaint = 1;
    return n;
}

void gui_printxy (component_t *com, int x, int y, const char *fmt, ...)
{
    va_list         argptr;

    va_start (argptr, fmt);
    com->text_index = __gui_printxy(com, com->text, com->text_size, fmt, argptr);
    va_end (argptr);
}

int gui_apendxy (component_t *com, int x, int y, const char *fmt, ...)
{
    va_list         argptr;
    uint16_t        offset = com->text_index - 1;

    if (com->text_index >= com->text_size - 1) {
        return -1;
    }

    va_start (argptr, fmt);
    com->text_index += __gui_printxy(com, com->text + offset,
                            com->text_size - com->text_index, fmt, argptr);
    va_end (argptr);
    return com->text_size - com->text_index;
}

static int __gui_draw_string (component_t *com, int line,
                              rgba_t textcolor, const char *str, Text_AlignModeTypdef txtmode)
{
    void *font = BSP_LCD_GetFont();
    int ret;

    if (font != (sFONT *)com->font) {
        BSP_LCD_SetFont((sFONT *)com->font);
    }
    BSP_LCD_SetTextColor(textcolor);
    ret = BSP_LCD_DisplayStringAt(com->dim.x, com->dim.y + LINE(line),
                                       com->dim.w, com->dim.h, (uint8_t *)str, txtmode);
    if (font != (sFONT *)com->font) {
        BSP_LCD_SetFont(font);
    }
    return ret;
}

int gui_draw_string (component_t *com, int line, rgba_t textcolor, const char *str)
{
    return __gui_draw_string(com, line, textcolor, str, LEFT_MODE);
}

int gui_draw_string_c (component_t *com, int line, rgba_t textcolor, const char *str)
{
    return __gui_draw_string(com, line, textcolor, str, CENTER_MODE);
}

static void gui_comp_draw (pane_t *pane, component_t *com)
{
    if (com->ispad) {
        gui_com_clear(com);
    } else if (!com->userdraw) {
        rgba_t color;
        int len = 0, tmp;
        int line = 0;
        uint8_t *text;

        color = gui_com_select_color(com);

        if (com->draw) {
            com->draw(pane, com, NULL);
        } else {
            gui_com_fill(com, color);
        }

        if (com->showname) {
            gui_draw_string_c(com, line, com->fcolor, com->name);
            line++;
        }
        len = com->text_index;
        text = (uint8_t *)com->text;
        tmp = len;
        while (len > 0 && tmp > 0) {
            tmp = gui_draw_string_c(com, line, com->fcolor, text);
            len = len - tmp;
            text += tmp;
            line++;
        }
    } else if (com->draw) {
        com->draw(pane, com, NULL);
    }
}

static void gui_pane_draw (gui_t *gui, pane_t *pane)
{
    component_t *com = pane->head;

    vid_vsync(0);
    while (com) {
        if (com->repaint) {
            gui_comp_draw(pane, com);
            gui->needsupdate++;
            com->repaint = 0;
        }
        com = com->next;
    }
    pane->repaint = 0;
}

static d_bool __gui_check_framerate (gui_t *gui)
{
    if (0 == d_rlimit_wrap(&gui->repainttsf, 1000 / gui->framerate)) {
        return d_false;
    }
    return d_true;
}

static void gui_hal_update (gui_t *gui)
{
    if (gui->needsupdate) {
        vid_vsync(1);
        if (gui->directmem) {
            int err;
            screen_t screen[] =
            {
                {NULL},
                {gui->directmem, gui->dim.x, gui->dim.y, gui->dim.w, gui->dim.h, GFX_COLOR_MODE_RGBA8888},
                {NULL},
            };
            err = vid_priv_ctl(VCTL_VRAM_COPY, &screen[1]);
            if (err < 0) {
                return;
            }
            vid_update(NULL);
            err = vid_priv_ctl(VCTL_VRAM_COPY, &screen[0]);
        } else {
            vid_update(NULL);
        }
        gui->needsupdate = 0;
    }
}

void gui_draw (gui_t *gui)
{
    pane_t *pane = gui->head;
    pane_t *selected = gui->selected;
    const dim_t *invis = NULL;

    if (!__gui_check_framerate(gui)) {
        return;
    }
    if (selected) {
        invis = &selected->dim;
    }
    while (pane) {
        if (pane != selected && pane->repaint) {
            if (!invis || !dim_check_invis(&pane->dim, invis)) {
                gui_pane_draw(gui, pane);
            }
        }
        pane = pane->next;
    }
    if (selected && selected->repaint) {
        gui_pane_draw(gui, selected);
    }
    //gui_hal_update(gui);
}

static void gui_act (gui_t *gui, component_t *com, gevt_t *evt)
{
    comp_handler_t h;
    pane_t *pane = com->parent;
    d_bool isrelease = d_false;

    if (pane != gui->selected) {
        return;
    }

    if (evt->e == GUIRELEASE) {
        isrelease = d_true;
    }

    if (isrelease) {
        h = com->release;
    } else {
        h = com->act;
    }

    if (pane->onfocus) {
        if (isrelease) {
            assert(com == pane->onfocus);
            pane->onfocus = NULL;
            com->focus = 0;
        }
    }
    if (h && h(pane, com, evt) > 0) {
        if (!isrelease) {
            pane->onfocus = com;
            com->focus = 1;
            com->glow = 127;
        }
    }
}

void gui_resp (gui_t *gui, component_t *com, gevt_t *evt)
{
    comp_handler_t h;
    d_bool isrelease = d_false;
    pane_t *pane = gui->selected;

    if (evt->p.x == 0 && evt->p.y == 0) {
        com = gui->selected->onfocus;
    }
    if (com) {
        gui_act(gui, com, evt);
    }

    vid_ptr_align(&evt->p.x, &evt->p.y);
    if (evt->e == GUIRELEASE) {
        isrelease = d_true;
    }
    if (pane) {
        point_t pp;

        pp = evt->p;
        dim_tolocal_p(&pp, &pane->dim);
        if (pane->onfocus) {
            com = pane->onfocus;
            if (isrelease) {
                pane->onfocus = NULL;
                com->focus = 0;
            }
        } else {
            com = pane->head;
        }
        while (com) {
            if (dim_check(&com->dim, &evt->p)) {
                if (isrelease) {
                    h = com->release;
                } else {
                    h = com->act;
                }
                if (h) {
                    gevt_t _evt = {pp, evt->e, evt->sym};

                    dim_tolocal_p(&_evt.p, &com->dim);
                    if (h(pane, com, &_evt) > 0) {
                        if (!isrelease) {
                            pane->onfocus = com;
                            com->focus = 1;
                            com->glow = 127;
                        }
                        break;
                    } else {
                        if (gui_bsp_sfxplay(gui)) {
                            gui_bsp_sfxplay(gui)(com->sfx.sfx_other, 100);
                        }
                    }
                }
            }
            com = com->next;
        }
        pane = pane->next;
    }
}

static int __namecomp (component_t *com, void *user)
{
    return !strcmp(com->name, user);
}

static component_t *gui_get_for_name (gui_t *gui, const char *name)
{
    component_t *com;

    com = gui_iterate_all(gui, (char *)name, __namecomp);
    return com;
}

static int __force_wakeup (component_t *com, void *user)
{
    com->repaint = 1;
    if (com->parent->parent->selected == com->parent) {
        com->parent->repaint = 1;
    }
    return 0;
}

static int __wakeup (gui_t *gui, component_t *com)
{
    com->repaint = 1;
    if (com->parent == gui->selected) {
        com->parent->repaint = 1;
    }
    return 0;
}

void gui_wakeup (gui_t *gui, const char *name)
{
    if (name == NULL) {
        gui_iterate_all(gui, NULL, __force_wakeup);
    } else {
        component_t *com = gui_get_for_name(gui, name);
        if (com) {
            __wakeup(gui, com);
        }
    }
}

void gui_wakeup_com (gui_t *gui, component_t *com)
{
    __wakeup(gui, com);
}

void gui_wakeup_pane (pane_t *pane)
{
    component_t *com = pane->head;

    if (pane->parent->selected == pane) {
        pane->repaint = 1;
    }
    while (com) {
        com->repaint = 1;
        //if (pane->repaint) {
        //    gui_com_clear(com);
        //}
        com = com->next;
    }
}

pane_t *gui_search_pane (gui_t *gui, const char *name)
{
    pane_t *pane = gui->head;

    while (pane) {
        if (strcmp(pane->name, name) == 0) {
            return pane;
        }
        pane = pane->next;
    }
    return NULL;
}

component_t *gui_search_com (pane_t *pane, const char *name)
{
    return gui_get_for_name(pane->parent, name);
}

static inline pane_t *
__gui_select_next_pane (gui_t *gui, pane_t *pane)
{
    assert(pane);
    pane = pane->next;

    if (!pane) {
        pane = gui->head;
    }
    return pane;
}

void gui_select_pane (gui_t *gui, pane_t *pane)
{
    pane->prevselected = gui->selected;
    gui->selected = pane;
    gui_wakeup_pane(pane);
}

void gui_release_pane (gui_t *gui, pane_t *pane)
{
    gui->selected = pane->prevselected;
    if (gui->selected) {
        gui_wakeup_pane(gui->selected);
    }
}

pane_t *gui_select_next_pane (gui_t *gui)
{
    pane_t *prevpane = gui->selected;
    pane_t *pane = __gui_select_next_pane(gui, prevpane);

    while (prevpane != pane && !pane->selectable) {
        pane = __gui_select_next_pane(gui, pane);
    }
    gui_release_pane(gui, prevpane);
    gui_select_pane(gui, pane);
    return pane;
}

void *gui_touch (gui_t *gui, gui_obj_type_t *type, const char *path)
{
    pane_t *pane;
    component_t *com;
    char buf[GUI_MAX_STRBUF];
    const char *tokens[8];
    int tkncnt = 0;
    *type = GUIX_INVTYPE;

    snprintf(buf, sizeof(buf), "%s", path);
    tkncnt = d_vstrtok(tokens, arrlen(tokens), buf, '.');

    if (tkncnt < 1) {
        dprintf("%s() : \'tkncnt < 2\'\n", __func__);
        return NULL;
    }
    pane = gui_search_pane(gui, tokens[0]);
    if (!pane) {
        dprintf("%s() : unknown name \'%s\'", __func__, tokens[0]);
        return NULL;
    }
    if (tkncnt < 2) {
        gui_select_pane(gui, pane);
        *type = GUIX_PANE;
        return pane;
    }
    com = gui_search_com(pane, tokens[1]);
    if (!com) {
        dprintf("%s() : unknown name \'%s\'", __func__, tokens[1]);
        return NULL;
    }
    pane->onfocus = com;
    *type = GUIX_COMP;
    return com;
}

component_t *gui_set_focus (gui_t *gui, const char *path)
{
    gui_obj_type_t type;
    void *obj = NULL;

    obj = gui_touch(gui, &type, path);
    switch (type) {
        case GUIX_PANE:
        {
            pane_t *pane = obj;
            gui->selected = pane;
            gui_wakeup_pane(gui->selected);
        }
        break;
        case GUIX_COMP:
        {
            component_t *com = obj;
            com->parent->onfocus = com;
            gui_wakeup_pane(com->parent);
            obj = com;
        }
        break;
        default:
        break;
    }
    return (component_t *)obj;
}

static inline component_t *
__gui_set_next_focus (pane_t *pane, component_t *com)
{
    com = com->next;
    if (!com) {
        com = pane->head;
    }
    return com;
}

component_t *gui_set_next_focus (gui_t *gui)
{
    pane_t *pane = gui->selected;
    component_t *com, *prev;

    if (!pane || !pane->onfocus) {
        return NULL;
    }
    prev = pane->onfocus;
    com = __gui_set_next_focus(pane, prev);

    while (com != prev && !com->selectable) {
        com = __gui_set_next_focus(pane, com);
    }
    pane->onfocus = com;
    pane->repaint = 1;
    __wakeup(gui, com);
    __wakeup(gui, prev);
    return com;
}
