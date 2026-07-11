/* lua_frame.c — Game UI frame widget system backed by a fixed pool.
 * Rendering and anchor resolution live here; Lua binding glue is in lua_api.c. */

#include "ui/python_api_internal.h"

float g_ui_scale = 1.0f;

static inline void get_frame_scaled_size(const LFrame *f, float *w, float *h) {
    if (!f) {
        *w = 0.0f;
        *h = 0.0f;
    } else if (f->id == LFRAME_UIPARENT) {
        *w = f->w;
        *h = f->h;
    } else {
        *w = f->w * g_ui_scale;
        *h = f->h * g_ui_scale;
    }
}

#include "raylib.h"

#ifdef RAYLIB_LUA_UI_TEST
#undef GetMousePosition
#undef IsMouseButtonPressed
#undef IsKeyDown
#undef IsKeyPressed
#undef GetCharPressed
#undef GetClipboardText
#undef SetClipboardText
#define GetMousePosition() test_GetMousePosition()
#define IsMouseButtonPressed(btn) test_IsMouseButtonPressed(btn)
#define IsKeyDown(key) test_IsKeyDown(key)
#define IsKeyPressed(key) test_IsKeyPressed(key)
#define GetCharPressed() test_GetCharPressed()
#define GetClipboardText() test_GetClipboardText()
#define SetClipboardText(txt) test_SetClipboardText(txt)

extern Vector2 test_GetMousePosition(void);
extern bool test_IsMouseButtonPressed(int button);
extern bool test_IsKeyDown(int key);
extern bool test_IsKeyPressed(int key);
extern int test_GetCharPressed(void);
extern const char* test_GetClipboardText(void);
extern void test_SetClipboardText(const char* text);
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* -------------------------------------------------------------------------
 * Internal pool
 * ---------------------------------------------------------------------- */
#define LF_MAX_SAVED_LAYOUTS 128

typedef struct {
    char  name[LF_NAME_LEN];
    char  point[LF_POINT_LEN];
    char  rel_point[LF_POINT_LEN];
    char  rel_name[LF_NAME_LEN];
    float ox, oy;
    float w, h;
    bool  has_saved;
} LFrameSavedLayout;

struct LFrameSystem {
    LFrame **frames;
    int    capacity;
    int    count; /* high-water mark */
    int    drag_frame_id;
    Vector2 drag_mouse_offset;
    int    resize_frame_id;
    Vector2 resize_start_size;
    Vector2 resize_start_mouse;
    int    focus_frame_id;
    int    select_frame_id;

    /* Drag and drop state */
    int     potential_drag_frame_id;
    int     potential_drag_button;
    Vector2 potential_drag_start_pos;

    bool    drag_active;
    int     drag_src_frame_id;
    char    drag_type[64];
    char    drag_data[256];
    int     drag_icon_id;
    char    drag_name[64];
    float   drag_color_r, drag_color_g, drag_color_b, drag_color_a;

    LFrame **draw_order;
    int    draw_order_capacity;

    LFrame **input_order;
    int    input_order_capacity;

    LFrameSavedLayout saved_layouts[LF_MAX_SAVED_LAYOUTS];
    int               saved_layouts_count;
};

/* -------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */
static Color rgba(float r, float g, float b, float a) {
    int ri = (int)(r * 255.0f); if (ri < 0) ri = 0; else if (ri > 255) ri = 255;
    int gi = (int)(g * 255.0f); if (gi < 0) gi = 0; else if (gi > 255) gi = 255;
    int bi = (int)(b * 255.0f); if (bi < 0) bi = 0; else if (bi > 255) bi = 255;
    int ai = (int)(a * 255.0f); if (ai < 0) ai = 0; else if (ai > 255) ai = 255;
    return (Color){ (unsigned char)ri, (unsigned char)gi,
                    (unsigned char)bi, (unsigned char)ai };
}

void strip_color_codes(const char *src, char *dst, int dst_max_size) {
    int s = 0;
    int d = 0;
    while (src[s] && d < dst_max_size - 1) {
        if (src[s] == '|') {
            if (src[s + 1] == '|') {
                dst[d++] = '|';
                s += 2;
            } else if ((src[s + 1] == 'c' || src[s + 1] == 'C') && src[s + 2] != '\0') {
                // Check if we have 8 hex characters following
                bool is_hex = true;
                for (int i = 0; i < 8; i++) {
                    char c = src[s + 2 + i];
                    if (!c) {
                        is_hex = false;
                        break;
                    }
                    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                        is_hex = false;
                        break;
                    }
                }
                if (is_hex) {
                    s += 10; // skip |c + 8 hex digits
                } else {
                    dst[d++] = src[s++];
                }
            } else if (src[s + 1] == 'r' || src[s + 1] == 'R') {
                s += 2; // skip |r
            } else {
                dst[d++] = src[s++];
            }
        } else {
            dst[d++] = src[s++];
        }
    }
    dst[d] = '\0';
}

#include <math.h>

#define MAX_WRAPPED_LINES 64
typedef struct {
    int start;
    int len;
    float width;
} WrappedLine;

extern bool g_chat_active;

int lframe_wrap_text(const char *text, int font_size, float max_w, WrappedLine *lines, int max_lines) {
    if (!text || max_w <= 0.0f || max_lines <= 0) return 0;
    int line_count = 0;
    int text_len = (int)strlen(text);
    int i = 0;
    
    int scaled_font = (int)(font_size * g_ui_scale);
    float scaled_max_w = max_w * g_ui_scale;
    
    while (i < text_len && line_count < max_lines) {
        if (text[i] == '\n') {
            lines[line_count++] = (WrappedLine){ i, 0, 0.0f };
            i++;
            continue;
        }
        
        int line_start = i;
        int last_space_idx = -1;
        int best_len = 0;
        float best_width = 0.0f;
        
        int curr = i;
        while (curr < text_len && text[curr] != '\n') {
            if (text[curr] == ' ') {
                last_space_idx = curr;
            }
            
            int prefix_len = curr - line_start + 1;
            char temp[512];
            if (prefix_len >= 512) prefix_len = 511;
            memcpy(temp, &text[line_start], (size_t)prefix_len);
            temp[prefix_len] = '\0';
            
            float w = (float)MeasureText(temp, scaled_font);
            if (w <= scaled_max_w) {
                best_len = prefix_len;
                best_width = w;
                curr++;
            } else {
                break;
            }
        }
        
        if (best_len == 0) {
            best_len = 1;
            char temp[2] = { text[line_start], '\0' };
            best_width = (float)MeasureText(temp, scaled_font);
        } else if (curr < text_len && text[curr] != '\n') {
            if (last_space_idx > line_start) {
                best_len = last_space_idx - line_start;
                char temp[512];
                if (best_len >= 512) best_len = 511;
                memcpy(temp, &text[line_start], (size_t)best_len);
                temp[best_len] = '\0';
                best_width = (float)MeasureText(temp, scaled_font);
            }
        }
        
        lines[line_count++] = (WrappedLine){ line_start, best_len, best_width };
        i = line_start + best_len;
        
        if (i < text_len && text[i] == ' ') {
            i++;
        }
    }
    
    if (text_len > 0 && text[text_len - 1] == '\n' && line_count < max_lines) {
        lines[line_count++] = (WrappedLine){ text_len, 0, 0.0f };
    }
    
    return line_count;
}

void lframe_get_cursor_line_col(const char *text, int pos, const WrappedLine *lines, int line_count, int *out_line, int *out_col) {
    (void)text;
    *out_line = 0;
    *out_col = 0;
    for (int l = 0; l < line_count; l++) {
        int l_start = lines[l].start;
        int l_end = lines[l].start + lines[l].len;
        if (pos >= l_start && pos <= l_end) {
            *out_line = l;
            *out_col = pos - l_start;
            return;
        }
    }
    if (line_count > 0) {
        *out_line = line_count - 1;
        *out_col = lines[line_count - 1].len;
    }
}

int lframe_get_char_index_at(const LFrame *f, Vector2 mp) {
    WrappedLine lines[MAX_WRAPPED_LINES];
    float w, h;
    get_frame_scaled_size(f, &w, &h);
    float wrap_w = w / g_ui_scale - 16.0f;
    if (wrap_w < 10.0f) wrap_w = 10.0f;
    int line_count = lframe_wrap_text(f->text, f->font_size, wrap_w, lines, MAX_WRAPPED_LINES);
    if (line_count == 0) return 0;
    
    float rx = mp.x - (f->cx + 8 * g_ui_scale);
    float ry = mp.y - (f->cy + 6 * g_ui_scale);
    
    int line_height = (int)((f->font_size + 2) * g_ui_scale);
    int line_idx = (int)(ry / line_height);
    if (line_idx < 0) line_idx = 0;
    if (line_idx >= line_count) line_idx = line_count - 1;
    
    int best_col = 0;
    float min_dist = 999999.0f;
    for (int c = 0; c <= lines[line_idx].len; c++) {
        char temp[512];
        int len = c;
        if (len >= 512) len = 511;
        memcpy(temp, &f->text[lines[line_idx].start], (size_t)len);
        temp[len] = '\0';
        float w_char = (float)MeasureText(temp, (int)(f->font_size * g_ui_scale));
        float dist = (float)fabs(w_char - rx);
        if (dist < min_dist) {
            min_dist = dist;
            best_col = c;
        }
    }
    
    return lines[line_idx].start + best_col;
}

static void delete_text_range(char *text, int start, int end) {
    int len = (int)strlen(text);
    if (start < 0) start = 0;
    if (end > len) end = len;
    if (start >= end) return;
    memmove(&text[start], &text[end], (size_t)(len - end + 1));
}

