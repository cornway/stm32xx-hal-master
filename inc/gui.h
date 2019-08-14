#include <stdint.h>
#include <misc_utils.h>
#include <debug.h>

typedef uint32_t rgba_t;

#define GUI_MAX_NAME 16
#define GUI_MAX_STRBUF 256

typedef struct {
    void *data;
    uint16_t w, h;
    uint8_t colormode;
    uint8_t alpha;
} rawpic_t;

typedef enum {
    GUIX_INVTYPE,
    GUIX_COMP,
    GUIX_PANE,
    GUIX_MAXTYPE,
} gui_obj_type_t;

typedef struct {
    int sfx_open, sfx_close;
    int sfx_cantdothis;
    int sfx_other;
} gui_sfx_t;

typedef struct {
    int x, y, w, h;
} dim_t;

typedef struct {
    union {
        struct {int x, y;};
        struct {int w, h;};
    };
} point_t;

struct component_s;
struct pane_s;
struct gui_s;

typedef int (*comp_handler_t) (struct pane_s *p, struct component_s *c, void *user);

typedef struct {
    const void *font;
    uint8_t w, h;
} fontprop_t;

typedef struct prop_s {
    fontprop_t fontprop;
    rgba_t fcolor, bcolor;
    d_bool ispad;
    d_bool user_draw;
    gui_sfx_t sfx;
    const char *name;
} prop_t;

#define GUI_COMMON(type, parenttype)  \
struct {                                \
    struct type *next;                  \
    struct parenttype *parent;         \
    rgba_t bcolor, fcolor;              \
    void *ctxt, *user;                  \
    dim_t dim;                          \
    gui_sfx_t sfx;                      \
    uint16_t focus: 1,                  \
             type:  6,                  \
             ispad:   1,                \
             userdraw: 1;               \
}

typedef struct component_s {
    GUI_COMMON(component_s, pane_s);

    const void *font;
    comp_handler_t act, release;
    comp_handler_t draw;

    uint16_t text_offset;
    uint16_t text_size;
    uint16_t text_index;

    uint16_t glow:     8,
             showname: 1,
             selectable: 1,
             reserved: 6;
    uint32_t userflags;

    char name[GUI_MAX_NAME];
    char text[1];
} component_t;

/*must be about fixed size*/
typedef struct pane_s {
    GUI_COMMON(pane_s, gui_s);

    struct pane_s *child;
    rawpic_t *pic;
    component_t *onfocus;
    
    component_t *head, *tail;
    struct pane_s *prevselected;

    char name[GUI_MAX_NAME];

    uint8_t selectable: 1,
            repaint: 1,
            picontop: 1;
} pane_t;

typedef struct gui_bsp_api_s {
    struct mem_s {
        void *(*alloc) (int);
        void (*free) (void *);
    } mem;
    struct sfx_s {
        int (*alloc) (const char *name);
        void (*release) (int);
        void (*play) (int, uint8_t);
        int (*stop) (int);
    } sfx;
} gui_bsp_api_t;

#define gui_bsp_alloc(gui) \
    (gui)->bspapi.mem.alloc

#define gui_bsp_free(gui) \
    (gui)->bspapi.mem.free

#define gui_bsp_sfxalloc(gui) \
    (gui)->bspapi.sfx.alloc

#define gui_bsp_sfxrelease(gui) \
    (gui)->bspapi.sfx.release

#define gui_bsp_sfxplay(gui) \
    (gui)->bspapi.sfx.play

#define gui_bsp_sfxstop(gui) \
    (gui)->bspapi.sfx.stop

typedef struct gui_s {
    void *directmem;
    void *cachemem;
    void *user;
    dim_t dim;
    pane_t *head, *tail;
    pane_t *selected;
    uint32_t repainttsf;
    int32_t framerate;
    int32_t dbglvl;
    const void *font;
    uint8_t destroy;
    char name[GUI_MAX_NAME];

    gui_bsp_api_t bspapi;
} gui_t;

typedef enum {
    GUINONE,
    GUIACT,
    GUIRELEASE,
    GUIEVTMAX
} GUIEVENT;

typedef struct gevt_s {
    point_t p;
    GUIEVENT e;
    char sym;
} gevt_t;

typedef struct app_gui_api_s {
    bspdev_t dev;
} app_gui_api_t;

#if BSP_INDIR_API

#else /*BSP_INDIR_API*/

#endif /*BSP_INDIR_API*/

#define gui_print(com, args...) gui_printxy(com, 0, 0, args)

d_bool dim_check (const dim_t *d, const point_t *p);
void dim_place (dim_t *d, const dim_t *s);
void dim_tolocal (dim_t *d, const dim_t *s);
void dim_get_origin (point_t *d, const dim_t *s);
void dim_set_origin (dim_t *d, const point_t *s);
void dim_set_right (dim_t *d, dim_t *s);
void dim_set_left (dim_t *d, dim_t *s);
void dim_set_top (dim_t *d, dim_t *s);
void dim_set_bottom (dim_t *d, dim_t *s);

