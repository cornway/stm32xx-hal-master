#include <boot_int.h>

typedef enum {
    WINNONE,
    WINHIDE,
    WINCLOSE,
    WINACCEPT,
    WINDECLINE,
    WINSTATEMAX,
} winact_t;

static component_t *
win_new_border (void)
{
    component_t *com = gui_get_comp("pad", "");
    com->bcolor = COLOR_GREY;
    com->fcolor = COLOR_GREY;
    com->ispad = 1;
    return com;
}

static void win_setup_frame (pane_t *pane, int border, int x, int y, int w, int h)
{
    component_t *com;

    com = win_new_border();
    gui_set_comp(pane, com, x, y, border, h);

    com = win_new_border();
    gui_set_comp(pane, com, border, y, w - border, border);

    com = win_new_border();
    gui_set_comp(pane, com, w - border, y + border, border, h - border);

    com = win_new_border();
    gui_set_comp(pane, com, border, h - border, w - border, border);
}

pane_t *win_new_allert
    (gui_t *gui, int w, int h,
     comp_handler_t close, comp_handler_t accept, comp_handler_t decline)
{
    const int bordersize = 8;
    const int btnsize = 40;
    int sfxclose = -1;
    dim_t dim;
    point_t p;
    pane_t *pane = gui_get_pane("allert");
    component_t *com;

    if (gui->alloc_sfx) {
        gui->alloc_sfx(&sfxclose, GUISFX_CLOSE);
    }

    dim_get_origin(&p, &gui->dim);
    gui_set_panexy(gui, pane, p.x, p.y, w, h);

    com = gui_get_comp("a_close", "");
    com->bcolor = COLOR_BLACK;
    com->fcolor = COLOR_RED;
    com->ispad = 1;
    com->release = close;
    com->sfx.sfx_close = sfxclose;
    gui_set_comp(pane, com, 0, 0, btnsize, btnsize);

    com = gui_get_comp("a_title", NULL);
    com->bcolor = COLOR_BLUE;
    com->fcolor = COLOR_WHITE;
    gui_set_comp(pane, com, btnsize, 0, w - btnsize, btnsize);

    com = gui_get_comp("a_accept", "YES");
    com->bcolor = COLOR_GREY;
    com->fcolor = COLOR_BLACK;
    com->release = accept;
    com->sfx.sfx_close = sfxclose;
    gui_set_comp(pane, com, 0, h - btnsize, w / 2, btnsize);
    if (gui->alloc_sfx) {
        gui->alloc_sfx(&com->sfx.sfx_close, GUISFX_CLOSE);
    }

    com = gui_get_comp("a_decline", "NO");
    com->bcolor = COLOR_GREY;
    com->fcolor = COLOR_BLACK;
    com->release = decline;
    com->sfx.sfx_close = sfxclose;
    gui_set_comp(pane, com, w / 2, h - btnsize, w / 2, btnsize);

    dim.x = 0;
    dim.y = btnsize;
    dim.w = w;
    dim.h = h - btnsize;
    dim_get_origin(&p, &dim);
    dim.w = dim.w - 2 * bordersize;
    dim.h = dim.h - 2 * bordersize;
    dim_set_origin(&dim, &p);

    com = gui_get_comp("a_message", NULL);
    com->bcolor = COLOR_BLACK;
    com->fcolor = COLOR_WHITE;
    gui_set_comp(pane, com, dim.x, dim.y, dim.w, dim.h);

    win_setup_frame(pane, bordersize, 0, btnsize, w, h - btnsize);

    pane->iswin = 1;
    pane->repaint = 0;

    return pane;
}

int win_alert (gui_t *gui, const char *text)
{
    pane_t *pane = gui_search_pane(gui, "allert");
    component_t *com;
    if (!pane) {
        return -1;
    }
    com = gui_search_com(pane, "a_message");
    assert(com);
    gui_print(com, text);
    gui_select_pane(gui, pane);
    return 0;
}

int win_close_allert (gui_t *gui, pane_t *pane)
{
    if (!pane) {
        pane = gui_search_pane(gui, "allert");
    }
    if (!pane) {
        return -1;
    }
    gui_release_pane(gui, pane);
    return 0;
}