static void insert_text(char *text, int max_len, int pos, const char *ins) {
    int len = (int)strlen(text);
    int ins_len = (int)strlen(ins);
    if (pos < 0) pos = 0;
    if (pos > len) pos = len;
    if (len + ins_len >= max_len) {
        ins_len = max_len - len - 1;
        if (ins_len <= 0) return;
    }
    memmove(&text[pos + ins_len], &text[pos], (size_t)(len - pos + 1));
    memcpy(&text[pos], ins, (size_t)ins_len);
}

void lframe_set_focus(LFrameSystem *s, int id) {
    if (s) {
        s->focus_frame_id = id;
    }
}

int lframe_get_focus(const LFrameSystem *s) {
    return s ? s->focus_frame_id : -1;
}

void lframe_set_select_frame(LFrameSystem *s, int id) {
    if (s) {
        s->select_frame_id = id;
    }
}

int lframe_get_select_frame(const LFrameSystem *s) {
    return s ? s->select_frame_id : -1;
}

void lframe_handle_keyboard(LFrameSystem *s) {
    if (!s || s->focus_frame_id < 0) return;
    LFrame *f = lframe_get(s, s->focus_frame_id);
    if (!f || f->destroyed || !f->visible) {
        s->focus_frame_id = -1;
        return;
    }
    
    char old_text[LF_TEXT_LEN];
    strncpy(old_text, f->text, sizeof(old_text) - 1);
    old_text[sizeof(old_text) - 1] = '\0';
    
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    
    WrappedLine lines[MAX_WRAPPED_LINES];
    int line_count = lframe_wrap_text(f->text, f->font_size, f->w - 16, lines, MAX_WRAPPED_LINES);
    
    int cur_line = 0, cur_col = 0;
    lframe_get_cursor_line_col(f->text, f->cursor_pos, lines, line_count, &cur_line, &cur_col);
    
    bool cursor_moved = false;
    int next_pos = f->cursor_pos;
    
    if (ctrl) {
        if (IsKeyPressed(KEY_C)) {
            if (f->selection_start != f->selection_end) {
                int start = f->selection_start < f->selection_end ? f->selection_start : f->selection_end;
                int end = f->selection_start > f->selection_end ? f->selection_start : f->selection_end;
                int len = end - start;
                char temp[512];
                if (len >= 512) len = 511;
                memcpy(temp, &f->text[start], (size_t)len);
                temp[len] = '\0';
                SetClipboardText(temp);
            }
            while (GetCharPressed() > 0);
            return;
        }
        else if (IsKeyPressed(KEY_X)) {
            if (f->selection_start != f->selection_end) {
                int start = f->selection_start < f->selection_end ? f->selection_start : f->selection_end;
                int end = f->selection_start > f->selection_end ? f->selection_start : f->selection_end;
                int len = end - start;
                char temp[512];
                if (len >= 512) len = 511;
                memcpy(temp, &f->text[start], (size_t)len);
                temp[len] = '\0';
                SetClipboardText(temp);
                
                delete_text_range(f->text, start, end);
                f->cursor_pos = start;
                f->selection_start = start;
                f->selection_end = start;
            }
            while (GetCharPressed() > 0);
            return;
        }
        else if (IsKeyPressed(KEY_V)) {
            const char *clip = GetClipboardText();
            if (clip && clip[0] != '\0') {
                if (f->selection_start != f->selection_end) {
                    int start = f->selection_start < f->selection_end ? f->selection_start : f->selection_end;
                    int end = f->selection_start > f->selection_end ? f->selection_start : f->selection_end;
                    delete_text_range(f->text, start, end);
                    f->cursor_pos = start;
                }
                char clean[512];
                int c_idx = 0;
                for (int i = 0; clip[i] != '\0' && c_idx < 511; i++) {
                    if ((clip[i] >= 32 && clip[i] <= 126) || clip[i] == '\n') {
                        clean[c_idx++] = clip[i];
                    }
                }
                clean[c_idx] = '\0';
                
                insert_text(f->text, LF_TEXT_LEN, f->cursor_pos, clean);
                f->cursor_pos += (int)strlen(clean);
                f->selection_start = f->cursor_pos;
                f->selection_end = f->cursor_pos;
            }
            while (GetCharPressed() > 0);
            return;
        }
    }
    
    if (IsKeyPressed(KEY_LEFT)) {
        if (next_pos > 0) {
            next_pos--;
            cursor_moved = true;
        }
    }
    else if (IsKeyPressed(KEY_RIGHT)) {
        if (next_pos < (int)strlen(f->text)) {
            next_pos++;
            cursor_moved = true;
        }
    }
    else if (IsKeyPressed(KEY_UP)) {
        if (cur_line > 0) {
            int prev_line = cur_line - 1;
            int col = cur_col;
            if (col > lines[prev_line].len) col = lines[prev_line].len;
            next_pos = lines[prev_line].start + col;
            cursor_moved = true;
        }
    }
    else if (IsKeyPressed(KEY_DOWN)) {
        if (cur_line < line_count - 1) {
            int next_line = cur_line + 1;
            int col = cur_col;
            if (col > lines[next_line].len) col = lines[next_line].len;
            next_pos = lines[next_line].start + col;
            cursor_moved = true;
        }
    }
    else if (IsKeyPressed(KEY_HOME)) {
        next_pos = lines[cur_line].start;
        cursor_moved = true;
    }
    else if (IsKeyPressed(KEY_END)) {
        next_pos = lines[cur_line].start + lines[cur_line].len;
        cursor_moved = true;
    }
    
    if (cursor_moved) {
        f->cursor_pos = next_pos;
        if (shift) {
            f->selection_end = next_pos;
        } else {
            f->selection_start = next_pos;
            f->selection_end = next_pos;
        }
    }
    
    if (IsKeyPressed(KEY_BACKSPACE)) {
        if (f->selection_start != f->selection_end) {
            int start = f->selection_start < f->selection_end ? f->selection_start : f->selection_end;
            int end = f->selection_start > f->selection_end ? f->selection_start : f->selection_end;
            delete_text_range(f->text, start, end);
            f->cursor_pos = start;
            f->selection_start = start;
            f->selection_end = start;
        } else if (f->cursor_pos > 0) {
            delete_text_range(f->text, f->cursor_pos - 1, f->cursor_pos);
            f->cursor_pos--;
            f->selection_start = f->cursor_pos;
            f->selection_end = f->cursor_pos;
        }
    }
    else if (IsKeyPressed(KEY_DELETE)) {
        if (f->selection_start != f->selection_end) {
            int start = f->selection_start < f->selection_end ? f->selection_start : f->selection_end;
            int end = f->selection_start > f->selection_end ? f->selection_start : f->selection_end;
            delete_text_range(f->text, start, end);
            f->cursor_pos = start;
            f->selection_start = start;
            f->selection_end = start;
        } else if (f->cursor_pos < (int)strlen(f->text)) {
            delete_text_range(f->text, f->cursor_pos, f->cursor_pos + 1);
        }
    }
    else if (IsKeyPressed(KEY_ESCAPE)) {
        s->focus_frame_id = -1;
        g_chat_active = false;
        return;
    }
    
    int char_pressed = GetCharPressed();
    while (char_pressed > 0) {
        if ((char_pressed >= 32 && char_pressed <= 126) || char_pressed == '\n') {
            if (f->selection_start != f->selection_end) {
                int start = f->selection_start < f->selection_end ? f->selection_start : f->selection_end;
                int end = f->selection_start > f->selection_end ? f->selection_start : f->selection_end;
                delete_text_range(f->text, start, end);
                f->cursor_pos = start;
            }
            
            char ins_str[2] = { (char)char_pressed, '\0' };
            insert_text(f->text, LF_TEXT_LEN, f->cursor_pos, ins_str);
            f->cursor_pos++;
            f->selection_start = f->cursor_pos;
            f->selection_end = f->cursor_pos;
        }
        char_pressed = GetCharPressed();
    }
    if (strcmp(old_text, f->text) != 0) {
        f->layout_dirty = true;
    }
}



/* Compute the (px,py) of a named anchor point on a rect whose top-left is
 * (cx,cy) and whose size is (w,h). */
static void anchor_point(const char *pt, float cx, float cy, float w, float h,
                          float *px, float *py)
{
    if (!pt || pt[0] == '\0') {
        *px = cx; *py = cy; return;
    }
    if      (strcmp(pt, "TOPLEFT")     == 0) { *px = cx;       *py = cy; }
    else if (strcmp(pt, "TOP")         == 0) { *px = cx+w/2;   *py = cy; }
    else if (strcmp(pt, "TOPRIGHT")    == 0) { *px = cx+w;     *py = cy; }
    else if (strcmp(pt, "LEFT")        == 0) { *px = cx;       *py = cy+h/2; }
    else if (strcmp(pt, "CENTER")      == 0) { *px = cx+w/2;   *py = cy+h/2; }
    else if (strcmp(pt, "RIGHT")       == 0) { *px = cx+w;     *py = cy+h/2; }
    else if (strcmp(pt, "BOTTOMLEFT")  == 0) { *px = cx;       *py = cy+h; }
    else if (strcmp(pt, "BOTTOM")      == 0) { *px = cx+w/2;   *py = cy+h; }
    else if (strcmp(pt, "BOTTOMRIGHT") == 0) { *px = cx+w;     *py = cy+h; }
    else                                      { *px = cx;       *py = cy; } /* fallback */
}

/* -------------------------------------------------------------------------
 * System lifecycle
 * ---------------------------------------------------------------------- */
