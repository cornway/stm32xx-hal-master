#include <stm32f769i_discovery_lcd.h>
#include <gui.h>
#include <jpeg.h>
#include <misc_utils.h>
#include <lcd_main.h>
#include <dev_io.h>
#include <heap.h>
#include <bsp_cmd.h>

typedef struct {
    dim_t activebox;
    dim_t dirtybox;
} gui_int_t; /*internal usage*/

#define gui_error(fmt, args ...) \
    if (gui->dbglvl >= DBG_ERR) {dprintf("%s() [fatal] : "fmt, __func__, args);}

#define gui_warn(args ...) \
    if (gui->dbglvl >= DBG_WARN) {dprintf(args);}

#define gui_info(fmt, args ...) \
    if (gui->dbglvl >= DBG_INFO) {dprintf("%s() : "fmt, __func__, args);}


#define G_TEXT_MAX 256

static d_bool __gui_draw_allowed (gui_t *gui);

static gui_int_t gui_int = {{0}};

void dim_extend (dim_t *dest, dim_t *ref)
{
    int box_dest[4] = {dest->x, dest->y, dest->x + dest->w, dest->y + dest->h};
    int box_ref[4] = {ref->x, ref->y, ref->x + ref->w, ref->y + ref->h};
    int durty = 0;

    if (box_ref[0] < box_dest[0]) {
        box_dest[2] += box_dest[0] - box_ref[0];
        box_dest[0] = box_ref[0];
        durty = 1;
    }
    if (box_ref[1] < box_dest[1]) {
        box_dest[2] += box_dest[1] - box_ref[1];
        box_dest[1] = box_ref[1];
        durty = 1;
    }
    if (box_ref[2] > box_dest[2]) {
        box_dest[2] = box_ref[2];
        durty = 1;
    }
    if (box_ref[3] > box_dest[3]) {
        box_dest[3] = box_ref[3];
        durty = 1;
    }
    if (!durty) {
        return;
    }
    dest->x = box_dest[0];
    dest->y = box_dest[1];
    dest->w = box_dest[2] - dest->x;
    dest->h = box_dest[3] - dest->y;
}

d_bool dim_check_intersect (dim_t *durty, dim_t *dim)
{
    int tmp,
        max_x = durty->x + durty->w,
        max_y = durty->y + durty->h;

    if (dim->x > max_x ||
        dim->y > max_y) {
        return d_false;
    }

    tmp = (dim->x + dim->w) - durty->x;

    if (tmp < 0) {
        return d_false;
    }

    tmp = (dim->y + dim->h) - durty->y;
    if (tmp < 0) {
        return d_false;
    }
    return d_true;
}

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

#define GUI_LINK(parent, item)     \
do  {                             \
    (item)->next = (parent)->head; \
    (parent)->head = (item);       \
} while (0)

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

static inline void
gui_set_dirty (gui_t *gui, dim_t *dim)
{
    gui_int_t *gui_int = gui->user;
    assert(gui_int);

    dim_extend(&gui_int->dirtybox, dim);
}

static inline d_bool
gui_is_com_durty (gui_t *gui, component_t *com)
{
    gui_int_t *gui_int = gui->user;
    assert(gui_int);

    return dim_check_intersect(&gui_int->activebox, &com->dim);
}

static inline void
gui_mark_dirty (gui_t *gui)
{
    gui_int_t *gui_int = gui->user;
    assert(gui_int);

    d_memcpy(&gui_int->activebox, &gui_int->dirtybox, sizeof(gui_int->activebox));
    d_memzero(&gui_int->dirtybox, sizeof(gui_int->dirtybox));
}

static inline void
gui_post_draw (gui_t *gui)
{
    gui_int_t *gui_int = gui->user;
    assert(gui_int);

    d_memzero(&gui_int->activebox, sizeof(gui_int->activebox));
}

/*HAL api*/
/*=======================================================================================*/

void
gui_rect_fill_HAL (component_t *com, dim_t *rect, rgba_t color)
{
    dim_t d = *rect;
    dim_place(&d, &com->dim);
    BSP_LCD_SetTextColor(color);
    BSP_LCD_FillRect(d.x, d.y, d.w, d.h);
    gui_set_dirty(com->parent->parent, &d);
}

