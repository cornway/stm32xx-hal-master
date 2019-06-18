#include <stdint.h>
#include <gfx.h>
#include <misc_utils.h>
#include <debug.h>

typedef enum {
    GUISFX_FIRST = 0,
    GUISFX_OPEN = GUISFX_FIRST,
    GUISFX_CLOSE,
    GUISFX_CANTDOTHIS,
    GUISFX_OTHER,
    GUISFX_MAX,
} gui_sfx_std_type_t;

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

typedef struct prop_s {
    pix_t fcolor, bcolor;
    d_bool ispad;
    d_bool user_draw;
    gui_sfx_t sfx;
} prop_t;

typedef struct component_s {
    struct component_s *next;
    dim_t dim;
    struct pane_s *parent;
    uint32_t focus: 1,
             type:  8,
             repaint: 1,
             userdraw: 1,
             glow:     8,
             ispad:      1,
             showname:  1,
             reserved: 11;

    pix_t bcolor, fcolor;

    void *ctxt;
    void *user;

    comp_handler_t act, release;
    comp_handler_t draw;

    uint16_t text_offset;
    uint16_t text_size;
    uint16_t text_index;
    gui_sfx_t sfx;
    char name_text[1];
} component_t;

typedef struct pane_s {
    struct pane_s *next;
    dim_t dim;
    struct gui_s *parent;
    uint32_t focus: 1,
             type:  8,
             repaint: 1,
             iswin:    1,
             reserved: 21;

    pix_t bcolor, fcolor;

    void *ctxt;
    void *user;

    component_t *onfocus;
    
    component_t *head, *tail;
    struct pane_s *prevselected;
    char name[1];
} pane_t;

typedef struct gui_s {
    void *dummy;
    dim_t dim;
    pane_t *head, *tail;
    pane_t *selected;
    void *user;
    void (*alloc_sfx) (int *, gui_sfx_std_type_t);
    void (*start_sfx) (int);
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

d_bool dim_check (const dim_t *d, const point_t *p);
void dim_place (dim_t *d, const dim_t *s);
void dim_tolocal (dim_t *d, const dim_t *s);
void dim_get_origin (point_t *d, const dim_t *s);
void dim_set_origin (dim_t *d, const point_t *s);

void gui_init (gui_t *gui, int x, int y, int w, int h);
pane_t *gui_get_pane (const char *name);
void gui_set_pane (gui_t *gui, pane_t *pane);
void gui_set_panexy (gui_t *gui, pane_t *pane, int x, int y, int w, int h);

component_t *gui_get_comp (const char *name, const char *text);
void gui_set_comp (pane_t *pane, component_t *c, int x, int y, int w, int h);
void gui_set_prop (component_t *c, prop_t *prop);

void gui_text (component_t *com, const char *text, int x, int y);
void gui_printxy (component_t *com, int x, int y, const char *fmt, ...) PRINTF_ATTR(4, 5);
void gui_apendxy (component_t *com, int x, int y, const char *fmt, ...) PRINTF_ATTR(4, 5);

#define gui_print(com, args...) gui_printxy(com, 0, 0, args)
#define gui_print(com, args...) gui_apendxy(com, 0, 0, args)

void gui_com_clear (component_t *com);
void gui_draw (gui_t *gui);
void gui_resp (gui_t *gui, component_t *com, gevt_t *evt);
void gui_wake (gui_t *gui, const char *name);
pane_t *gui_search_pane (gui_t *gui, const char *name);
component_t *gui_search_com (pane_t *pane, const char *name);
void gui_select_pane (gui_t *gui, pane_t *pane);
void gui_release_pane (gui_t *gui, pane_t *pane);


#define COLOR_WHITE (GFX_RGBA(0xff, 0xff, 0xff, 0xff))
#define COLOR_BLACK (GFX_RGBA(0x00, 0x00, 0x00, 0xff))
#define COLOR_RED (GFX_RGBA(0xff, 0x00, 0x00, 0xff))
#define COLOR_GREEN (GFX_RGBA(0x00, 0xff, 0x00, 0xff))
#define COLOR_BLUE (GFX_RGBA(0x00, 0x00, 0xff, 0xff))
#define COLOR_GREY (GFX_RGBA(0x80, 0x80, 0x80, 0xff))


pane_t *win_new_allert
    (gui_t *gui, int w, int h,
     comp_handler_t close, comp_handler_t accept, comp_handler_t decline);

int win_alert (gui_t *gui, const char *text);
int win_close_allert (gui_t *gui, pane_t *pane);

int boot_audio_open_wave (const char *name);
int boot_audio_play_wave (int hdl, uint8_t volume);
int boot_audio_stop_wave (int hdl);
void boot_audio_release_wave (int hdl);