LFrameSystem *lframe_system_create(void) {
    LFrameSystem *s = (LFrameSystem *)RL_CALLOC(1, sizeof(LFrameSystem));
    if (!s) return NULL;

    s->capacity = 128;
    s->frames = (LFrame **)RL_CALLOC((size_t)s->capacity, sizeof(LFrame *));
    if (!s->frames) {
        RL_FREE(s);
        return NULL;
    }

    /* Slot 0 = UIParent: always present, full-screen, no-op */
    s->frames[0] = (LFrame *)RL_CALLOC(1, sizeof(LFrame));
    if (!s->frames[0]) {
        RL_FREE(s->frames);
        RL_FREE(s);
        return NULL;
    }

    LFrame *ui = s->frames[0];
    ui->destroyed  = false;
    ui->id         = 0;
    ui->type       = LFT_FRAME;
    ui->strata     = LFS_BACKGROUND;
    ui->visible    = true;
    ui->alpha      = 1.0f;
    ui->parent_id  = -1;
    ui->font_size  = 14;
    ui->text_r     = 1.0f; ui->text_g = 1.0f; ui->text_b = 1.0f;
    ui->bar_max    = 1.0f;
    ui->hi_a       = 0.3f;
    ui->anchored_ok = true; /* will be set to screen size each tick */
    strncpy(ui->name, "UIParent", LF_NAME_LEN - 1);

    /* All script refs = LF_NOREF */

    s->count = 1;
    s->drag_frame_id = -1;
    s->resize_frame_id = -1;
    s->focus_frame_id = -1;
    s->select_frame_id = -1;

    s->potential_drag_frame_id = -1;
    s->potential_drag_button = -1;
    s->potential_drag_start_pos = (Vector2){0, 0};
    s->drag_active = false;
    s->drag_src_frame_id = -1;
    s->drag_icon_id = -1;
    s->drag_name[0] = '\0';
    s->drag_color_r = 0.0f;
    s->drag_color_g = 0.0f;
    s->drag_color_b = 0.0f;
    s->drag_color_a = 0.0f;

    return s;
}

void lframe_layout_load(LFrameSystem *s, const char *path) {
    if (!s) return;
    s->saved_layouts_count = 0;
    memset(s->saved_layouts, 0, sizeof(s->saved_layouts));

    FILE *f = fopen(path, "r");
    if (!f) {
        TraceLog(LOG_INFO, "[LFrame] Layout file not found or couldn't be opened: %s", path);
        return;
    }

    char line[256];
    LFrameSavedLayout *curr = NULL;

    while (fgets(line, sizeof(line), f)) {
        // Strip trailing whitespace/newlines
        int len = (int)strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n' || line[len - 1] == ' ' || line[len - 1] == '\t')) {
            line[--len] = '\0';
        }

        // Skip comments/empty lines
        if (line[0] == '#' || line[0] == '\0') continue;

        // Section header like [FrameName]
        if (line[0] == '[' && line[len - 1] == ']') {
            if (s->saved_layouts_count >= LF_MAX_SAVED_LAYOUTS) {
                curr = NULL;
                continue;
            }
            curr = &s->saved_layouts[s->saved_layouts_count++];
            memset(curr, 0, sizeof(LFrameSavedLayout));
            
            // Extract frame name inside [ ]
            int name_len = len - 2;
            if (name_len > LF_NAME_LEN - 1) name_len = LF_NAME_LEN - 1;
            memcpy(curr->name, &line[1], (size_t)name_len);
            curr->name[name_len] = '\0';
            curr->has_saved = true;
            continue;
        }

        if (!curr) continue;

        char key[64], val[128];
        if (sscanf(line, "%63[^=]=%127s", key, val) == 2) {
            if (strcmp(key, "point") == 0) {
                strncpy(curr->point, val, LF_POINT_LEN - 1);
            } else if (strcmp(key, "rel_point") == 0) {
                strncpy(curr->rel_point, val, LF_POINT_LEN - 1);
            } else if (strcmp(key, "rel_name") == 0) {
                strncpy(curr->rel_name, val, LF_NAME_LEN - 1);
            } else if (strcmp(key, "ox") == 0) {
                curr->ox = (float)atof(val);
            } else if (strcmp(key, "oy") == 0) {
                curr->oy = (float)atof(val);
            } else if (strcmp(key, "w") == 0) {
                curr->w = (float)atof(val);
            } else if (strcmp(key, "h") == 0) {
                curr->h = (float)atof(val);
            }
        }
    }
    fclose(f);
    TraceLog(LOG_INFO, "[LFrame] Loaded %d saved frame layouts from %s", s->saved_layouts_count, path);
}

void lframe_layout_save(const LFrameSystem *s, const char *path) {
    if (!s) return;
    FILE *f = fopen(path, "w");
    if (!f) {
        TraceLog(LOG_WARNING, "[LFrame] Failed to open layout file for saving: %s", path);
        return;
    }

    fprintf(f, "# Raylib Python UI Layout Saved Variables\n\n");
    for (int i = 0; i < s->saved_layouts_count; i++) {
        const LFrameSavedLayout *lay = &s->saved_layouts[i];
        if (!lay->has_saved) continue;
        fprintf(f, "[%s]\n", lay->name);
        fprintf(f, "point=%s\n", lay->point);
        fprintf(f, "rel_point=%s\n", lay->rel_point);
        fprintf(f, "rel_name=%s\n", lay->rel_name);
        fprintf(f, "ox=%.2f\n", lay->ox);
        fprintf(f, "oy=%.2f\n", lay->oy);
        fprintf(f, "w=%.2f\n", lay->w);
        fprintf(f, "h=%.2f\n", lay->h);
        fprintf(f, "\n");
    }
    fclose(f);
}

void lframe_update_saved_layout(LFrameSystem *s, const LFrame *f) {
    if (!s || !f || !f->name[0] || f->destroyed) return;

    LFrameSavedLayout *lay = NULL;
    for (int i = 0; i < s->saved_layouts_count; i++) {
        if (strcmp(s->saved_layouts[i].name, f->name) == 0) {
            lay = &s->saved_layouts[i];
            break;
        }
    }

    if (!lay) {
        if (s->saved_layouts_count < LF_MAX_SAVED_LAYOUTS) {
            lay = &s->saved_layouts[s->saved_layouts_count++];
            memset(lay, 0, sizeof(LFrameSavedLayout));
            strncpy(lay->name, f->name, LF_NAME_LEN - 1);
        } else {
            TraceLog(LOG_WARNING, "[LFrame] Max saved layouts reached. Cannot save layout for '%s'", f->name);
            return;
        }
    }

    lay->has_saved = true;
    lay->w = f->w;
    lay->h = f->h;
    if (f->has_anchor) {
        strncpy(lay->point, f->anchor.point, LF_POINT_LEN - 1);
        strncpy(lay->rel_point, f->anchor.rel_point, LF_POINT_LEN - 1);
        strncpy(lay->rel_name, f->anchor.rel_name, LF_NAME_LEN - 1);
        lay->ox = f->anchor.ox;
        lay->oy = f->anchor.oy;
    } else {
        strncpy(lay->point, "TOPLEFT", LF_POINT_LEN - 1);
        strncpy(lay->rel_point, "TOPLEFT", LF_POINT_LEN - 1);
        if (f->parent_id >= 0 && f->parent_id < s->capacity && s->frames[f->parent_id] && s->frames[f->parent_id]->name[0]) {
            strncpy(lay->rel_name, s->frames[f->parent_id]->name, LF_NAME_LEN - 1);
        } else {
            strcpy(lay->rel_name, "UIParent");
        }
        lay->ox = f->cx;
        lay->oy = -f->cy;
    }
}

void lframe_apply_saved_layout(LFrameSystem *s, LFrame *f) {
    if (!s || !f || !f->name[0]) return;

    const LFrameSavedLayout *lay = NULL;
    for (int i = 0; i < s->saved_layouts_count; i++) {
        if (s->saved_layouts[i].has_saved && strcmp(s->saved_layouts[i].name, f->name) == 0) {
            lay = &s->saved_layouts[i];
            break;
        }
    }

    if (!lay) return;

    f->w = lay->w;
    f->h = lay->h;
    f->has_anchor = true;
    strncpy(f->anchor.point, lay->point, LF_POINT_LEN - 1);
    strncpy(f->anchor.rel_point, lay->rel_point, LF_POINT_LEN - 1);
    strncpy(f->anchor.rel_name, lay->rel_name, LF_NAME_LEN - 1);
    f->anchor.ox = lay->ox;
    f->anchor.oy = lay->oy;

    f->anchor.rel_id = -1;
    if (strcmp(lay->rel_name, "UIParent") == 0) {
        f->anchor.rel_id = LFRAME_UIPARENT;
    } else {
        LFrame *rel_f = lframe_get_by_name(s, lay->rel_name);
        if (rel_f) f->anchor.rel_id = rel_f->id;
    }

    f->user_placed = true;
    f->anchored_ok = false;
}

void lframe_remove_saved_layout(LFrameSystem *s, const char *name) {
    if (!s || !name || !name[0]) return;
    for (int i = 0; i < s->saved_layouts_count; i++) {
        if (strcmp(s->saved_layouts[i].name, name) == 0) {
            s->saved_layouts[i].has_saved = false;
            break;
        }
    }
}

void lframe_system_destroy(LFrameSystem *s) {
    if (!s) return;
    for (int i = 0; i < s->capacity; i++) {
        if (s->frames[i]) {
            RL_FREE(s->frames[i]);
        }
    }
    RL_FREE(s->frames);
    if (s->draw_order) RL_FREE(s->draw_order);
    if (s->input_order) RL_FREE(s->input_order);
    RL_FREE(s);
}