void gui_init (gui_t *gui, const char *name, uint8_t framerate,
                dim_t *dim, gui_bsp_api_t *bspapi);

void gui_destroy (gui_t *gui);
pane_t *gui_get_pane (gui_t *gui, const char *name, int extra);
void gui_set_pane (gui_t *gui, pane_t *pane);
void gui_set_panexy (gui_t *gui, pane_t *pane, int x, int y, int w, int h);
void gui_set_child (pane_t *parent, pane_t *child);

rawpic_t *gui_cache_jpeg (pane_t *, const char *path);
void gui_set_pic (pane_t *pane, rawpic_t *pic, int top);

component_t *gui_get_comp (gui_t *gui, const char *name, const char *text);
void gui_set_comp (pane_t *pane, component_t *c, int x, int y, int w, int h);
void gui_set_prop (component_t *c, prop_t *prop);
void gui_get_font_prop (fontprop_t *prop, const void *_font);
const void *gui_get_font_4_size (gui_t *gui, int size, int bestmatch);

void gui_set_text (component_t *com, const char *text, int x, int y);
void gui_printxy (component_t *com, int x, int y, const char *fmt, ...) PRINTF_ATTR(4, 5);
int gui_apendxy (component_t *com, int x, int y, const char *fmt, ...) PRINTF_ATTR(4, 5);
int gui_draw_string (component_t *com, int line, rgba_t textcolor, const char *str);
int gui_draw_string_c (component_t *com, int line, rgba_t textcolor, const char *str);
void gui_rect_fill_HAL (component_t *com, dim_t *rect, rgba_t color);

void gui_com_clear (component_t *com);
void gui_com_fill_HAL (component_t *com, rgba_t color);
void gui_draw (gui_t *gui);
void gui_resp (gui_t *gui, component_t *com, gevt_t *evt);
void gui_wakeup_com (gui_t *gui, component_t *com);
void gui_wakeup_pane (pane_t *pane);
pane_t *gui_search_pane (gui_t *gui, const char *name);
component_t *gui_search_com (pane_t *pane, const char *name);
void gui_select_pane (gui_t *gui, pane_t *pane);
void gui_release_pane (gui_t *gui, pane_t *pane);
pane_t *gui_select_next_pane (gui_t *gui);
component_t *gui_set_next_focus (gui_t *gui);

#define COLOR_WHITE (GFX_RGBA8888(0xffU, 0xffU, 0xffU, 0xffU))
#define COLOR_BLACK (GFX_RGBA8888(0x00U, 0x00U, 0x00U, 0xffU))
#define COLOR_YELLOW (GFX_RGBA8888(0x7fU, 0x7fU, 0x00U, 0xffU))
#define COLOR_RED (GFX_RGBA8888(0xffU, 0x00U, 0x00U, 0xffU))
#define COLOR_GREEN (GFX_RGBA8888(0x00U, 0xffU, 0x00U, 0xffU))
#define COLOR_BLUE (GFX_RGBA8888(0x00U, 0x00U, 0xffU, 0xffU))
#define COLOR_LBLUE (GFX_RGBA8888(0x80U, 0x80U, 0xffU, 0xffU))
#define COLOR_GREY (GFX_RGBA8888(0x80U, 0x80U, 0x80U, 0xffU))
#define COLOR_DGREY (GFX_RGBA8888(0x40U, 0x40U, 0x40U, 0xffU))
#define COLOR_LGREY (GFX_RGBA8888(0xC0U, 0xC0U, 0xC0U, 0xffU))

typedef int (*win_handler_t) (pane_t *, void *, int);

void win_set_act_clbk (void *_pane, comp_handler_t h);
component_t *win_get_user_component (pane_t *pane);

pane_t *win_new_allert (gui_t *gui, int w, int h);
int win_alert (gui_t *gui, const char *text,
                   win_handler_t close, win_handler_t accept, win_handler_t decline);

#define WIN_ALERT_ACCEPT(gui, text, accept) win_alert(gui, text, NULL, accept, NULL)
#define WIN_ALERT_DECLINE(gui, text, decline) win_alert(gui, text, decline, NULL, decline)

int win_close_allert (gui_t *gui, pane_t *pane);

pane_t *win_new_console (gui_t *gui, prop_t *prop, int x, int y, int w, int h);
int win_con_append (pane_t *pane, const char *str, rgba_t textcolor);
int win_con_get_dim (pane_t *pane, int *w, int *h);
int win_con_print_at (pane_t *pane, int x, int y, const char *str, rgba_t textcolor);
int win_con_printline (pane_t *pane, int y,
                                    const char *str, rgba_t textcolor);
int win_con_printline_c (pane_t *pane, int y,
                                    const char *str, rgba_t textcolor);
void win_con_clear (pane_t *pane);

pane_t *win_new_progress (gui_t *gui, prop_t *prop, int x, int y, int w, int h);
int win_prog_set (pane_t *pane, const char *text, int percent);

