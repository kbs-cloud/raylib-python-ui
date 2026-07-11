#ifndef RAYLIB_PYTHON_UI_FRAME_H
#define RAYLIB_PYTHON_UI_FRAME_H

#include <stdbool.h>
#include <stdint.h>

#define LF_MAX_FRAMES   1024
#define LF_NAME_LEN      64
#define LF_POINT_LEN     16
#define LF_TEXT_LEN     512
#define LF_MAX_CHILDREN  64
#define LF_MAX_EVENTS    16
#define LFRAME_UIPARENT   0   /* UIParent is always frame id 0 */

/* Unset script ref (mirrors LUA_NOREF = -1 without including lua.h) */
#define LF_NOREF (-1)

typedef enum { LFT_FRAME=0, LFT_BUTTON, LFT_STATUSBAR, LFT_FONTSTRING, LFT_TEXTURE, LFT_EDITBOX } LFType;
typedef enum {
    LFS_BACKGROUND=0, LFS_LOW, LFS_MEDIUM, LFS_HIGH,
    LFS_DIALOG, LFS_FULLSCREEN, LFS_TOOLTIP, LFS_COUNT
} LFStrata;

typedef struct {
    char  point[LF_POINT_LEN];
    int   rel_id;                   /* frame id to anchor to (-1 = use parent) */
    char  rel_point[LF_POINT_LEN];
    float ox, oy;                   /* pixel offset */
    char  rel_name[LF_NAME_LEN];    /* name of frame to anchor to */
} LFAnchor;

#include "raylib.h"

typedef struct {
    char text[256];
    Color raw_color;
    bool is_default_color;
} LFTextRun;

#define LF_MAX_RUNS_PER_LINE 16
#define LF_MAX_LINES 16

typedef struct {
    LFTextRun runs[LF_MAX_RUNS_PER_LINE];
    int run_count;
    int measured_width;
} LFLineLayout;

typedef struct {
    LFLineLayout lines[LF_MAX_LINES];
    int line_count;
    int total_width;
    int total_height;
} LFTextLayout;

typedef struct {
    int      id;
    char     name[LF_NAME_LEN];
    LFType   type;
    LFStrata strata;
    LFStrata effective_strata;

    bool     has_anchor;
    LFAnchor anchor;
    float    w, h;

    /* computed top-left (screen coords, y=0 at top) */
    float    cx, cy;
    bool     anchored_ok;

    float    alpha;
    bool     has_bg;
    float    bg_r, bg_g, bg_b, bg_a;
    bool     has_border;
    float    border_r, border_g, border_b;
    float    border_w;
    bool     visible;
    bool     enable_mouse;

    /* text (Button, FontString) */
    char     text[LF_TEXT_LEN];
    int      font_size;
    float    text_r, text_g, text_b;
    int      justify;  /* 0=LEFT 1=CENTER 2=RIGHT */

    /* Caching layout */
    LFTextLayout layout_cache;
    bool     layout_dirty;

    /* StatusBar */
    float    bar_min, bar_max, bar_val;
    float    bar_r, bar_g, bar_b;
    float    bar_alpha;

    /* Button highlight color */
    float    hi_r, hi_g, hi_b, hi_a;

    /* Texture solid color */
    float    tex_r, tex_g, tex_b, tex_a;
    bool     has_tex_color;

    /* EditBox */
    int      cursor_pos;
    int      selection_start;
    int      selection_end;

    /* Script existence indicators */
    bool     has_OnLoad;
    bool     has_OnUpdate;
    bool     has_OnEvent;
    bool     has_OnClick;
    bool     has_OnEnter;
    bool     has_OnLeave;
    bool     has_OnDraw;

    /* Registered event names */
    char     events[LF_MAX_EVENTS][LF_NAME_LEN];
    int      event_count;

    /* Hierarchy */
    int      parent_id;
    int      children[LF_MAX_CHILDREN];
    int      child_count;

    bool     is_mouse_over;
    bool     destroyed;
    bool     movable;
    bool     resizable;
    bool     pre_escaped;

    bool     register_drag_left;
    bool     register_drag_right;

    bool     has_OnDragStart;
    bool     has_OnDragStop;
    bool     has_OnReceiveDrag;

    /* Layout persistence support */
    bool     user_placed;
    bool     has_default_anchor;
    LFAnchor default_anchor;
    float    default_w, default_h;
} LFrame;

