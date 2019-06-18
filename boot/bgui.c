
#include <stm32f769i_discovery_lcd.h>
#include <boot_int.h>
#include <boot.h>
#include <lcd_main.h>
#include <dev_io.h>

static int g_gui_debug_lvl = DBG_WARN;

#define gui_error(fmt, args ...) \
    if (g_gui_debug_lvl >= DBG_ERR) {dprintf("%s() [fatal] : "fmt, __func__, args);}

#define gui_warn(args ...) \
    if (g_gui_debug_lvl >= DBG_WARN) {dprintf(args);}

#define gui_info(fmt, args ...) \
    if (g_gui_debug_lvl >= DBG_INFO) {dprintf("%s() : "fmt, __func__, args);}


#define G_TEXT_MAX 128

typedef struct {
    gui_t gui;
    uint8_t fonth, fontw;
} gui_ctxt_t;

gui_ctxt_t g_gui_ctxt;

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

void gui_init (gui_t *gui, int x, int y, int w, int h)
{
    memset(gui, 0, sizeof(*gui));

    g_gui_ctxt.fonth = BSP_LCD_GetFont()->Height;
    g_gui_ctxt.fontw = BSP_LCD_GetFont()->Width;

    gui->dim.x = x;
    gui->dim.y = y;
    gui->dim.w = w;
    gui->dim.h = h;

    d_dvar_int32(&g_gui_debug_lvl, "guidbglvl");
}