/* -------------------------------------------------------------------------
 * Frame creation / lookup / destruction
 * ---------------------------------------------------------------------- */
int lframe_create(LFrameSystem *s, LFType type, const char *name, int parent_id) {
    if (!s) return -1;

    /* Find the first slot at index >= 1.
     * Either s->frames[i] is NULL (unallocated), or it is allocated but destroyed. */
    int fid = -1;
    for (int i = 1; i < s->capacity; i++) {
        if (!s->frames[i] || s->frames[i]->destroyed) {
            fid = i;
            break;
        }
    }

    /* If no slot is found, grow the array */
    if (fid < 0) {
        int new_cap = s->capacity * 2;
        LFrame **new_arr = (LFrame **)RL_REALLOC(s->frames, (size_t)new_cap * sizeof(LFrame *));
        if (!new_arr) {
            TraceLog(LOG_WARNING, "[LFrame] Realloc failed! Cannot create frame '%s'", name ? name : "");
            return -1;
        }
        s->frames = new_arr;
        memset(s->frames + s->capacity, 0, (size_t)(new_cap - s->capacity) * sizeof(LFrame *));
        fid = s->capacity;
        s->capacity = new_cap;
    }

    /* Allocate frame struct if it is NULL */
    if (!s->frames[fid]) {
        s->frames[fid] = (LFrame *)RL_CALLOC(1, sizeof(LFrame));
        if (!s->frames[fid]) {
            TraceLog(LOG_WARNING, "[LFrame] Calloc failed! Cannot create frame '%s'", name ? name : "");
            return -1;
        }
    }

    LFrame *f = s->frames[fid];
    memset(f, 0, sizeof(LFrame));

    f->id         = fid;
    f->destroyed  = false;
    f->visible    = true;
    f->pre_escaped = false;
    f->alpha      = 1.0f;
    f->font_size  = 14;
    f->text_r     = 1.0f; f->text_g = 1.0f; f->text_b = 1.0f;
    f->layout_dirty = true;
    f->bar_max    = 1.0f;
    f->hi_a       = 0.3f;
    f->type       = type;
    f->strata     = LFS_MEDIUM;
    f->effective_strata = LFS_MEDIUM;
    f->parent_id  = (parent_id >= 0 && parent_id < s->capacity) ? parent_id : LFRAME_UIPARENT;

    f->register_drag_left = false;
    f->register_drag_right = false;

    if (name && name[0]) {
        strncpy(f->name, name, LF_NAME_LEN - 1);
        lframe_apply_saved_layout(s, f);
    }

    /* Register child in parent */
    if (f->parent_id >= 0 && f->parent_id < s->capacity) {
        LFrame *par = s->frames[f->parent_id];
        if (par && !par->destroyed && par->child_count < LF_MAX_CHILDREN)
            par->children[par->child_count++] = fid;
    }

    if (fid >= s->count) s->count = fid + 1;
    return fid;
}

int lframe_get_capacity(const LFrameSystem *s) {
    return s ? s->capacity : 0;
}

LFrame *lframe_get(LFrameSystem *s, int id) {
    if (!s || id < 0 || id >= s->capacity) return NULL;
    if (!s->frames[id] || s->frames[id]->destroyed) return NULL;
    return s->frames[id];
}

LFrame *lframe_get_by_name(LFrameSystem *s, const char *name) {
    if (!s || !name || !name[0]) return NULL;
    for (int i = 0; i < s->capacity; i++) {
        if (s->frames[i] && !s->frames[i]->destroyed && strcmp(s->frames[i]->name, name) == 0)
            return s->frames[i];
    }
    return NULL;
}

void lframe_destroy(LFrameSystem *s, int id) {
    if (!s || id <= 0 || id >= s->capacity) return; /* cannot destroy UIParent */
    if (s->drag_frame_id == id) s->drag_frame_id = -1;
    if (s->resize_frame_id == id) s->resize_frame_id = -1;
    if (s->focus_frame_id == id) {
        s->focus_frame_id = -1;
        g_chat_active = false;
    }
    if (s->select_frame_id == id) s->select_frame_id = -1;
    LFrame *f = s->frames[id];
    if (!f || f->destroyed) return;

    /* Remove from parent's child list */
    if (f->parent_id >= 0 && f->parent_id < s->capacity) {
        LFrame *par = s->frames[f->parent_id];
        if (par) {
            for (int i = 0; i < par->child_count; i++) {
                if (par->children[i] == id) {
                    par->children[i] = par->children[--par->child_count];
                    break;
                }
            }
        }
    }

    /* Recursively destroy children */
    for (int i = 0; i < f->child_count; i++)
        lframe_destroy(s, f->children[i]);

    f->destroyed = true;
}

/* -------------------------------------------------------------------------
 * Layout resolution
 * ---------------------------------------------------------------------- */
void lframe_resolve_layout(LFrameSystem *s, float screen_w, float screen_h) {
    if (!s) return;

    /* Compute scale factor based on design height 720 */
#ifdef RAYLIB_LUA_UI_TEST
    g_ui_scale = 1.0f;
#else
    g_ui_scale = screen_h / 720.0f;
    if (g_ui_scale < 0.5f) g_ui_scale = 0.5f;
    if (g_ui_scale > 4.0f) g_ui_scale = 4.0f;
#endif

    /* UIParent always covers the full screen */
    LFrame *ui = s->frames[0];
    if (ui) {
        ui->cx = 0.0f; ui->cy = 0.0f;
        ui->w  = screen_w; ui->h = screen_h;
        ui->anchored_ok = true;
    }

    /* Reset resolved state for all non-UIParent frames */
    for (int i = 1; i < s->capacity; i++) {
        LFrame *f = s->frames[i];
        if (!f || f->destroyed) continue;
        f->anchored_ok = false;
    }

    /* Iterative resolution: repeat up to 3 passes until no progress */
    for (int pass = 0; pass < 3; pass++) {
        bool progress = false;
        for (int i = 1; i < s->capacity; i++) {
            LFrame *f = s->frames[i];
            if (!f || f->destroyed || f->anchored_ok) continue;

            if (!f->has_anchor) {
                /* No anchor: default to (0,0) */
                f->cx = 0.0f; f->cy = 0.0f;
                f->anchored_ok = true;
                progress = true;
                continue;
            }

            /* Determine reference frame */
            int rid = f->anchor.rel_id;
            if (rid < 0 && f->anchor.rel_name[0] != '\0') {
                if (strcmp(f->anchor.rel_name, "UIParent") == 0) {
                    f->anchor.rel_id = LFRAME_UIPARENT;
                    rid = LFRAME_UIPARENT;
                } else {
                    LFrame *rel_f = lframe_get_by_name(s, f->anchor.rel_name);
                    if (rel_f) {
                        f->anchor.rel_id = rel_f->id;
                        rid = rel_f->id;
                    }
                }
            }
            if (rid < 0) rid = f->parent_id; /* -1 = use parent */
            if (rid < 0) rid = LFRAME_UIPARENT;
            if (rid >= s->capacity) rid = LFRAME_UIPARENT;

            LFrame *rel = s->frames[rid];
            if (!rel || rel->destroyed || !rel->anchored_ok) continue; /* wait for it */

            float rel_w, rel_h;
            get_frame_scaled_size(rel, &rel_w, &rel_h);

            /* Position of the named point on the reference frame */
            float rpx, rpy;
            anchor_point(f->anchor.rel_point, rel->cx, rel->cy, rel_w, rel_h, &rpx, &rpy);

            float frame_w, frame_h;
            get_frame_scaled_size(f, &frame_w, &frame_h);

            /* Our own anchor offset within (0,0,w,h) */
            float opx, opy;
            anchor_point(f->anchor.point, 0.0f, 0.0f, frame_w, frame_h, &opx, &opy);

            f->cx = rpx + f->anchor.ox * g_ui_scale - opx;
            f->cy = rpy - f->anchor.oy * g_ui_scale - opy;
            f->anchored_ok = true;
            progress = true;
        }
        if (!progress) break;
    }
}

/* -------------------------------------------------------------------------
 * Sorting helper — sort by strata ascending (draw low strata first)
 * ---------------------------------------------------------------------- */
static int compare_strata(const void *a, const void *b) {
    const LFrame *fa = *(const LFrame **)a;
    const LFrame *fb = *(const LFrame **)b;
    int diff = (int)fa->effective_strata - (int)fb->effective_strata;
    if (diff != 0) return diff;
    return fa->id - fb->id; /* lower id = created first = parent before children */
}

/* Walk the parent chain; returns false if any ancestor is hidden or destroyed. */
static bool is_frame_actually_visible(const LFrameSystem *s, int id) {
    int cur = id;
    while (cur >= 0 && cur < s->capacity && cur != LFRAME_UIPARENT) {
        const LFrame *f = s->frames[cur];
        if (!f || f->destroyed || !f->visible) return false;
        cur = f->parent_id;
    }
    return true;
}

/* -------------------------------------------------------------------------
 * Drawing
 * ---------------------------------------------------------------------- */