typedef struct LFrameSystem LFrameSystem;

LFrameSystem *lframe_system_create(void);
void          lframe_system_destroy(LFrameSystem *s);

int     lframe_create(LFrameSystem *s, LFType type, const char *name, int parent_id);
int     lframe_get_capacity(const LFrameSystem *s);
LFrame *lframe_get(LFrameSystem *s, int id);
LFrame *lframe_get_by_name(LFrameSystem *s, const char *name);
void    lframe_destroy(LFrameSystem *s, int id);

void    lframe_start_moving(LFrameSystem *s, int id);
void    lframe_stop_moving_or_sizing(LFrameSystem *s);

void    lframe_start_drag(LFrameSystem *s, const char *type, const char *data, int icon_id, const char *name, float r, float g, float b, float a);
bool    lframe_get_drag_info(const LFrameSystem *s, const char **out_type, const char **out_data, int *out_icon_id, const char **out_name, float *out_r, float *out_g, float *out_b, float *out_a, int *out_src_fid);
void    lframe_clear_drag(LFrameSystem *s);

void    lframe_layout_load(LFrameSystem *s, const char *path);
void    lframe_layout_save(const LFrameSystem *s, const char *path);
void    lframe_update_saved_layout(LFrameSystem *s, const LFrame *f);
void    lframe_apply_saved_layout(LFrameSystem *s, LFrame *f);
void    lframe_remove_saved_layout(LFrameSystem *s, const char *name);

/* Called each frame in the 2D draw pass */
void lframe_resolve_layout(LFrameSystem *s, float screen_w, float screen_h);
void lframe_draw(LFrameSystem *s, void (*on_draw_cb)(int fid, void *userdata), void *userdata);

typedef struct {
    void (*on_click)(int fid, const char *button, void *userdata);
    void (*on_drag_start)(int fid, const char *button, void *userdata);
    void (*on_drag_stop)(int fid, const char *type, const char *data, int src_fid, void *userdata);
    void (*on_receive_drag)(int fid, const char *type, const char *data, int src_fid, void *userdata);
    void (*on_enter)(int fid, void *userdata);
    void (*on_leave)(int fid, void *userdata);
} LFrameInputCallbacks;

bool lframe_handle_input(LFrameSystem *s, const LFrameInputCallbacks *cbs, void *userdata);
void lframe_handle_keyboard(LFrameSystem *s);
void lframe_set_focus(LFrameSystem *s, int id);
int  lframe_get_focus(const LFrameSystem *s);
void lframe_set_select_frame(LFrameSystem *s, int id);
int  lframe_get_select_frame(const LFrameSystem *s);

bool    lframe_is_mouse_over_any(const LFrameSystem *s);

/* -------------------------------------------------------------------------
 * Test / diagnostic helpers
 * ---------------------------------------------------------------------- */

/* True if the frame and ALL of its ancestors are visible (not destroyed). */
bool lframe_is_visible_in_hierarchy(const LFrameSystem *s, int id);

/* Fill `out_ids` with the IDs of frames that would be drawn this frame,
 * sorted in draw order (ascending strata, then ascending id as tiebreaker).
 * Returns the count written (capped at max_ids).
 * Does NOT perform any actual drawing. */
int lframe_collect_draw_order(const LFrameSystem *s, int *out_ids, int max_ids);

/* Return the number of frames currently alive (not destroyed). */
int lframe_count_alive(const LFrameSystem *s);

/* Color tag helper for strings */
void strip_color_codes(const char *src, char *dst, int dst_max_size);

#endif /* RAYLIB_PYTHON_UI_FRAME_H */