pane_t *gui_get_pane (const char *name)
{
    int namelen = strlen(name);
    int allocsize;
    pane_t *pane;

    allocsize = sizeof(*pane) + namelen + 1;
    allocsize = ROUND_UP(allocsize, 4);
    pane = Sys_Malloc(allocsize);
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
    gui->selected = pane;
    pane->repaint = 1;
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

component_t *gui_get_comp (const char *name, const char *text)
{
    int namelen = strlen(name);
    int textlen = text ? strlen(text) : G_TEXT_MAX;
    int allocsize;
    component_t *com;

    allocsize = sizeof(*com) + namelen + 1 + textlen + 1;
    allocsize = ROUND_UP(allocsize, sizeof(uint32_t));
    com = Sys_Malloc(allocsize);
    memset(com, 0, allocsize);
    assert(name);
    snprintf(com->name_text, namelen + 1, "%s", name);
    if (text) {
        snprintf(com->name_text + namelen + 1, textlen + 1, "%s", text);
    }
    com->text_offset = namelen + 1;
    com->text_size = textlen;
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
    c->repaint = 1;
    c->parent = pane;
    dim_place(&c->dim, &pane->dim);

    if (pane->parent->alloc_sfx) {
        pane->parent->alloc_sfx(&c->sfx.sfx_other, GUISFX_OTHER);
    }
}

void gui_set_prop (component_t *c, prop_t *prop)
{
    c->fcolor = prop->fcolor;
    c->bcolor = prop->bcolor;
    c->ispad = prop->ispad;
    c->userdraw = prop->user_draw;
}

void gui_com_clear (component_t *com)
{
    screen_vsync();
    BSP_LCD_SetTextColor(com->bcolor);
    BSP_LCD_FillRect(com->dim.x, com->dim.y, com->dim.w, com->dim.h);
}

static inline void
__gui_text (component_t *com, char *buf, int bufsize, const char *text, int x, int y)
{
    snprintf(buf, bufsize, "%s", text);
    com->repaint = 1;
}

void gui_text (component_t *com, const char *text, int x, int y)
{
    __gui_text(com, com->name_text + com->text_offset, com->text_size, text, x, y);
}

static uint32_t __gui_printxy
                    (component_t *com, char *buf, int bufsize,
                     int x, int y, const char *fmt, va_list argptr)
{
    return vsnprintf (buf, bufsize, fmt, argptr);
}

void gui_printxy (component_t *com, int x, int y, const char *fmt, ...)
{
    va_list         argptr;
    uint32_t printed;

    va_start (argptr, fmt);
    printed = __gui_printxy(com, com->name_text + com->text_offset, com->text_size, x, y, fmt, argptr);
    va_end (argptr);
}

void gui_apendxy (component_t *com, int x, int y, const char *fmt, ...)
{
    va_list         argptr;
    int offset = com->text_offset + com->text_offset;

    if (com->text_offset >= com->text_size - 1) {
        return;
    }

    va_start (argptr, fmt);
    com->text_offset += __gui_printxy(com, com->name_text + offset, com->text_size - offset, x, y, fmt, argptr);
    va_end (argptr);
}


static void gui_comp_draw (pane_t *pane, component_t *com)
{
    screen_t screen;

    if (com->ispad) {
        BSP_LCD_SetTextColor(com->bcolor);
        BSP_LCD_FillRect(com->dim.x, com->dim.y, com->dim.w, com->dim.h);
    } else if (!com->userdraw) {
        int x, y, w, h;

        if (com->draw) {
            com->draw(pane, com, NULL);
        }
        
        BSP_LCD_SetTextColor(com->bcolor);
        BSP_LCD_FillRect(com->dim.x, com->dim.y, com->dim.w, com->dim.h);

        BSP_LCD_SetTextColor(com->fcolor);

        x = com->dim.x + g_gui_ctxt.fontw / 2;
        y = com->dim.y + g_gui_ctxt.fonth / 2;
        w = com->dim.w;
        h = com->dim.h;

        if (com->showname) {
            BSP_LCD_DisplayStringAt(x, y, w, h, com->name_text, LEFT_MODE);
        }
        y += g_gui_ctxt.fonth;
        BSP_LCD_DisplayStringAt(x, y, w, h, com->name_text + com->text_offset, LEFT_MODE);
    } else if (com->draw) {
        com->draw(pane, com, NULL);
    }
}

static void gui_pane_draw (gui_t *gui, pane_t *pane)
{
    component_t *com = pane->head;

    while (com) {
        if (com->repaint) {
            screen_vsync();
            gui_comp_draw(pane, com);
            com->repaint = 0;
        }
        com = com->next;
    }
}

void gui_draw (gui_t *gui)
{
    pane_t *pane = gui->head;
    pane_t *selected = gui->selected;

    while (pane) {
        if (pane != selected && pane->repaint) {
            gui_pane_draw(gui, pane);
            pane->repaint = 0;
        }
        pane = pane->next;
    }
    if (selected && selected->repaint) {
        gui_pane_draw(gui, selected);
        selected->repaint = 0;
    }
}

static void gui_act (gui_t *gui, component_t *com, gevt_t *evt)
{
    comp_handler_t h;
    pane_t *pane = com->parent;
    d_bool isrelease = d_false;

    if (pane == gui->selected) {
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
    if (h(pane, com, NULL) > 0) {
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

    screen_ts_align(&evt->p.x, &evt->p.y);
    if (com) {
        gui_act(gui, com, evt);
    }

    if (evt->e == GUIRELEASE) {
        isrelease = d_true;
    }
    if (pane) {
        point_t pp, p;

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
                    p = pp;
                    dim_tolocal_p(&p, &com->dim);
                    if (h(pane, com, &p) > 0) {
                        if (!isrelease) {
                            pane->onfocus = com;
                            com->focus = 1;
                            com->glow = 127;
                        }
                        break;
                    } else {
                        if (gui->start_sfx) {
                            gui->start_sfx(com->sfx.sfx_other);
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
    const char *name = user;

    return !strcmp(com->name_text, user);
}

static component_t *gui_get_for_name (gui_t *gui, const char *name)
{
    component_t *com;

    com = gui_iterate_all(gui, (char *)name, __namecomp);
    return com;
}

static int __wake (component_t *com, void *user)
{
    com->repaint = 1;
    com->parent->repaint = 1;
    return 0;
}

void gui_wake (gui_t *gui, const char *name)
{
    if (name == NULL) {
        gui_iterate_all(gui, NULL, __wake);
    } else {
        component_t *com = gui_get_for_name(gui, name);
        if (com) {
            __wake(com, NULL);
        }
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

void gui_select_pane (gui_t *gui, pane_t *pane)
{
    pane->prevselected = gui->selected;
    gui->selected = pane;
    pane->repaint = 1;
}

void gui_release_pane (gui_t *gui, pane_t *pane)
{
    gui->selected = pane->prevselected;
    if (gui->selected) {
        gui->selected->repaint = 1;
    }
    pane->repaint = 0;
}

