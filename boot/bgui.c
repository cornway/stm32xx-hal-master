
#include <stm32f769i_discovery_lcd.h>
#include <boot_int.h>
#include <boot.h>
#include <lcd_main.h>

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
    snprintf(pane->text, namelen + 1, "%s", name);
    return pane;
}

void gui_set_pane (gui_t *gui, pane_t *pane)
{
    pane->next = NULL;
    if (gui->head == NULL) {
        gui->head = pane;
        gui->tail = pane;
    } else {
        pane->next = gui->head;
        gui->head = pane;
    }
    pane->dim.x = gui->dim.x;
    pane->dim.y = gui->dim.y;
    pane->dim.w = gui->dim.w;
    pane->dim.h = gui->dim.h;
    pane->parent = gui;
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
    dim_place(&c->dim, &pane->dim);
    c->repaint = 1;
    c->parent = pane;
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

void gui_text (component_t *com, const char *text, int x, int y)
{
    sprintf(com->name_text + com->text_offset, "%s", text);
    com->repaint = 1;
}

void gui_printxy (component_t *com, int x, int y, char *fmt, ...)
{
    va_list         argptr;
    char            string[1024];
    int size = 0, max = sizeof(string);

    va_start (argptr, fmt);
    size += vsnprintf (string, max, fmt, argptr);
    va_end (argptr);

    assert(size < arrlen(string));
    gui_text(com, string, x, y);
}

static void gui_comp_draw (pane_t *pane, component_t *com)
{
    screen_t screen;

    if (com->ispad) {
        BSP_LCD_SetTextColor(com->bcolor);
        BSP_LCD_FillRect(com->dim.x, com->dim.y, com->dim.w, com->dim.h);
    } else if (!com->userdraw) {
        BSP_LCD_SetTextColor(com->bcolor);
        BSP_LCD_FillRect(com->dim.x, com->dim.y, com->dim.w, com->dim.h);

        BSP_LCD_SetTextColor(com->fcolor);
        BSP_LCD_DisplayStringAt(com->dim.x + g_gui_ctxt.fontw / 2, com->dim.y + g_gui_ctxt.fonth / 2, com->name_text, LEFT_MODE);
        BSP_LCD_DisplayStringAt(com->dim.x + g_gui_ctxt.fontw / 2, com->dim.y + (g_gui_ctxt.fonth * 3) / 2, com->name_text + com->text_offset, LEFT_MODE);
    }
    if (com->draw) {
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

    while (pane) {
        gui_pane_draw(gui, pane);
        pane = pane->next;
    }
}

void gui_resp (gui_t *gui, gevt_t *evt)
{
    comp_handler_t h;
    d_bool isrelease = d_false;

    pane_t *pane = gui->head;
    component_t *com;

    screen_ts_align(&evt->p.x, &evt->p.y);

    if (evt->e == GUIRELEASE) {
        isrelease = d_true;
    }
    while (pane) {
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
                    }
                }
            }
            com = com->next;
        }
        pane = pane->next;
    }
}

void gui_act (gui_t *gui, component_t *com, gevt_t *evt)
{
    pane_t *pane = com->parent;
    d_bool isrelease = d_false;

    if (evt->e == GUIRELEASE) {
        isrelease = d_true;
    }
    if (pane->onfocus) {
        if (isrelease) {
            pane->onfocus = NULL;
            com->focus = 0;
        }
    }
    if (!isrelease) {
        pane->onfocus = com;
        com->focus = 1;
        com->glow = 127;
        if (com->release) {
            com->release(pane, com, NULL);
        } else if (com->act) {
            com->act(pane, com, NULL);
        }
        return;
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
    return 0;
}

void gui_wake (gui_t *gui, const char *name)
{
    if (name == NULL) {
        gui_iterate_all(gui, NULL, __wake);
    } else {
        component_t *com = gui_get_for_name(gui, name);
        if (com) {
            com->repaint = 1;
        }
    }
}

