#include <string.h>
#include <gfx.h>
#include <gui.h>

#if defined(BSP_DRIVER)

#define WIN_ERR(args ...) \
    dprintf("gui err : "args)

typedef enum {
    WINNONE,
    WINHIDE,
    WINCLOSE,
    WINACCEPT,
    WINDECLINE,
    WINSTATEMAX,
} winact_t;

typedef enum {
    WIN_NONE,
    WIN_ALERT,
    WIN_CONSOLE,
    WIN_MAX,
} WIN_TYPE;

typedef struct {
    pane_t *pane;
    WIN_TYPE type;
} win_t;

static component_t *
win_new_border (gui_t *gui)
{
    component_t *com = gui_get_comp(gui, "pad", "");
    com->bcolor = COLOR_GREY;
    com->fcolor = COLOR_GREY;
    com->ispad = 1;
    return com;
}

static void win_setup_frame (dim_t *dim, pane_t *pane, int border, int x, int y, int w, int h)
{
    gui_t *gui = pane->parent;
    component_t *com;

    if (dim) {
        dim->x = x + border;
        dim->y = y + border;

        dim->w = w - border * 2;
        dim->h = h - border * 2;
    }
    com = win_new_border(gui);
    gui_set_comp(pane, com, x, y, border, h);

    com = win_new_border(gui);
    gui_set_comp(pane, com, border, y, w - border, border);

    com = win_new_border(gui);
    gui_set_comp(pane, com, w - border, y + border, border, h - border);

    com = win_new_border(gui);
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
    pane_t *pane;
    component_t *com;

    pane = gui_get_pane(gui, "allert", 0);
    if (gui_bsp_sfxalloc(gui)) {
        sfxclose = gui_bsp_sfxalloc(gui)("guiclose");
    }

    dim_get_origin(&p, &gui->dim);
    gui_set_panexy(gui, pane, p.x, p.y, w, h);

    com = gui_get_comp(gui, "a_close", "");
    com->bcolor = COLOR_BLACK;
    com->fcolor = COLOR_RED;
    com->ispad = 1;
    com->release = close;
    com->sfx.sfx_close = sfxclose;
    gui_set_comp(pane, com, 0, 0, btnsize, btnsize);

    com = gui_get_comp(gui, "a_title", NULL);
    com->bcolor = COLOR_BLUE;
    com->fcolor = COLOR_WHITE;
    gui_set_comp(pane, com, btnsize, 0, w - btnsize, btnsize);

    com = gui_get_comp(gui, "a_accept", "YES");
    com->bcolor = COLOR_GREY;
    com->fcolor = COLOR_BLACK;
    com->release = accept;
    com->sfx.sfx_close = sfxclose;
    gui_set_comp(pane, com, 0, h - btnsize, w / 2, btnsize);
    com->sfx.sfx_close = sfxclose;

    com = gui_get_comp(gui, "a_decline", "NO");
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

    com = gui_get_comp(gui, "a_message", NULL);
    com->bcolor = COLOR_BLACK;
    com->fcolor = COLOR_WHITE;
    gui_set_comp(pane, com, dim.x, dim.y, dim.w, dim.h);

    win_setup_frame(NULL, pane, bordersize, 0, btnsize, w, h - btnsize);

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
    gui_print(com, "%s", text);
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

typedef struct con_line_s {
    struct con_line_s *next;
    char *ptr;
    rgba_t color;
    uint16_t pos;
    uint8_t flags;
} con_line_t;

typedef struct {
    win_t win;
    component_t *com;

    uint16_t wmax, hmax;

    char *textbuf;
    con_line_t *linehead, *linetail;
    con_line_t *lastline;
    con_line_t line[1];
} win_con_t;

static int win_con_repaint (pane_t *pane, component_t *com, void *user);

static win_con_t *WCON_HANDLE (void *_pane)
{
    pane_t *pane = _pane;
    win_con_t *con;
    win_t *win;

    win = (win_t *)(pane + 1);
    assert(win->type == WIN_CONSOLE);
    assert(win->pane == pane);

    con = container_of(win, win_con_t, win);
    return con;
}

static int wcon_print_line (int *nextline, char *dest, const char *src, int len)
{
    int cnt = 0;

    *nextline = 0;
    for (cnt = 0; *src && cnt < len; cnt++) {
        if (*src == '\n') {
            *nextline = 1;
            *dest = 0;
            cnt++;
            break;
        } else if (*src == '\r') {
           *dest = ' ';
        } else {
           *dest = *src;
        }
        dest++;
        src++;
    }
    if (*src || cnt == len) {
        *nextline = 1;
    }
    *dest = 0;
    return cnt;
}

static con_line_t *wcon_next_line (win_con_t *con)
{
    con_line_t *line = con->linehead;
    /*Move first to the tail, to make it 'scrolling'*/
    con->linehead = line->next;
    con->linetail->next = line;
    con->linetail = line;
    line->next = NULL;
    return line;
}

static inline void wcon_reset_line (con_line_t *line)
{
    line->pos = 0;
}

static void wcon_lsetup_lines (win_con_t *con, rgba_t textcolor)
{
    char *buf = con->textbuf;
    con_line_t *line, *prev = NULL;
    int i;

    line = &con->line[0];
    con->linehead = line;

    for(i = 0; i < con->hmax; i++) {
        line->color = textcolor;
        line->flags = 0;
        line->pos = 0;
        line->ptr = buf;
        buf += con->wmax;
        prev = line;
        line++;
        if (prev) {
            prev->next = line;
        }
    }
    con->linetail = (line - 1);
    con->lastline = con->linehead;
}

static win_con_t *wcon_alloc (gui_t *gui, int x, int y, int w, int h)
{
    uint32_t wmax, hmax;
    int textsize, textoff;
    pane_t *pane;
    win_con_t *win;

    int winmemsize = sizeof(win_con_t);

    wmax = w / gui->fontw;
    hmax = h / gui->fonth;

    textsize = hmax * wmax;
    winmemsize += hmax * sizeof(con_line_t);
    winmemsize += textsize;
    textoff = winmemsize - textsize;

    pane = gui_get_pane(gui, "console", winmemsize);

    win = (win_con_t *)(pane + 1);
    memset(win, 0, winmemsize);
    win->win.pane = pane;
    win->win.type = WIN_CONSOLE;
    win->wmax = wmax;
    win->hmax = hmax;
    win->textbuf = (char *)win + textoff;

    gui_set_pane(gui, pane);

    return win;
}

pane_t *win_new_console (gui_t *gui, prop_t *prop, int x, int y, int w, int h)
{
    uint8_t bordersize = 8;
    component_t *com;
    win_con_t *win;
    dim_t dim;

    win = wcon_alloc(gui, x, y, w, h);
    wcon_lsetup_lines(win, prop->fcolor);

    win_setup_frame(&dim, win->win.pane, bordersize, 0, 0, w, h);

    prop->user_draw = 1;
    com = gui_get_comp(gui, "textarea", "");
    com->draw = win_con_repaint;
    gui_set_prop(com, prop);
    gui_set_comp(win->win.pane, com, dim.x, dim.y, dim.w, dim.h);

    win->com = com;

    return win->win.pane;
}

static int wcon_append_line (int *nextline, win_con_t *con, con_line_t *line, const char *str)
{
    char *ptr;
    int len = con->wmax - line->pos;
    int cnt;

    if (len <= 0) {
        line->pos = 0;
        len = con->wmax;
    }

    ptr = line->ptr + line->pos;
    cnt = wcon_print_line(nextline, ptr, str, len);
    line->pos += cnt;
    return cnt;
}

static void wcom_wakeup (win_con_t *con)
{
    gui_wakeup_pane(con->win.pane);
}

int win_con_append (pane_t *pane, const char *str, rgba_t textcolor)
{
    win_con_t *con;
    const char *strptr = str;
    int charcnt = 0, linecnt = 0;
    int tmp, nextline;
    con_line_t *line;

    con = WCON_HANDLE(pane);
    line = con->lastline;
    assert(line);
    while (*strptr && linecnt < con->hmax) {

        nextline = 0;
        tmp = wcon_append_line(&nextline, con, line, strptr);
        strptr += tmp;

        if (nextline) {
            line = wcon_next_line(con);
            wcon_reset_line(line);
            linecnt++;
        }
    }
    con->lastline = line;

    charcnt = strptr - str;
    if (charcnt) {
        wcom_wakeup(con);
    }
    return charcnt;
}

static int win_con_repaint (pane_t *pane, component_t *com, void *user)
{
    win_con_t *con;
    con_line_t *line;
    int linecnt = 0;

    con = WCON_HANDLE(pane);
    assert(com == con->com);
    gui_com_clear(con->com);

    line = con->linehead;

    while (line) {
        if (line->pos) {
            gui_string_direct(con->com, linecnt, line->color, line->ptr);
            linecnt++;
        }
        line = line->next;
    }
    return linecnt;
}


#endif /*BSP_DRIVER*/