void
gui_com_fill_HAL (component_t *com, rgba_t color)
{
    BSP_LCD_SetTextColor(color);
    BSP_LCD_FillRect(com->dim.x, com->dim.y, com->dim.w, com->dim.h);
    gui_set_dirty(com->parent->parent, &com->dim);
}

static int
gui_draw_string_HAL (component_t *com, int line,
                              rgba_t textcolor, const char *str, Text_AlignModeTypdef txtmode)
{
    void *font = BSP_LCD_GetFont();
    int ret;
    dim_t dim;

    d_memcpy(&dim, &com->dim, sizeof(dim));

    if (font != (sFONT *)com->font) {
        BSP_LCD_SetFont((sFONT *)com->font);
    }
    BSP_LCD_SetTextColor(textcolor);
    ret = BSP_LCD_DisplayStringAt(dim.x, dim.y + LINE(line),
                                  dim.w, dim.h, (uint8_t *)str, txtmode);
    if (font != (sFONT *)com->font) {
        BSP_LCD_SetFont(font);
    }
    gui_set_dirty(com->parent->parent, &dim);
    return ret;
}

/*=======================================================================================*/


void gui_init (gui_t *gui, const char *name, uint8_t framerate,
                dim_t *dim, gui_bsp_api_t *bspapi)
{
    char temp[GUI_MAX_NAME];

    d_memzero(gui, sizeof(*gui));

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
    d_memcpy(&gui_int.activebox, &gui->dim, sizeof(gui_int.activebox));
    gui->user = &gui_int;
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

pane_t *gui_create_pane (gui_t *gui, const char *name, int extra)
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
    d_memzero(pane, allocsize);
    snprintf(pane->name, namelen + 1, "%s", name);
    return pane;
}