void lframe_update_layout(LFrame *f) {
    if (!f) return;

    LFTextLayout *layout = &f->layout_cache;
    memset(layout, 0, sizeof(LFTextLayout));

    const char *p = f->text;
    int line_idx = 0;
    int char_idx = 0;
    char temp_lines[LF_MAX_LINES][LF_TEXT_LEN];

    while (*p && line_idx < LF_MAX_LINES) {
        if (*p == '\n') {
            temp_lines[line_idx][char_idx] = '\0';
            line_idx++;
            char_idx = 0;
        } else {
            if (char_idx < LF_TEXT_LEN - 1) {
                temp_lines[line_idx][char_idx++] = *p;
            }
        }
        p++;
    }
    if (char_idx > 0 && line_idx < LF_MAX_LINES) {
        temp_lines[line_idx][char_idx] = '\0';
        line_idx++;
    }
    layout->line_count = line_idx;

    for (int l = 0; l < layout->line_count; l++) {
        LFLineLayout *line = &layout->lines[l];
        const char *src = temp_lines[l];

        if (f->pre_escaped || !strchr(src, '|')) {
            line->run_count = 1;
            snprintf(line->runs[0].text, sizeof(line->runs[0].text), "%.255s", src);
            line->runs[0].is_default_color = true;
            line->measured_width = MeasureText(src, (int)(f->font_size * g_ui_scale));
        } else {
            int s_idx = 0;
            char clean_text[LF_TEXT_LEN] = "";
            int clean_len = 0;
            Color current_color = { 255, 255, 255, 255 };
            bool is_default = true;

            char run_buf[256];
            int run_len = 0;

            while (src[s_idx] && line->run_count < LF_MAX_RUNS_PER_LINE) {
                if (src[s_idx] == '|') {
                    if (src[s_idx + 1] == '|') {
                        if (run_len < (int)sizeof(run_buf) - 1) {
                            run_buf[run_len++] = '|';
                        }
                        if (clean_len < (int)sizeof(clean_text) - 1) {
                            clean_text[clean_len++] = '|';
                        }
                        s_idx += 2;
                    } else if (src[s_idx + 1] == 'c' || src[s_idx + 1] == 'C') {
                        bool is_hex = true;
                        for (int i = 0; i < 8; i++) {
                            char c = src[s_idx + 2 + i];
                            if (!c || !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                                is_hex = false;
                                break;
                            }
                        }
                        if (is_hex) {
                            if (run_len > 0) {
                                run_buf[run_len] = '\0';
                                LFTextRun *run = &line->runs[line->run_count++];
                                snprintf(run->text, sizeof(run->text), "%.255s", run_buf);
                                run->raw_color = current_color;
                                run->is_default_color = is_default;
                                run_len = 0;
                            }
                            unsigned int a = 255, r = 255, g = 255, b = 255;
                            char hex[9];
                            memcpy(hex, &src[s_idx + 2], 8);
                            hex[8] = '\0';
                            sscanf(hex, "%02x%02x%02x%02x", &a, &r, &g, &b);
                            current_color = (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a };
                            is_default = false;
                            s_idx += 10;
                        } else {
                            if (run_len < (int)sizeof(run_buf) - 1) {
                                run_buf[run_len++] = src[s_idx];
                            }
                            if (clean_len < (int)sizeof(clean_text) - 1) {
                                clean_text[clean_len++] = src[s_idx];
                            }
                            s_idx++;
                        }
                    } else if (src[s_idx + 1] == 'r' || src[s_idx + 1] == 'R') {
                        if (run_len > 0) {
                            run_buf[run_len] = '\0';
                            LFTextRun *run = &line->runs[line->run_count++];
                            snprintf(run->text, sizeof(run->text), "%.255s", run_buf);
                            run->raw_color = current_color;
                            run->is_default_color = is_default;
                            run_len = 0;
                        }
                        is_default = true;
                        s_idx += 2;
                    } else {
                        if (run_len < (int)sizeof(run_buf) - 1) {
                            run_buf[run_len++] = src[s_idx];
                        }
                        if (clean_len < (int)sizeof(clean_text) - 1) {
                            clean_text[clean_len++] = src[s_idx];
                        }
                        s_idx++;
                    }
                } else {
                    if (run_len < (int)sizeof(run_buf) - 1) {
                        run_buf[run_len++] = src[s_idx];
                    }
                    if (clean_len < (int)sizeof(clean_text) - 1) {
                        clean_text[clean_len++] = src[s_idx];
                    }
                    s_idx++;
                }
            }
            if (run_len > 0 && line->run_count < LF_MAX_RUNS_PER_LINE) {
                run_buf[run_len] = '\0';
                LFTextRun *run = &line->runs[line->run_count++];
                snprintf(run->text, sizeof(run->text), "%.255s", run_buf);
                run->raw_color = current_color;
                run->is_default_color = is_default;
            }

            clean_text[clean_len] = '\0';
            line->measured_width = MeasureText(clean_text, (int)(f->font_size * g_ui_scale));
        }

        if (line->measured_width > layout->total_width) {
            layout->total_width = line->measured_width;
        }
    }

    int spacing = (int)(2 * g_ui_scale);
    layout->total_height = layout->line_count * (int)(f->font_size * g_ui_scale) + (layout->line_count - 1) * spacing;
    f->layout_dirty = false;
}