static void __gui_set_panexy (gui_t *gui, pane_t *pane, int x, int y, int w, int h)
{
    GUI_LINK(gui, pane);
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

void gui_set_child (pane_t *parent, pane_t *child)
{
    child->child = parent->child;
    parent->child = child;
}

rawpic_t *gui_cache_jpeg (gui_t *gui, const char *path)
{
    const uint32_t memsize = (2 << 20);

    if (!gui->cachemem) {
        gui->cachemem = heap_alloc_shared(memsize);
    }
    if (!gui->cachemem) {
        return NULL;
    }
    return jpeg_2_rawpic(path, gui->cachemem, memsize);
}

void gui_set_pic (component_t *com, rawpic_t *pic, int top)
{
    if (!pic->alpha) {
        pic->alpha = 0xff;
    }
    com->pic = pic;
    com->pictop = top;
    if (pic->w > com->dim.w) {
        pic->w = com->dim.w;
    }
    if (pic->h > com->dim.h) {
        pic->h = com->dim.h;
    }
}

static void __gui_set_comp_def_sfx (component_t *com)
{
    gui_sfx_t *sfx = &com->sfx;

    sfx->sfx_cantdothis = -1;
    sfx->sfx_close = -1;
    sfx->sfx_open = -1;
    sfx->sfx_other = -1;
}

component_t *gui_create_comp (gui_t *gui, const char *name, const char *text)
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
    assert(com && name);
    d_memzero(com, allocsize);
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
    GUI_LINK(pane, c);

    c->dim.x = (x > 0) ? x : 0;
    c->dim.y = (y > 0) ? y : 0;
    c->dim.w = (w > 0) ? w : pane->dim.w;
    c->dim.h = (h > 0) ? h : pane->dim.h;
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

static inline void gui_com_clear (component_t *com)
{
    gui_com_fill_HAL(com, com->bcolor);
}

void gui_set_text (component_t *com, const char *text, int x, int y)
{
    snprintf(com->text, com->text_size, "%s", text);
}

void gui_printxy (component_t *com, int x, int y, const char *fmt, ...)
{
    va_list         argptr;

    va_start (argptr, fmt);
    com->text_index = vsnprintf (com->text, com->text_size, fmt, argptr);
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

    com->text_index +=
        vsnprintf (com->text + offset, com->text_size - com->text_index, fmt, argptr);
    va_end (argptr);
    return com->text_size - com->text_index;
}

int gui_draw_string (component_t *com, int line, rgba_t textcolor, const char *str)
{
    return gui_draw_string_HAL(com, line, textcolor, str, LEFT_MODE);
}

int gui_draw_string_c (component_t *com, int line, rgba_t textcolor, const char *str)
{
    return gui_draw_string_HAL(com, line, textcolor, str, CENTER_MODE);
}

static int gui_rawpic_draw (component_t *com, rawpic_t *pic)
{
    screen_t dest = {0}, src = {0};
    point_t p;
    dim_t d = {0, 0, pic->w, pic->h};

    dim_get_origin(&p, &com->dim);
    dim_set_origin(&d, &p);

    vid_get_ready_screen(&dest);

    dest.x = d.x;
    dest.y = d.y;

    src.x = 0;
    src.y = 0;
    src.width = pic->w;
    src.height = pic->h;
    src.buf = pic->data;
    src.colormode = GFX_COLOR_MODE_RGBA8888;
    src.alpha = pic->alpha;

    vid_copy(&dest, &src);
    vid_vsync(0);
    return 0;
}

static void gui_comp_draw (pane_t *pane, component_t *com)
{
    if (com->ispad) {
        gui_com_clear(com);
        return;
    }
    if (com->pic && !com->pictop) {
        gui_rawpic_draw(com, com->pic);
    }
    if (!com->userdraw) {
        rgba_t color;
        int len = 0, tmp;
        int line = 0;
        uint8_t *text;

        color = gui_com_select_color(com);

        if (com->draw) {
            com->draw(pane, com, NULL);
        } else if (com->bcolor != color) {
            gui_com_fill_HAL(com, color);
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
    if (com->pic && com->pictop) {
        gui_rawpic_draw(com, com->pic);
    }
}

static void gui_pane_draw (gui_t *gui, pane_t *pane)
{
    component_t *com = pane->head;

    vid_vsync(0);
    if (pane->child && pane->child->repaint) {
        gui_pane_draw(gui, pane->child);
    }
    while (com) {
        if (gui_is_com_durty(gui, com)) {
            gui_comp_draw(pane, com);
        }
        com = com->next;
    }
    pane->repaint = 0;
}

void gui_draw (gui_t *gui, int force)
{
    pane_t *pane = gui->head;
    pane_t *selected = gui->selected_head;
    const dim_t *invis = NULL;

    if (!__gui_draw_allowed(gui)) {
        return;
    }
    if (force) {
        gui_mark_dirty(gui);
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
    gui_post_draw(gui);
}

static component_t *
gui_get_com_4_xy (pane_t *pane, const gevt_t *evt, point_t *pplocal)
{
    component_t *com;
    point_t plocal = evt->p;

    vid_ptr_align(&plocal.x, &plocal.y);
    dim_tolocal_p(&plocal, &pane->dim);
    com = pane->onfocus;

    if (com) {
        if (evt->e == GUIRELEASE) {
            gui_set_focus(pane, NULL, com);
        }
    } else {
        com = pane->head;
    }
    while (com) {
        if (dim_check(&com->dim, &evt->p)) {
            break;
        }
        com = com->next;
    }
    if (com) {
        pplocal->x = plocal.x;
        pplocal->y = plocal.y;
        dim_tolocal_p(pplocal, &com->dim);
        if (evt->e == GUIACT) {
            gui_set_focus(pane, com, NULL);
        }
    }
    return com;
}

static int
gui_com_input_direct (pane_t *pane, component_t *com, gevt_t *evt)
{
    comp_handler_t h;

    if (evt->e == GUIRELEASE) {
        h = com->release;
    } else {
        h = com->act;
    }
    if (h && h(pane, com, evt) > 0) {
        return 1;
    }
    return 0;
}

static int
gui_com_input_xy (pane_t *pane, const gevt_t *evt)
{
    point_t plocal;
    comp_handler_t h;
    d_bool isrelease = d_false;
    component_t *com = gui_get_com_4_xy(pane, evt, &plocal);
    gevt_t comevt;

    if (!com) {
        return 0;
    }

    if (isrelease) {
        h = com->release;
    } else {
        h = com->act;
    }
    if (!h) {
        return 0;
    }

    comevt.e = evt->e;
    comevt.p = plocal;
    comevt.sym = evt->sym;
    comevt.symbolic = evt->symbolic;

    if (h(pane, com, &comevt) > 0) {
        return 1;
    }
    return 0;
}

int gui_send_event (gui_t *gui, gevt_t *evt)
{
    component_t *com = NULL;
    pane_t *pane = gui->selected_head;
    int handled = 0;

    assert(pane);
    if (evt->symbolic) {
        if (pane->onfocus == NULL) {
            return 0;
        }
        com = pane->onfocus;
    }
    if (com) {
        if (pane == gui->selected_head) {
            handled = gui_com_input_direct(pane, com, evt);
        }
    } else {
        handled = gui_com_input_xy(pane, evt);
    }
    return handled;
}

static int
gui_contain_child (pane_t *parent, pane_t *child)
{
    while (parent) {
        if (parent == child) {
            return 1;
        }
        parent = parent->child;
    }
    return 0;
}

void gui_wakeup_com (gui_t *gui, component_t *com)
{
    pane_t *pane = com->parent;

    if (pane != gui->selected_head &&
        !gui_contain_child(gui->selected_head, pane)) {
        return;
    }
    if (gui_is_com_durty(gui, com)) {
        gui_com_clear(com);
    }
    com->parent->repaint = 1;

}

void gui_wakeup_pane (pane_t *pane)
{
    component_t *com = pane->head;
    gui_t *gui = pane->parent;

    if (pane->parent->selected_head == pane) {
        if (pane->child) {
            gui_wakeup_pane(pane->child);
        }
    }
    gui_set_dirty(gui, &pane->dim);
    gui_mark_dirty(gui);
    while (com) {
        gui_wakeup_com(pane->parent, com);
        com = com->next;
    }
}

pane_t *gui_get_pane_4_name (gui_t *gui, const char *name)
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

void gui_select_pane (gui_t *gui, pane_t *pane)
{
    pane->selected_next = gui->selected_head;
    if (!gui->selected_tail) {
        gui->selected_tail = pane;
    }
    gui->selected_head = pane;

    gui_wakeup_pane(pane);
}

pane_t *gui_release_pane (gui_t *gui)
{
    pane_t *head = gui->selected_head,
           *tail = gui->selected_tail;

    if (!head) {
        return NULL;
    }
    if (head == tail) {
        gui->selected_head = NULL;
        gui->selected_tail = NULL;
    } else {
        gui->selected_head = head->selected_next;
    }
    return head;
}

static inline pane_t *
__gui_iter_next_pane (gui_t *gui, pane_t *pane)
{
    assert(pane);
    pane = pane->next;

    if (!pane) {
        pane = gui->head;
    }
    return pane;
}

pane_t *
gui_select_next_pane (gui_t *gui)
{
    pane_t *prevpane = gui->selected_head;
    pane_t *pane = __gui_iter_next_pane(gui, prevpane);

    while (prevpane != pane && !pane->selectable) {
        pane = __gui_iter_next_pane(gui, pane);
    }
    if (prevpane != pane) {
        gui_release_pane(gui);
        gui_select_pane(gui, pane);
    }
    return pane;
}

static inline component_t *
__gui_iter_next_focus (pane_t *pane, component_t *com)
{
    com = com->next;
    if (!com) {
        com = pane->head;
    }
    return com;
}

component_t *
gui_set_focus (pane_t *pane, component_t *com, component_t *prev)
{
    gui_t *gui = pane->parent;
    int dirty = 0;

    assert(com || prev);
    pane->onfocus = com;
    if (com) {
        gui_set_dirty(gui, &com->dim);
        com->glow = 127;
        dirty = 1;
    }
    if (prev) {
        gui_set_dirty(gui, &prev->dim);
        prev->glow = 0;
        dirty = 1;
    }

    if (dirty) gui_mark_dirty(gui);
    if (com) gui_wakeup_com(gui, com);
    if (prev) gui_wakeup_com(gui, prev);

    return com;
}

component_t *gui_set_next_focus (gui_t *gui)
{
    pane_t *pane = gui->selected_head;
    component_t *com, *prev;

    if (!pane || !pane->onfocus) {
        return NULL;
    }
    prev = pane->onfocus;
    com = __gui_iter_next_focus(pane, prev);

    while (com != prev && !com->selectable) {
        com = __gui_iter_next_focus(pane, com);
    }
    if (com == prev) {
        return NULL;
    }

    return gui_set_focus(pane, com, prev);
}

static d_bool __gui_draw_allowed (gui_t *gui)
{
    if (0 == d_rlimit_wrap(&gui->repainttsf, 1000 / gui->framerate)) {
        return d_false;
    }
    return d_true;
}