void lframe_draw(LFrameSystem *s, void (*on_draw_cb)(int fid, void *userdata), void *userdata) {
    if (!s) return;

    /* Compute effective strata for all frames by propagating non-MEDIUM strata down from parents */
    for (int i = 0; i < s->capacity; i++) {
        LFrame *f = s->frames[i];
        if (!f || f->destroyed) continue;
        int cur = i;
        LFStrata eff = f->strata;
        while (cur >= 0 && cur < s->capacity) {
            LFrame *curr = s->frames[cur];
            if (!curr || curr->destroyed) break;
            if (curr->strata != LFS_MEDIUM || cur == LFRAME_UIPARENT) {
                eff = curr->strata;
                break;
            }
            cur = curr->parent_id;
        }
        f->effective_strata = eff;
    }

    /* Resize draw_order if needed */
    if (s->capacity > s->draw_order_capacity) {
        s->draw_order = (LFrame **)RL_REALLOC(s->draw_order, (size_t)s->capacity * sizeof(LFrame *));
        s->draw_order_capacity = s->capacity;
    }

    /* Collect visible, non-destroyed frames into our dynamic array */
    int n = 0;
    for (int i = 0; i < s->capacity; i++) {
        LFrame *f = s->frames[i];
        if (!f || f->destroyed || !f->visible || i == LFRAME_UIPARENT) continue;
        if (!is_frame_actually_visible(s, f->parent_id)) continue;
        s->draw_order[n++] = f;
    }

    /* Stable sort by strata */
    qsort(s->draw_order, (size_t)n, sizeof(LFrame *), compare_strata);

    for (int si = 0; si < n; si++) {
        LFrame *f = s->draw_order[si]; /* non-const for is_mouse_over etc */
        float x = f->cx, y = f->cy;
        float w, h;
        get_frame_scaled_size(f, &w, &h);

        /* Background */
        if (f->has_bg) {
            float a = f->bg_a * f->alpha;
            DrawRectangle((int)x, (int)y, (int)w, (int)h,
                          rgba(f->bg_r, f->bg_g, f->bg_b, a));
        }

        /* Border */
        if (f->has_border) {
            DrawRectangleLinesEx((Rectangle){ x, y, w, h }, f->border_w * g_ui_scale,
                                  rgba(f->border_r, f->border_g, f->border_b, f->alpha));
        }

        /* Visual resize handle in bottom-right corner */
        if (f->resizable && f->visible) {
            Color handle_color = rgba(0.5f, 0.5f, 0.6f, f->alpha);
            DrawLine((int)(x + w - 4 * g_ui_scale), (int)(y + h - 2 * g_ui_scale), (int)(x + w - 2 * g_ui_scale), (int)(y + h - 4 * g_ui_scale), handle_color);
            DrawLine((int)(x + w - 8 * g_ui_scale), (int)(y + h - 2 * g_ui_scale), (int)(x + w - 2 * g_ui_scale), (int)(y + h - 8 * g_ui_scale), handle_color);
            DrawLine((int)(x + w - 12 * g_ui_scale), (int)(y + h - 2 * g_ui_scale), (int)(x + w - 2 * g_ui_scale), (int)(y + h - 12 * g_ui_scale), handle_color);
        }

        /* Type-specific rendering */
        switch (f->type) {
            case LFT_STATUSBAR: {
                /* Dark background bar */
                DrawRectangle((int)x, (int)y, (int)w, (int)h,
                              rgba(f->bar_r * 0.3f, f->bar_g * 0.3f, f->bar_b * 0.3f, f->bar_alpha));
                /* Filled portion */
                float range = f->bar_max - f->bar_min;
                float frac  = (range > 0.0f) ? (f->bar_val - f->bar_min) / range : 0.0f;
                if (frac < 0.0f) frac = 0.0f;
                if (frac > 1.0f) frac = 1.0f;
                DrawRectangle((int)x, (int)y, (int)(w * frac), (int)h,
                              rgba(f->bar_r, f->bar_g, f->bar_b, f->bar_alpha));
                break;
            }
            case LFT_TEXTURE: {
                if (f->has_tex_color)
                    DrawRectangle((int)x, (int)y, (int)w, (int)h,
                                  rgba(f->tex_r, f->tex_g, f->tex_b, f->tex_a * f->alpha));
                break;
            }
            case LFT_BUTTON: {
                /* Highlight overlay when hovered */
                if (f->is_mouse_over && f->hi_a > 0.0f)
                    DrawRectangle((int)x, (int)y, (int)w, (int)h,
                                  rgba(f->hi_r, f->hi_g, f->hi_b, f->hi_a));
                /* Centered text */
                if (f->text[0]) {
                    if (f->layout_dirty) {
                        lframe_update_layout(f);
                    }
                    LFTextLayout *layout = &f->layout_cache;
                    if (layout->line_count > 0) {
                        LFLineLayout *line = &layout->lines[0];
                        int tw = line->measured_width;
                        int tx = (int)(x + (w - tw) / 2.0f);
                        int ty = (int)(y + (h - (int)(f->font_size * g_ui_scale)) / 2.0f);

                        int x_offset = 0;
                        for (int r = 0; r < line->run_count; r++) {
                            LFTextRun *run = &line->runs[r];
                            Color color;
                            if (run->is_default_color) {
                                color = rgba(f->text_r, f->text_g, f->text_b, f->alpha);
                            } else {
                                color = (Color){
                                    run->raw_color.r,
                                    run->raw_color.g,
                                    run->raw_color.b,
                                    (unsigned char)(run->raw_color.a * f->alpha)
                                };
                            }
                            DrawText(run->text, tx + x_offset, ty, (int)(f->font_size * g_ui_scale), color);
                            x_offset += MeasureText(run->text, (int)(f->font_size * g_ui_scale));
                        }
                    }
                }
                break;
            }
            case LFT_FONTSTRING: {
                if (f->text[0]) {
                    if (f->layout_dirty) {
                        lframe_update_layout(f);
                    }
                    LFTextLayout *layout = &f->layout_cache;
                    if (layout->line_count > 0) {
                        int spacing = (int)(2 * g_ui_scale); // pixel spacing between lines
                        int start_y = (int)(y + (h - layout->total_height) / 2.0f);
                        for (int i = 0; i < layout->line_count; i++) {
                            LFLineLayout *line = &layout->lines[i];
                            int tw = line->measured_width;
                            int tx;
                            if      (f->justify == 1) tx = (int)(x + (w - tw) / 2.0f); /* CENTER */
                            else if (f->justify == 2) tx = (int)(x + w - tw);           /* RIGHT  */
                            else                      tx = (int)x;                        /* LEFT   */
                            int ty = start_y + i * ((int)(f->font_size * g_ui_scale) + spacing);

                            int x_offset = 0;
                            for (int r = 0; r < line->run_count; r++) {
                                LFTextRun *run = &line->runs[r];
                                Color color;
                                if (run->is_default_color) {
                                    color = rgba(f->text_r, f->text_g, f->text_b, f->alpha);
                                } else {
                                    color = (Color){
                                        run->raw_color.r,
                                        run->raw_color.g,
                                        run->raw_color.b,
                                        (unsigned char)(run->raw_color.a * f->alpha)
                                    };
                                }
                                DrawText(run->text, tx + x_offset, ty, (int)(f->font_size * g_ui_scale), color);
                                x_offset += MeasureText(run->text, (int)(f->font_size * g_ui_scale));
                            }
                        }
                    }
                }
                break;
            }
            case LFT_EDITBOX: {
                WrappedLine wlines[MAX_WRAPPED_LINES];
                float wrap_w = w / g_ui_scale - 16.0f;
                if (wrap_w < 10.0f) wrap_w = 10.0f;
                int wline_count = lframe_wrap_text(f->text, f->font_size, wrap_w, wlines, MAX_WRAPPED_LINES);

                if (f->selection_start != f->selection_end) {
                    int sel_min = f->selection_start < f->selection_end ? f->selection_start : f->selection_end;
                    int sel_max = f->selection_start > f->selection_end ? f->selection_start : f->selection_end;

                    for (int l = 0; l < wline_count; l++) {
                        int l_start = wlines[l].start;
                        int l_end = wlines[l].start + wlines[l].len;

                        int int_start = sel_min > l_start ? sel_min : l_start;
                        int int_end = sel_max < l_end ? sel_max : l_end;

                        if (int_start < int_end) {
                            int col_start = int_start - l_start;
                            int col_end = int_end - l_start;

                            char temp_start[512];
                            int len_s = col_start;
                            if (len_s >= 512) len_s = 511;
                            memcpy(temp_start, &f->text[l_start], (size_t)len_s);
                            temp_start[len_s] = '\0';
                            float w_start = (float)MeasureText(temp_start, (int)(f->font_size * g_ui_scale));

                            char temp_end[512];
                            int len_e = col_end;
                            if (len_e >= 512) len_e = 511;
                            memcpy(temp_end, &f->text[l_start], (size_t)len_e);
                            temp_end[len_e] = '\0';
                            float w_end = (float)MeasureText(temp_end, (int)(f->font_size * g_ui_scale));

                            DrawRectangle((int)(x + 8 * g_ui_scale + w_start),
                                          (int)(y + 6 * g_ui_scale + l * ((int)(f->font_size * g_ui_scale) + (int)(2 * g_ui_scale))),
                                          (int)(w_end - w_start),
                                          (int)(f->font_size * g_ui_scale) + (int)(2 * g_ui_scale),
                                          rgba(0.2f, 0.4f, 0.8f, 0.4f * f->alpha));
                        }
                    }
                }

                for (int l = 0; l < wline_count; l++) {
                    char temp[512];
                    int len = wlines[l].len;
                    if (len >= 512) len = 511;
                    memcpy(temp, &f->text[wlines[l].start], (size_t)len);
                    temp[len] = '\0';
                    DrawText(temp,
                             (int)(x + 8 * g_ui_scale),
                             (int)(y + 6 * g_ui_scale + l * ((int)(f->font_size * g_ui_scale) + (int)(2 * g_ui_scale))),
                             (int)(f->font_size * g_ui_scale),
                             rgba(f->text_r, f->text_g, f->text_b, f->alpha));
                }

                if (s->focus_frame_id == f->id) {
                    bool draw_cursor = (((int)(GetTime() * 2.0f)) % 2 == 0);
                    if (draw_cursor) {
                        int cur_line = 0, cur_col = 0;
                        lframe_get_cursor_line_col(f->text, f->cursor_pos, wlines, wline_count, &cur_line, &cur_col);

                        char temp[512];
                        int len = cur_col;
                        if (len >= 512) len = 511;
                        memcpy(temp, &f->text[wlines[cur_line].start], (size_t)len);
                        temp[len] = '\0';
                        float w_cursor = (float)MeasureText(temp, (int)(f->font_size * g_ui_scale));

                        DrawLine((int)(x + 8 * g_ui_scale + w_cursor),
                                 (int)(y + 6 * g_ui_scale + cur_line * ((int)(f->font_size * g_ui_scale) + (int)(2 * g_ui_scale))),
                                 (int)(x + 8 * g_ui_scale + w_cursor),
                                 (int)(y + 6 * g_ui_scale + cur_line * ((int)(f->font_size * g_ui_scale) + (int)(2 * g_ui_scale)) + (int)(f->font_size * g_ui_scale) + (int)(2 * g_ui_scale)),
                                 rgba(f->text_r, f->text_g, f->text_b, f->alpha));
                    }
                }
                break;
            }
            default:
                /* LFT_FRAME — bg/border already drawn above, text if any */
                if (f->text[0]) {
                    if (f->layout_dirty) {
                        lframe_update_layout(f);
                    }
                    LFTextLayout *layout = &f->layout_cache;
                    if (layout->line_count > 0) {
                        LFLineLayout *line = &layout->lines[0];
                        int x_offset = 0;
                        for (int r = 0; r < line->run_count; r++) {
                            LFTextRun *run = &line->runs[r];
                            Color color;
                            if (run->is_default_color) {
                                color = rgba(f->text_r, f->text_g, f->text_b, f->alpha);
                            } else {
                                color = (Color){
                                    run->raw_color.r,
                                    run->raw_color.g,
                                    run->raw_color.b,
                                    (unsigned char)(run->raw_color.a * f->alpha)
                                };
                            }
                            DrawText(run->text, (int)x + x_offset, (int)y, (int)(f->font_size * g_ui_scale), color);
                            x_offset += MeasureText(run->text, (int)(f->font_size * g_ui_scale));
                        }
                    }
                }
                break;
        }

        /* OnDraw script callback */
        if (f->has_OnDraw && on_draw_cb) {
            on_draw_cb(f->id, userdata);
        }
    }

    /* Draw the active drag-and-drop follower if active */
    if (s->drag_active) {
        Vector2 mp = GetMousePosition();
        float w = 40.0f * g_ui_scale;
        float h = 40.0f * g_ui_scale;
        float x = mp.x - w / 2.0f;
        float y = mp.y - h / 2.0f;

        if (s->drag_icon_id >= 0) {
#ifndef RAYLIB_LUA_UI_TEST
            /* Provide callback for user application to draw icon if needed */ /* TODO: hook up drag icon rendering callback */
#endif
            DrawRectangleLinesEx((Rectangle){ x, y, w, h }, 2.0f * g_ui_scale, WHITE);
        } else if (s->drag_color_a > 0.0f) {
            DrawRectangle((int)x, (int)y, (int)w, (int)h,
                          rgba(s->drag_color_r, s->drag_color_g, s->drag_color_b, s->drag_color_a * 0.75f));
            DrawRectangleLinesEx((Rectangle){ x, y, w, h }, 1.5f * g_ui_scale,
                                  rgba(0.9f, 0.8f, 0.5f, 0.8f));
            if (s->drag_name[0]) {
                char short_name[8];
                strncpy(short_name, s->drag_name, 5);
                short_name[5] = '\0';
                DrawText(short_name, (int)(x + 4 * g_ui_scale), (int)(y + h * 0.35f), (int)(9 * g_ui_scale), WHITE);
            }
        }
    }
}

/* -------------------------------------------------------------------------
 * Input handling
 * ---------------------------------------------------------------------- */
bool lframe_handle_input(LFrameSystem *s, const LFrameInputCallbacks *cbs, void *userdata) {
    if (!s) return false;

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    lframe_resolve_layout(s, sw, sh);

    Vector2 mp = GetMousePosition();

    /* A. Handle active drag-and-drop follower if active */
    if (s->drag_active) {
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
            /* Drop! Find topmost frame under mouse */
            LFrame *topmost = NULL;
            for (int i = 1; i < s->capacity; i++) {
                LFrame *f = s->frames[i];
                if (!f || f->destroyed || !f->visible || !f->enable_mouse || !f->anchored_ok) continue;
                if (!is_frame_actually_visible(s, f->parent_id)) continue;
                float x = f->cx, y = f->cy;
                float w, h;
                get_frame_scaled_size(f, &w, &h);
                if (mp.x >= x && mp.x <= x + w && mp.y >= y && mp.y <= y + h) {
                    if (!topmost || f->effective_strata > topmost->effective_strata || 
                        (f->effective_strata == topmost->effective_strata && f->id > topmost->id)) {
                        topmost = f;
                    }
                }
            }

            /* Fire OnReceiveDrag on target */
            if (topmost && topmost->has_OnReceiveDrag && cbs && cbs->on_receive_drag) {
                cbs->on_receive_drag(topmost->id, s->drag_type, s->drag_data, s->drag_src_frame_id, userdata);
            }

            /* Fire OnDragStop on source */
            LFrame *src_f = lframe_get(s, s->drag_src_frame_id);
            if (src_f && src_f->has_OnDragStop && cbs && cbs->on_drag_stop) {
                cbs->on_drag_stop(src_f->id, s->drag_type, s->drag_data, topmost ? topmost->id : -1, userdata);
            }

            /* Clear drag state */
            s->drag_active = false;
            s->drag_src_frame_id = -1;
            s->drag_type[0] = '\0';
            s->drag_data[0] = '\0';
            s->drag_icon_id = -1;
            s->drag_name[0] = '\0';
            s->drag_color_r = 0.0f;
            s->drag_color_g = 0.0f;
            s->drag_color_b = 0.0f;
            s->drag_color_a = 0.0f;
        }
        return true; /* click consumed while dragging */
    }

    /* B. Handle potential drag-and-drop detection */
    if (s->potential_drag_frame_id >= 0) {
        bool still_down = false;
        if (s->potential_drag_button == MOUSE_BUTTON_LEFT) {
            still_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        } else if (s->potential_drag_button == MOUSE_BUTTON_RIGHT) {
            still_down = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
        }

        if (!still_down) {
            s->potential_drag_frame_id = -1;
            s->potential_drag_button = -1;
        } else {
            float dist = Vector2Distance(mp, s->potential_drag_start_pos);
            if (dist > 5.0f * g_ui_scale) {
                int fid = s->potential_drag_frame_id;
                int btn = s->potential_drag_button;
                s->potential_drag_frame_id = -1;
                s->potential_drag_button = -1;
                s->drag_src_frame_id = fid;

                LFrame *src_f = lframe_get(s, fid);
                if (src_f && src_f->has_OnDragStart && cbs && cbs->on_drag_start) {
                    cbs->on_drag_start(src_f->id, (btn == MOUSE_BUTTON_LEFT) ? "Left" : "Right", userdata);
                }
            }
        }
    }

    /* 1. Handle active resizing if in progress */
    if (s->resize_frame_id >= 0) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            LFrame *f = lframe_get(s, s->resize_frame_id);
            if (f) {
                float target_cx = f->cx;
                float target_cy = f->cy;

                float new_w = s->resize_start_size.x + (mp.x - s->resize_start_mouse.x) / g_ui_scale;
                float new_h = s->resize_start_size.y + (mp.y - s->resize_start_mouse.y) / g_ui_scale;
                if (new_w < 50.0f) new_w = 50.0f;
                if (new_h < 30.0f) new_h = 30.0f;
                f->w = new_w;
                f->h = new_h;

                if (!f->has_anchor) {
                    f->has_anchor = true;
                    f->anchor.rel_id = f->parent_id >= 0 ? f->parent_id : LFRAME_UIPARENT;
                    strncpy(f->anchor.point, "TOPLEFT", LF_POINT_LEN - 1);
                    strncpy(f->anchor.rel_point, "TOPLEFT", LF_POINT_LEN - 1);
                }

                int rid = f->anchor.rel_id;
                if (rid < 0) rid = f->parent_id;
                if (rid < 0 || rid >= s->capacity || !s->frames[rid] || s->frames[rid]->destroyed) rid = LFRAME_UIPARENT;
                LFrame *rel = s->frames[rid];

                float rel_w, rel_h;
                get_frame_scaled_size(rel, &rel_w, &rel_h);
                float rpx, rpy;
                anchor_point(f->anchor.rel_point, rel->cx, rel->cy, rel_w, rel_h, &rpx, &rpy);

                float frame_w, frame_h;
                get_frame_scaled_size(f, &frame_w, &frame_h);
                float opx, opy;
                anchor_point(f->anchor.point, 0.0f, 0.0f, frame_w, frame_h, &opx, &opy);

                f->anchor.ox = (target_cx - rpx + opx) / g_ui_scale;
                f->anchor.oy = (rpy - target_cy - opy) / g_ui_scale;

                f->anchored_ok = false;
                /* Invalidate anchors of child/rel frames */
                for (int i = 0; i < s->capacity; i++) {
                    LFrame *cf = s->frames[i];
                    if (cf && !cf->destroyed && cf->anchor.rel_id == f->id) {
                        cf->anchored_ok = false;
                    }
                }
            }
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            lframe_stop_moving_or_sizing(s);
        }
        return true;
    }

    /* 2. Handle active dragging if in progress */
    if (s->drag_frame_id >= 0) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            LFrame *f = lframe_get(s, s->drag_frame_id);
            if (f) {
                float target_cx = mp.x - s->drag_mouse_offset.x;
                float target_cy = mp.y - s->drag_mouse_offset.y;

                if (!f->has_anchor) {
                    f->has_anchor = true;
                    f->anchor.rel_id = f->parent_id >= 0 ? f->parent_id : LFRAME_UIPARENT;
                    strncpy(f->anchor.point, "TOPLEFT", LF_POINT_LEN - 1);
                    strncpy(f->anchor.rel_point, "TOPLEFT", LF_POINT_LEN - 1);
                }

                int rid = f->anchor.rel_id;
                if (rid < 0) rid = f->parent_id;
                if (rid < 0 || rid >= s->capacity || !s->frames[rid] || s->frames[rid]->destroyed) rid = LFRAME_UIPARENT;
                LFrame *rel = s->frames[rid];

                float rel_w, rel_h;
                get_frame_scaled_size(rel, &rel_w, &rel_h);
                float rpx, rpy;
                anchor_point(f->anchor.rel_point, rel->cx, rel->cy, rel_w, rel_h, &rpx, &rpy);

                float frame_w, frame_h;
                get_frame_scaled_size(f, &frame_w, &frame_h);
                float opx, opy;
                anchor_point(f->anchor.point, 0.0f, 0.0f, frame_w, frame_h, &opx, &opy);

                f->anchor.ox = (target_cx - rpx + opx) / g_ui_scale;
                f->anchor.oy = (rpy - target_cy - opy) / g_ui_scale;
                f->anchored_ok = false;
                /* Invalidate anchors of child/rel frames */
                for (int i = 0; i < s->capacity; i++) {
                    LFrame *cf = s->frames[i];
                    if (cf && !cf->destroyed && cf->anchor.rel_id == f->id) {
                        cf->anchored_ok = false;
                    }
                }
            }
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            lframe_stop_moving_or_sizing(s);
        }
        return true;
    }

    /* 3. Handle active text selection dragging if in progress */
    if (s->select_frame_id >= 0) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            LFrame *f = lframe_get(s, s->select_frame_id);
            if (f && f->type == LFT_EDITBOX) {
                int idx = lframe_get_char_index_at(f, mp);
                f->selection_end = idx;
                f->cursor_pos = idx;
            }
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            s->select_frame_id = -1;
        }
        return true;
    }

    /* Resize input_order if needed */
    if (s->capacity > s->input_order_capacity) {
        s->input_order = (LFrame **)RL_REALLOC(s->input_order, (size_t)s->capacity * sizeof(LFrame *));
        s->input_order_capacity = s->capacity;
    }

    /* Collect clickable frames into sorted order (highest strata first) */
    int n = 0;
    for (int i = 1; i < s->capacity; i++) {
        LFrame *f = s->frames[i];
        if (!f || f->destroyed || !f->visible || !f->enable_mouse || !f->anchored_ok) continue;
        if (!is_frame_actually_visible(s, f->parent_id)) continue;
        s->input_order[n++] = f;
    }
    /* Sort highest strata first for input (reverse of draw order) */
    qsort(s->input_order, (size_t)n, sizeof(LFrame *), compare_strata);

    int pressed_btn = -1;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) pressed_btn = MOUSE_BUTTON_LEFT;
    else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) pressed_btn = MOUSE_BUTTON_RIGHT;

    LFrame *target_frame = NULL;

    for (int si = n - 1; si >= 0; si--) {
        LFrame *f = s->input_order[si];
        float x = f->cx, y = f->cy;
        float w, h;
        get_frame_scaled_size(f, &w, &h);
        bool in_bounds = (mp.x >= x && mp.x <= x + w && mp.y >= y && mp.y <= y + h);

        bool was_over = f->is_mouse_over;
        f->is_mouse_over = in_bounds;

        if (cbs) {
            if (!was_over && in_bounds && f->has_OnEnter && cbs->on_enter) {
                cbs->on_enter(f->id, userdata);
            }
            if (was_over && !in_bounds && f->has_OnLeave && cbs->on_leave) {
                cbs->on_leave(f->id, userdata);
            }
        }

        if (!target_frame && in_bounds && pressed_btn != -1) {
            target_frame = f;
        }
    }

    if (pressed_btn != -1) {
        if (!target_frame || target_frame->type != LFT_EDITBOX) {
            if (s->focus_frame_id >= 0) {
                s->focus_frame_id = -1;
                g_chat_active = false;
            }
        }
    }

    bool click_consumed = false;
    if (target_frame) {
        /* Check if click starts potential drag-and-drop */
        if ((pressed_btn == MOUSE_BUTTON_LEFT && target_frame->register_drag_left) ||
            (pressed_btn == MOUSE_BUTTON_RIGHT && target_frame->register_drag_right)) {
            s->potential_drag_frame_id = target_frame->id;
            s->potential_drag_button = pressed_btn;
            s->potential_drag_start_pos = mp;
        }

        if (target_frame->type == LFT_EDITBOX) {
            s->focus_frame_id = target_frame->id;
            g_chat_active = true;
            if (pressed_btn == MOUSE_BUTTON_LEFT) {
                float x = target_frame->cx, y = target_frame->cy;
                float w, h;
                get_frame_scaled_size(target_frame, &w, &h);
                if (target_frame->resizable &&
                    mp.x >= x + w - 15.0f * g_ui_scale && mp.x <= x + w &&
                    mp.y >= y + h - 15.0f * g_ui_scale && mp.y <= y + h) {
                    s->resize_frame_id = target_frame->id;
                    s->resize_start_size = (Vector2){ target_frame->w, target_frame->h };
                    s->resize_start_mouse = mp;
                    click_consumed = true;
                }
                else {
                    int idx = lframe_get_char_index_at(target_frame, mp);
                    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
                    if (shift) {
                        target_frame->selection_end = idx;
                    } else {
                        target_frame->selection_start = idx;
                        target_frame->selection_end = idx;
                    }
                    target_frame->cursor_pos = idx;
                    s->select_frame_id = target_frame->id;
                    click_consumed = true;
                }
            } else {
                click_consumed = true;
            }
        }
        else if (pressed_btn == MOUSE_BUTTON_LEFT) {
            /* Check if click starts resizing/dragging */
            float x = target_frame->cx, y = target_frame->cy;
            float w, h;
            get_frame_scaled_size(target_frame, &w, &h);
            if (target_frame->resizable &&
                mp.x >= x + w - 15.0f * g_ui_scale && mp.x <= x + w &&
                mp.y >= y + h - 15.0f * g_ui_scale && mp.y <= y + h) {
                s->resize_frame_id = target_frame->id;
                s->resize_start_size = (Vector2){ target_frame->w, target_frame->h };
                s->resize_start_mouse = mp;
                click_consumed = true;
            }
            else if (target_frame->movable) {
                s->drag_frame_id = target_frame->id;
                s->drag_mouse_offset = (Vector2){ mp.x - x, mp.y - y };
                click_consumed = true;
            }
        }

        if (!click_consumed) {
            /* Bubble the click event up the hierarchy */
            LFrame *curr = target_frame;
            while (curr && curr->id != LFRAME_UIPARENT) {
                bool stop_propagation = false;
                if (curr->has_OnClick && cbs && cbs->on_click) {
                    TraceLog(LOG_INFO, "[Frame Click] Clicked frame: '%s' (id: %d)", curr->name, curr->id);
                    cbs->on_click(curr->id, (pressed_btn == MOUSE_BUTTON_LEFT) ? "Left" : "Right", userdata);
                    stop_propagation = true;
                }
                if (stop_propagation) {
                    break;
                }
                /* Move to parent */
                if (curr->parent_id >= 0 && curr->parent_id < s->capacity) {
                    LFrame *parent = s->frames[curr->parent_id];
                    if (parent && !parent->destroyed && parent->visible) {
                        curr = parent;
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }
            click_consumed = true;
        }
    }

    return click_consumed;
}

/* -------------------------------------------------------------------------
 * Test / diagnostic helpers
 * ---------------------------------------------------------------------- */

bool lframe_is_visible_in_hierarchy(const LFrameSystem *s, int id) {
    if (!s || id < 0 || id >= s->capacity) return false;
    const LFrame *f = s->frames[id];
    if (!f || f->destroyed || !f->visible) return false;
    return is_frame_actually_visible(s, f->parent_id);
}

/* Same collection+sort logic as lframe_draw, without the actual drawing. */
int lframe_collect_draw_order(const LFrameSystem *s, int *out_ids, int max_ids) {
    if (!s || !out_ids || max_ids <= 0) return 0;

    /* Compute effective strata for all frames by propagating non-MEDIUM strata down from parents */
    LFrameSystem *non_const_s = (LFrameSystem *)s;
    for (int i = 0; i < s->capacity; i++) {
        LFrame *f = non_const_s->frames[i];
        if (!f || f->destroyed) continue;
        int cur = i;
        LFStrata eff = f->strata;
        while (cur >= 0 && cur < s->capacity) {
            LFrame *curr = non_const_s->frames[cur];
            if (!curr || curr->destroyed) break;
            if (curr->strata != LFS_MEDIUM || cur == LFRAME_UIPARENT) {
                eff = curr->strata;
                break;
            }
            cur = curr->parent_id;
        }
        f->effective_strata = eff;
    }

    const LFrame **order = (const LFrame **)RL_MALLOC((size_t)s->capacity * sizeof(LFrame *));
    if (!order) return 0;

    int n = 0;
    for (int i = 0; i < s->capacity; i++) {
        const LFrame *f = s->frames[i];
        if (!f || f->destroyed || !f->visible || i == LFRAME_UIPARENT) continue;
        if (!is_frame_actually_visible(s, f->parent_id)) continue;
        order[n++] = f;
    }
    qsort(order, (size_t)n, sizeof(LFrame *), compare_strata);
    int out = n < max_ids ? n : max_ids;
    for (int i = 0; i < out; i++) out_ids[i] = order[i]->id;
    RL_FREE(order);
    return out;
}

int lframe_count_alive(const LFrameSystem *s) {
    if (!s) return 0;
    int count = 0;
    for (int i = 0; i < s->capacity; i++)
        if (s->frames[i] && !s->frames[i]->destroyed) count++;
    return count;
}

void lframe_start_moving(LFrameSystem *s, int id) {
    if (!s || id < 0 || id >= s->capacity) return;
    LFrame *f = s->frames[id];
    if (!f || f->destroyed) return;
    Vector2 mp = GetMousePosition();
    s->drag_frame_id = id;
    s->drag_mouse_offset = (Vector2){ mp.x - f->cx, mp.y - f->cy };
}

void lframe_stop_moving_or_sizing(LFrameSystem *s) {
    if (!s) return;
    int prev_drag = s->drag_frame_id;
    int prev_resize = s->resize_frame_id;

    s->drag_frame_id = -1;
    s->resize_frame_id = -1;

    int fid = (prev_drag >= 0) ? prev_drag : prev_resize;
    if (fid >= 0) {
        LFrame *f = lframe_get(s, fid);
        if (f && f->name[0]) {
            lframe_update_saved_layout(s, f);
            lframe_layout_save(s, "layout.cfg");
        }
    }
}

bool lframe_is_mouse_over_any(const LFrameSystem *s) {
    if (!s) return false;
    if (s->drag_active) return true;
    Vector2 mp = GetMousePosition();
    for (int i = 1; i < s->capacity; i++) {
        LFrame *f = s->frames[i];
        if (!f || f->destroyed || !f->visible || !f->enable_mouse || !f->anchored_ok) continue;
        if (!is_frame_actually_visible(s, f->parent_id)) continue;
        float x = f->cx, y = f->cy, w = f->w, h = f->h;
        if (mp.x >= x && mp.x <= x + w && mp.y >= y && mp.y <= y + h) {
            return true;
        }
    }
    return false;
}

void lframe_start_drag(LFrameSystem *s, const char *type, const char *data, int icon_id, const char *name, float r, float g, float b, float a) {
    if (!s) return;
    s->drag_active = true;
    if (s->drag_src_frame_id < 0) {
        s->drag_src_frame_id = s->potential_drag_frame_id;
    }
    strncpy(s->drag_type, type ? type : "", sizeof(s->drag_type) - 1);
    strncpy(s->drag_data, data ? data : "", sizeof(s->drag_data) - 1);
    s->drag_icon_id = icon_id;
    strncpy(s->drag_name, name ? name : "", sizeof(s->drag_name) - 1);
    s->drag_color_r = r;
    s->drag_color_g = g;
    s->drag_color_b = b;
    s->drag_color_a = a;
}

bool lframe_get_drag_info(const LFrameSystem *s, const char **out_type, const char **out_data, int *out_icon_id, const char **out_name, float *out_r, float *out_g, float *out_b, float *out_a, int *out_src_fid) {
    if (!s || !s->drag_active) return false;
    if (out_type) *out_type = s->drag_type;
    if (out_data) *out_data = s->drag_data;
    if (out_icon_id) *out_icon_id = s->drag_icon_id;
    if (out_name) *out_name = s->drag_name;
    if (out_r) *out_r = s->drag_color_r;
    if (out_g) *out_g = s->drag_color_g;
    if (out_b) *out_b = s->drag_color_b;
    if (out_a) *out_a = s->drag_color_a;
    if (out_src_fid) *out_src_fid = s->drag_src_frame_id;
    return true;
}

void lframe_clear_drag(LFrameSystem *s) {
    if (!s) return;
    s->drag_active = false;
    s->drag_src_frame_id = -1;
    s->drag_type[0] = '\0';
    s->drag_data[0] = '\0';
    s->drag_icon_id = -1;
    s->drag_name[0] = '\0';
    s->drag_color_r = 0.0f;
    s->drag_color_g = 0.0f;
    s->drag_color_b = 0.0f;
    s->drag_color_a = 0.0f;
}

