#include "ui/python_api_internal.h"

PythonSystem *g_py_sys = NULL;

static int g_chat_active = 0; // Stub for compatibility

/* =========================================================================
 * Python C methods mapping flat C to LFrameSystem operations
 * ====================================================================== */

static PyObject* py_CreateFrame(PyObject* self, PyObject* args) {
    const char *type_str;
    const char *name;
    int parent_id;
    if (!PyArg_ParseTuple(args, "ssi", &type_str, &name, &parent_id)) return NULL;
    if (!g_py_sys) { PyErr_SetString(PyExc_RuntimeError, "PythonSystem not initialized"); return NULL; }

    LFType type = LFT_FRAME;
    if      (strcmp(type_str, "Button")     == 0) type = LFT_BUTTON;
    else if (strcmp(type_str, "StatusBar")  == 0) type = LFT_STATUSBAR;
    else if (strcmp(type_str, "FontString") == 0) type = LFT_FONTSTRING;
    else if (strcmp(type_str, "Texture")    == 0) type = LFT_TEXTURE;
    else if (strcmp(type_str, "EditBox")    == 0) type = LFT_EDITBOX;

    int fid = lframe_create(g_py_sys->frames, type, name, parent_id);
    if (type == LFT_EDITBOX) {
        LFrame *f = lframe_get(g_py_sys->frames, fid);
        if (f) {
            f->has_bg = true;
            f->bg_r = 0.1f; f->bg_g = 0.1f; f->bg_b = 0.15f; f->bg_a = 0.9f;
            f->has_border = true;
            f->border_r = 0.4f; f->border_g = 0.4f; f->border_b = 0.5f;
            f->border_w = 1.0f;
            f->enable_mouse = true;
            f->text_r = 0.9f; f->text_g = 0.9f; f->text_b = 0.9f;
        }
    }
    return PyLong_FromLong(fid);
}

static PyObject* py_GetName(PyObject* self, PyObject* args) {
    int fid;
    if (!PyArg_ParseTuple(args, "i", &fid)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) return PyUnicode_FromString(f->name);
    return PyUnicode_FromString("");
}

static PyObject* py_Hide(PyObject* self, PyObject* args) {
    int fid;
    if (!PyArg_ParseTuple(args, "i", &fid)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) { f->visible = false; f->layout_dirty = true; }
    Py_RETURN_NONE;
}

static PyObject* py_Show(PyObject* self, PyObject* args) {
    int fid;
    if (!PyArg_ParseTuple(args, "i", &fid)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) { f->visible = true; f->layout_dirty = true; }
    Py_RETURN_NONE;
}

static PyObject* py_IsVisible(PyObject* self, PyObject* args) {
    int fid;
    if (!PyArg_ParseTuple(args, "i", &fid)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        return PyBool_FromLong(lframe_is_visible_in_hierarchy(g_py_sys->frames, fid));
    }
    return PyBool_FromLong(0);
}

static PyObject* py_SetSize(PyObject* self, PyObject* args) {
    int fid;
    double w, h;
    if (!PyArg_ParseTuple(args, "idd", &fid, &w, &h)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        f->w = (float)w;
        f->h = (float)h;
        f->layout_dirty = true;
    }
    Py_RETURN_NONE;
}

static PyObject* py_GetWidth(PyObject* self, PyObject* args) {
    int fid;
    if (!PyArg_ParseTuple(args, "i", &fid)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    return PyFloat_FromDouble(f && !f->destroyed ? f->w : 0.0);
}

static PyObject* py_GetHeight(PyObject* self, PyObject* args) {
    int fid;
    if (!PyArg_ParseTuple(args, "i", &fid)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    return PyFloat_FromDouble(f && !f->destroyed ? f->h : 0.0);
}

extern float g_ui_scale;

static PyObject* py_GetRect(PyObject* self, PyObject* args) {
    int fid;
    if (!PyArg_ParseTuple(args, "i", &fid)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        return Py_BuildValue("dddd", (double)(f->cx / g_ui_scale), (double)(f->cy / g_ui_scale), (double)f->w, (double)f->h);
    }
    return Py_BuildValue("dddd", 0.0, 0.0, 0.0, 0.0);
}

static PyObject* py_SetPoint(PyObject* self, PyObject* args) {
    int fid, rel_fid;
    const char *point, *rel_point;
    double ox, oy;
    if (!PyArg_ParseTuple(args, "isizdd", &fid, &point, &rel_fid, &rel_point, &ox, &oy)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        f->has_anchor = true;
        strncpy(f->anchor.point, point, LF_POINT_LEN - 1);
        f->anchor.point[LF_POINT_LEN - 1] = '\0';
        f->anchor.rel_id = rel_fid;
        if (rel_point) {
            strncpy(f->anchor.rel_point, rel_point, LF_POINT_LEN - 1);
            f->anchor.rel_point[LF_POINT_LEN - 1] = '\0';
        } else {
            f->anchor.rel_point[0] = '\0';
        }
        f->anchor.ox = (float)ox;
        f->anchor.oy = (float)oy;
        f->layout_dirty = true;
    }
    Py_RETURN_NONE;
}

static PyObject* py_ClearAllPoints(PyObject* self, PyObject* args) {
    int fid;
    if (!PyArg_ParseTuple(args, "i", &fid)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) { f->has_anchor = false; f->layout_dirty = true; }
    Py_RETURN_NONE;
}

static PyObject* py_SetBackdropColor(PyObject* self, PyObject* args) {
    int fid;
    double r, g, b, a;
    if (!PyArg_ParseTuple(args, "idddd", &fid, &r, &g, &b, &a)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        f->has_bg = true;
        f->bg_r = (float)r; f->bg_g = (float)g; f->bg_b = (float)b; f->bg_a = (float)a;
    }
    Py_RETURN_NONE;
}

static PyObject* py_SetBackdropBorderColor(PyObject* self, PyObject* args) {
    int fid;
    double r, g, b, a = 1.0;
    if (!PyArg_ParseTuple(args, "iddd|d", &fid, &r, &g, &b, &a)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        f->has_border = true;
        f->border_r = (float)r; f->border_g = (float)g; f->border_b = (float)b;
    }
    Py_RETURN_NONE;
}

static PyObject* py_SetBackdropBorderWidth(PyObject* self, PyObject* args) {
    int fid;
    double w;
    if (!PyArg_ParseTuple(args, "id", &fid, &w)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        f->has_border = true;
        f->border_w = (float)w;
    }
    Py_RETURN_NONE;
}

static PyObject* py_SetMovable(PyObject* self, PyObject* args) {
    int fid;
    int movable;
    if (!PyArg_ParseTuple(args, "ip", &fid, &movable)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) { f->movable = (movable != 0); }
    Py_RETURN_NONE;
}

static PyObject* py_SetFrameStrata(PyObject* self, PyObject* args) {
    int fid;
    const char *strata_str;
    if (!PyArg_ParseTuple(args, "is", &fid, &strata_str)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        LFStrata strata = LFS_MEDIUM;
        if (strcmp(strata_str, "BACKGROUND") == 0) strata = LFS_BACKGROUND;
        else if (strcmp(strata_str, "LOW") == 0) strata = LFS_LOW;
        else if (strcmp(strata_str, "MEDIUM") == 0) strata = LFS_MEDIUM;
        else if (strcmp(strata_str, "HIGH") == 0) strata = LFS_HIGH;
        else if (strcmp(strata_str, "DIALOG") == 0) strata = LFS_DIALOG;
        else if (strcmp(strata_str, "FULLSCREEN") == 0) strata = LFS_FULLSCREEN;
        else if (strcmp(strata_str, "TOOLTIP") == 0) strata = LFS_TOOLTIP;
        f->strata = strata;
    }
    Py_RETURN_NONE;
}

static PyObject* py_EnableMouse(PyObject* self, PyObject* args) {
    int fid;
    int enable;
    if (!PyArg_ParseTuple(args, "ip", &fid, &enable)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) { f->enable_mouse = (enable != 0); }
    Py_RETURN_NONE;
}

static PyObject* py_SetText(PyObject* self, PyObject* args) {
    int fid;
    const char *text;
    if (!PyArg_ParseTuple(args, "is", &fid, &text)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        strncpy(f->text, text, sizeof(f->text) - 1);
        f->text[sizeof(f->text) - 1] = '\0';
        f->layout_dirty = true;
    }
    Py_RETURN_NONE;
}

static PyObject* py_GetText(PyObject* self, PyObject* args) {
    int fid;
    if (!PyArg_ParseTuple(args, "i", &fid)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) return PyUnicode_FromString(f->text);
    return PyUnicode_FromString("");
}

static PyObject* py_SetTextColor(PyObject* self, PyObject* args) {
    int fid;
    double r, g, b;
    if (!PyArg_ParseTuple(args, "iddd", &fid, &r, &g, &b)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        f->text_r = (float)r; f->text_g = (float)g; f->text_b = (float)b;
        f->layout_dirty = true;
    }
    Py_RETURN_NONE;
}

static PyObject* py_SetJustifyH(PyObject* self, PyObject* args) {
    int fid;
    const char *justify;
    if (!PyArg_ParseTuple(args, "is", &fid, &justify)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        if      (strcmp(justify, "LEFT")   == 0) f->justify = 0;
        else if (strcmp(justify, "CENTER") == 0) f->justify = 1;
        else if (strcmp(justify, "RIGHT")  == 0) f->justify = 2;
        f->layout_dirty = true;
    }
    Py_RETURN_NONE;
}

static PyObject* py_SetMinMaxValues(PyObject* self, PyObject* args) {
    int fid;
    double min_val, max_val;
    if (!PyArg_ParseTuple(args, "idd", &fid, &min_val, &max_val)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        f->bar_min = (float)min_val;
        f->bar_max = (float)max_val;
    }
    Py_RETURN_NONE;
}

static PyObject* py_SetValue(PyObject* self, PyObject* args) {
    int fid;
    double val;
    if (!PyArg_ParseTuple(args, "id", &fid, &val)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        f->bar_val = (float)val;
    }
    Py_RETURN_NONE;
}

static PyObject* py_SetStatusBarColor(PyObject* self, PyObject* args) {
    int fid;
    double r, g, b;
    if (!PyArg_ParseTuple(args, "iddd", &fid, &r, &g, &b)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        f->bar_r = (float)r; f->bar_g = (float)g; f->bar_b = (float)b;
    }
    Py_RETURN_NONE;
}

static PyObject* py_SetHighlightColor(PyObject* self, PyObject* args) {
    int fid;
    double r, g, b, a;
    if (!PyArg_ParseTuple(args, "idddd", &fid, &r, &g, &b, &a)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        f->hi_r = (float)r; f->hi_g = (float)g; f->hi_b = (float)b; f->hi_a = (float)a;
    }
    Py_RETURN_NONE;
}

static PyObject* py_SetScript(PyObject* self, PyObject* args) {
    int fid;
    const char *script_name;
    int has_handler;
    if (!PyArg_ParseTuple(args, "isp", &fid, &script_name, &has_handler)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        bool val = (has_handler != 0);
        if      (strcmp(script_name, "OnLoad")        == 0) f->has_OnLoad = val;
        else if (strcmp(script_name, "OnUpdate")      == 0) f->has_OnUpdate = val;
        else if (strcmp(script_name, "OnEvent")       == 0) f->has_OnEvent = val;
        else if (strcmp(script_name, "OnClick")       == 0) f->has_OnClick = val;
        else if (strcmp(script_name, "OnEnter")       == 0) f->has_OnEnter = val;
        else if (strcmp(script_name, "OnLeave")       == 0) f->has_OnLeave = val;
        else if (strcmp(script_name, "OnDraw")        == 0) f->has_OnDraw = val;
        else if (strcmp(script_name, "OnDragStart")   == 0) f->has_OnDragStart = val;
        else if (strcmp(script_name, "OnDragStop")    == 0) f->has_OnDragStop = val;
        else if (strcmp(script_name, "OnReceiveDrag") == 0) f->has_OnReceiveDrag = val;
    }
    Py_RETURN_NONE;
}

static PyObject* py_RegisterEvent(PyObject* self, PyObject* args) {
    int fid;
    const char *event_name;
    if (!PyArg_ParseTuple(args, "is", &fid, &event_name)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        if (f->event_count < LF_MAX_EVENTS) {
            strncpy(f->events[f->event_count++], event_name, LF_NAME_LEN - 1);
        }
    }
    Py_RETURN_NONE;
}

static PyObject* py_SetFont(PyObject* self, PyObject* args) {
    int fid;
    double size;
    if (!PyArg_ParseTuple(args, "id", &fid, &size)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        int sz = (int)size;
        if (f->font_size != sz) {
            f->font_size = sz;
            f->layout_dirty = true;
        }
        if (f->type == LFT_FONTSTRING && f->h <= 0.0f) {
            f->h = (float)sz;
        }
    }
    Py_RETURN_NONE;
}

static PyObject* py_SetAllPoints(PyObject* self, PyObject* args) {
    int fid;
    PyObject* rel_obj = NULL;
    if (!PyArg_ParseTuple(args, "i|O", &fid, &rel_obj)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        int rel_id = -1;
        char rel_name[LF_NAME_LEN] = "";
        if (rel_obj && rel_obj != Py_None) {
            if (PyLong_Check(rel_obj)) {
                rel_id = (int)PyLong_AsLong(rel_obj);
            } else {
                PyObject* id_attr = PyObject_GetAttrString(rel_obj, "id");
                if (id_attr) {
                    rel_id = (int)PyLong_AsLong(id_attr);
                    Py_DECREF(id_attr);
                }
            }
            LFrame *rel_f = lframe_get(g_py_sys->frames, rel_id);
            if (rel_f) {
                strncpy(rel_name, rel_f->name, LF_NAME_LEN - 1);
            }
        }
        if (rel_name[0] == '\0') {
            if (f->parent_id >= 0) {
                LFrame *par = lframe_get(g_py_sys->frames, f->parent_id);
                if (par && par->name[0]) {
                    strncpy(rel_name, par->name, LF_NAME_LEN - 1);
                }
            }
            if (rel_name[0] == '\0') {
                strcpy(rel_name, "UIParent");
            }
        }
        f->has_default_anchor = true;
        strncpy(f->default_anchor.point, "TOPLEFT", LF_POINT_LEN - 1);
        f->default_anchor.rel_id = rel_id;
        strncpy(f->default_anchor.rel_point, "TOPLEFT", LF_POINT_LEN - 1);
        strncpy(f->default_anchor.rel_name, rel_name, LF_NAME_LEN - 1);
        f->default_anchor.ox = 0.0f;
        f->default_anchor.oy = 0.0f;

        if (!f->user_placed) {
            f->has_anchor = true;
            strncpy(f->anchor.point,     "TOPLEFT",  LF_POINT_LEN - 1);
            f->anchor.rel_id = rel_id;
            strncpy(f->anchor.rel_point, "TOPLEFT",  LF_POINT_LEN - 1);
            strncpy(f->anchor.rel_name,  rel_name,   LF_NAME_LEN - 1);
            f->anchor.ox  = 0.0f;
            f->anchor.oy  = 0.0f;
            f->anchored_ok = false;
        }
    }
    Py_RETURN_NONE;
}

static PyObject* py_SetResizable(PyObject* self, PyObject* args) {
    int fid;
    PyObject *val;
    if (!PyArg_ParseTuple(args, "iO", &fid, &val)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        f->resizable = PyObject_IsTrue(val) ? true : false;
    }
    Py_RETURN_NONE;
}

static PyObject* py_IsResizable(PyObject* self, PyObject* args) {
    int fid;
    if (!PyArg_ParseTuple(args, "i", &fid)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        return PyBool_FromLong(f->resizable);
    }
    return PyBool_FromLong(0);
}

static PyObject* py_RegisterForDrag(PyObject* self, PyObject* args) {
    int fid;
    PyObject *btn1 = NULL, *btn2 = NULL;
    if (!PyArg_ParseTuple(args, "i|OO", &fid, &btn1, &btn2)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        f->register_drag_left = false;
        f->register_drag_right = false;
        PyObject *btns[2] = {btn1, btn2};
        for (int i = 0; i < 2; i++) {
            if (btns[i] && PyUnicode_Check(btns[i])) {
                const char *btn = PyUnicode_AsUTF8(btns[i]);
                if (strcmp(btn, "LeftButton") == 0 || strcmp(btn, "Left") == 0) {
                    f->register_drag_left = true;
                } else if (strcmp(btn, "RightButton") == 0 || strcmp(btn, "Right") == 0) {
                    f->register_drag_right = true;
                }
            }
        }
    }
    Py_RETURN_NONE;
}

static PyObject* py_SetPreEscaped(PyObject* self, PyObject* args) {
    int fid;
    PyObject *val;
    if (!PyArg_ParseTuple(args, "iO", &fid, &val)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        f->pre_escaped = PyObject_IsTrue(val) ? true : false;
    }
    Py_RETURN_NONE;
}

static PyObject* py_IsPreEscaped(PyObject* self, PyObject* args) {
    int fid;
    if (!PyArg_ParseTuple(args, "i", &fid)) return NULL;
    LFrame *f = lframe_get(g_py_sys->frames, fid);
    if (f && !f->destroyed) {
        return PyBool_FromLong(f->pre_escaped);
    }
    return PyBool_FromLong(0);
}

static PyMethodDef RaylibPythonUiMethods[] = {
    {"CreateFrame", py_CreateFrame, METH_VARARGS, "Create a UI frame"},
    {"GetName", py_GetName, METH_VARARGS, "Get name of frame"},
    {"Show", py_Show, METH_VARARGS, "Show frame"},
    {"Hide", py_Hide, METH_VARARGS, "Hide frame"},
    {"IsVisible", py_IsVisible, METH_VARARGS, "Get frame visibility"},
    {"SetSize", py_SetSize, METH_VARARGS, "Set frame size"},
    {"GetWidth", py_GetWidth, METH_VARARGS, "Get frame width"},
    {"GetHeight", py_GetHeight, METH_VARARGS, "Get frame height"},
    {"GetRect", py_GetRect, METH_VARARGS, "Get frame rect (x, y, w, h)"},
    {"SetPoint", py_SetPoint, METH_VARARGS, "Set frame anchor point"},
    {"ClearAllPoints", py_ClearAllPoints, METH_VARARGS, "Clear all points"},
    {"SetBackdropColor", py_SetBackdropColor, METH_VARARGS, "Set backdrop color"},
    {"SetBackdropBorderColor", py_SetBackdropBorderColor, METH_VARARGS, "Set backdrop border color"},
    {"SetBackdropBorderWidth", py_SetBackdropBorderWidth, METH_VARARGS, "Set backdrop border width"},
    {"SetMovable", py_SetMovable, METH_VARARGS, "Set if frame is movable"},
    {"SetFrameStrata", py_SetFrameStrata, METH_VARARGS, "Set frame strata"},
    {"EnableMouse", py_EnableMouse, METH_VARARGS, "Enable or disable mouse input on frame"},
    {"SetText", py_SetText, METH_VARARGS, "Set frame text"},
    {"GetText", py_GetText, METH_VARARGS, "Get frame text"},
    {"SetTextColor", py_SetTextColor, METH_VARARGS, "Set text color"},
    {"SetJustifyH", py_SetJustifyH, METH_VARARGS, "Set text horizontal justification"},
    {"SetMinMaxValues", py_SetMinMaxValues, METH_VARARGS, "Set status bar min/max values"},
    {"SetValue", py_SetValue, METH_VARARGS, "Set status bar value"},
    {"SetStatusBarColor", py_SetStatusBarColor, METH_VARARGS, "Set status bar color"},
    {"SetHighlightColor", py_SetHighlightColor, METH_VARARGS, "Set button highlight color"},
    {"SetScript", py_SetScript, METH_VARARGS, "Set whether a script callback exists"},
    {"RegisterEvent", py_RegisterEvent, METH_VARARGS, "Register an event"},
    {"SetFont", py_SetFont, METH_VARARGS, "Set FontString font size"},
    {"SetAllPoints", py_SetAllPoints, METH_VARARGS, "Set all points to relative frame"},
    {"SetResizable", py_SetResizable, METH_VARARGS, "Set if frame is resizable"},
    {"IsResizable", py_IsResizable, METH_VARARGS, "Get if frame is resizable"},
    {"RegisterForDrag", py_RegisterForDrag, METH_VARARGS, "Register mouse buttons for dragging"},
    {"SetPreEscaped", py_SetPreEscaped, METH_VARARGS, "Set whether input strings are pre-escaped"},
    {"IsPreEscaped", py_IsPreEscaped, METH_VARARGS, "Check if input strings are pre-escaped"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef raylib_python_ui_module = {
    PyModuleDef_HEAD_INIT,
    "_raylib_python_ui",
    "Internal C extension for raylib-python-ui",
    -1,
    RaylibPythonUiMethods
};

PyObject* PyInit__raylib_python_ui(void) {
    return PyModule_Create(&raylib_python_ui_module);
}

/* =========================================================================
 * Layout callbacks invoked from python_frame.c
 * ====================================================================== */

static void on_draw_cb(int fid, void *userdata) {
    PythonSystem *sys = (PythonSystem *)userdata;
    if (sys && sys->py_dispatch_on_draw && PyCallable_Check(sys->py_dispatch_on_draw)) {
        PyObject *args = PyTuple_Pack(1, PyLong_FromLong(fid));
        PyObject *res = PyObject_CallObject(sys->py_dispatch_on_draw, args);
        if (!res) PyErr_Print();
        Py_XDECREF(res);
        Py_DECREF(args);
    }
}

static void on_click_cb(int fid, const char *button, void *userdata) {
    PythonSystem *sys = (PythonSystem *)userdata;
    if (sys && sys->py_dispatch_on_click && PyCallable_Check(sys->py_dispatch_on_click)) {
        PyObject *args = PyTuple_Pack(2, PyLong_FromLong(fid), PyUnicode_FromString(button));
        PyObject *res = PyObject_CallObject(sys->py_dispatch_on_click, args);
        if (!res) PyErr_Print();
        Py_XDECREF(res);
        Py_DECREF(args);
    }
}

static void on_drag_start_cb(int fid, const char *button, void *userdata) {
    PythonSystem *sys = (PythonSystem *)userdata;
    if (sys && sys->py_dispatch_on_drag_start && PyCallable_Check(sys->py_dispatch_on_drag_start)) {
        PyObject *args = PyTuple_Pack(2, PyLong_FromLong(fid), PyUnicode_FromString(button));
        PyObject *res = PyObject_CallObject(sys->py_dispatch_on_drag_start, args);
        if (!res) PyErr_Print();
        Py_XDECREF(res);
        Py_DECREF(args);
    }
}

static void on_drag_stop_cb(int fid, const char *type, const char *data, int dest_fid, void *userdata) {
    PythonSystem *sys = (PythonSystem *)userdata;
    if (sys && sys->py_dispatch_on_drag_stop && PyCallable_Check(sys->py_dispatch_on_drag_stop)) {
        PyObject *args = PyTuple_Pack(4, PyLong_FromLong(fid), PyUnicode_FromString(type), PyUnicode_FromString(data), PyLong_FromLong(dest_fid));
        PyObject *res = PyObject_CallObject(sys->py_dispatch_on_drag_stop, args);
        if (!res) PyErr_Print();
        Py_XDECREF(res);
        Py_DECREF(args);
    }
}

static void on_receive_drag_cb(int fid, const char *type, const char *data, int src_fid, void *userdata) {
    PythonSystem *sys = (PythonSystem *)userdata;
    if (sys && sys->py_dispatch_on_receive_drag && PyCallable_Check(sys->py_dispatch_on_receive_drag)) {
        PyObject *args = PyTuple_Pack(4, PyLong_FromLong(fid), PyUnicode_FromString(type), PyUnicode_FromString(data), PyLong_FromLong(src_fid));
        PyObject *res = PyObject_CallObject(sys->py_dispatch_on_receive_drag, args);
        if (!res) PyErr_Print();
        Py_XDECREF(res);
        Py_DECREF(args);
    }
}

static void on_enter_cb(int fid, void *userdata) {
    PythonSystem *sys = (PythonSystem *)userdata;
    if (sys && sys->py_dispatch_on_enter && PyCallable_Check(sys->py_dispatch_on_enter)) {
        PyObject *args = PyTuple_Pack(1, PyLong_FromLong(fid));
        PyObject *res = PyObject_CallObject(sys->py_dispatch_on_enter, args);
        if (!res) PyErr_Print();
        Py_XDECREF(res);
        Py_DECREF(args);
    }
}

static void on_leave_cb(int fid, void *userdata) {
    PythonSystem *sys = (PythonSystem *)userdata;
    if (sys && sys->py_dispatch_on_leave && PyCallable_Check(sys->py_dispatch_on_leave)) {
        PyObject *args = PyTuple_Pack(1, PyLong_FromLong(fid));
        PyObject *res = PyObject_CallObject(sys->py_dispatch_on_leave, args);
        if (!res) PyErr_Print();
        Py_XDECREF(res);
        Py_DECREF(args);
    }
}

static LFrameInputCallbacks g_input_cbs = {
    .on_click = on_click_cb,
    .on_drag_start = on_drag_start_cb,
    .on_drag_stop = on_drag_stop_cb,
    .on_receive_drag = on_receive_drag_cb,
    .on_enter = on_enter_cb,
    .on_leave = on_leave_cb
};

/* =========================================================================
 * PythonSystem API implementations
 * ====================================================================== */

void python_system_register_inittab(const char *name, void *initfunc) {
    if (!Py_IsInitialized()) {
        PyImport_AppendInittab(name, (PyObject* (*)(void))initfunc);
    }
}

PythonSystem *python_system_create(void) {
    PythonSystem *sys = RL_CALLOC(1, sizeof(PythonSystem));
    sys->frames = lframe_system_create();
    lframe_layout_load(sys->frames, "layout.cfg");

    g_py_sys = sys;

    if (!Py_IsInitialized()) {
        PyImport_AppendInittab("_raylib_python_ui", PyInit__raylib_python_ui);
        Py_Initialize();
    }

    return sys;
}

void python_system_destroy(PythonSystem *sys) {
    if (!sys) return;
    if (sys->frames) {
        lframe_layout_save(sys->frames, "layout.cfg");
        lframe_system_destroy(sys->frames);
    }
    Py_XDECREF(sys->py_dispatch_event);
    Py_XDECREF(sys->py_dispatch_on_update);
    Py_XDECREF(sys->py_dispatch_on_draw);
    Py_XDECREF(sys->py_dispatch_on_click);
    Py_XDECREF(sys->py_dispatch_on_drag_start);
    Py_XDECREF(sys->py_dispatch_on_drag_stop);
    Py_XDECREF(sys->py_dispatch_on_receive_drag);
    Py_XDECREF(sys->py_dispatch_on_enter);
    Py_XDECREF(sys->py_dispatch_on_leave);

    if (g_py_sys == sys) g_py_sys = NULL;
    RL_FREE(sys);
}

void python_system_set_userdata(PythonSystem *sys, void *userdata) {
    if (sys) sys->userdata = userdata;
}

void *python_system_get_userdata(PythonSystem *sys) {
    return sys ? sys->userdata : NULL;
}

bool python_system_is_active(const PythonSystem *sys) {
    return sys && sys->active;
}

static int run_python_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buffer = (char *)malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return -1;
    }
    size_t read_bytes = fread(buffer, 1, size, f);
    buffer[read_bytes] = '\0';
    fclose(f);

    PyObject *co = Py_CompileString(buffer, path, Py_file_input);
    free(buffer);

    if (!co) {
        PyErr_Print();
        return -1;
    }

    PyObject *m = PyImport_AddModule("__main__");
    if (!m) {
        Py_DECREF(co);
        return -1;
    }
    PyObject *d = PyModule_GetDict(m);
    if (!d) {
        Py_DECREF(co);
        return -1;
    }
    PyObject *v = PyEval_EvalCode(co, d, d);
    Py_DECREF(co);

    if (!v) {
        PyErr_Print();
        return -1;
    }
    Py_DECREF(v);
    return 0;
}

bool python_system_load_addons(PythonSystem *sys, void *userdata, const char *addons_dir) {
    if (!sys || !addons_dir) return false;
    sys->userdata = userdata;

    FilePathList files = LoadDirectoryFiles(addons_dir);
    if (files.count == 0) {
        TraceLog(LOG_WARNING, "No addons directory: %s", addons_dir);
        UnloadDirectoryFiles(files);
        return false;
    }

    char addon_names[64][LF_NAME_LEN];
    int  addon_count = 0;

    for (unsigned int i = 0; i < files.count && addon_count < 64; i++) {
        const char *name = GetFileName(files.paths[i]);
        if (name[0] == '.') continue;
        char toc[1024];
        snprintf(toc, sizeof(toc), "%s/%s/%s.toc", addons_dir, name, name);
        FILE *f_test = fopen(toc, "r");
        if (f_test) {
            strncpy(addon_names[addon_count++], name, LF_NAME_LEN - 1);
            fclose(f_test);
        }
    }
    UnloadDirectoryFiles(files);

    if (addon_count == 0) {
        TraceLog(LOG_INFO, "No addons found in %s", addons_dir);
        return false;
    }

    /* CoreUI goes first */
    for (int i = 1; i < addon_count; i++) {
        if (strcmp(addon_names[i], "CoreUI") == 0) {
            char tmp[LF_NAME_LEN];
            memcpy(tmp,             addon_names[0], LF_NAME_LEN);
            memcpy(addon_names[0],  addon_names[i], LF_NAME_LEN);
            memcpy(addon_names[i],  tmp,            LF_NAME_LEN);
            break;
        }
    }

    PyRun_SimpleString("import sys\n"
                       "sys.path.insert(0, 'addons')\n"
                       "sys.path.insert(0, 'addons/CoreUI')\n");

    /* Bootstrap builtins */
    PyRun_SimpleString("import builtins\n"
                       "try:\n"
                       "    import glimmerwood\n"
                       "    for name in dir(glimmerwood):\n"
                       "        if not name.startswith('_'):\n"
                       "            setattr(builtins, name, getattr(glimmerwood, name))\n"
                       "except ImportError: pass\n"
                       "import raylib_python_ui\n"
                       "for name in dir(raylib_python_ui):\n"
                       "    if not name.startswith('_'):\n"
                       "        setattr(builtins, name, getattr(raylib_python_ui, name))\n");

    for (int ai = 0; ai < addon_count; ai++) {
        char toc_path[1024];
        snprintf(toc_path, sizeof(toc_path), "%s/%s/%s.toc",
                 addons_dir, addon_names[ai], addon_names[ai]);
        FILE *f = fopen(toc_path, "r");
        if (!f) continue;

        TraceLog(LOG_INFO, "Loading python addon: %s", addon_names[ai]);
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            int len = (int)strlen(line);
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
                line[--len] = '\0';
            if (line[0] == '#' || line[0] == '\0') continue;

            char py_file[512];
            strncpy(py_file, line, sizeof(py_file) - 1);
            py_file[sizeof(py_file) - 1] = '\0';
            char *dot = strrchr(py_file, '.');
            if (dot && strcmp(dot, ".lua") == 0) {
                strcpy(dot, ".py");
            }

            char py_path[1024];
            snprintf(py_path, sizeof(py_path), "%s/%s/%s",
                     addons_dir, addon_names[ai], py_file);

            if (run_python_file(py_path) != 0) {
                TraceLog(LOG_WARNING, "Could not load/run python script: %s", py_path);
            }
        }
        fclose(f);
    }

    PyObject *mod = PyImport_ImportModule("raylib_python_ui");
    if (mod) {
        sys->py_dispatch_event = PyObject_GetAttrString(mod, "_dispatch_event");
        sys->py_dispatch_on_update = PyObject_GetAttrString(mod, "_dispatch_on_update");
        sys->py_dispatch_on_draw = PyObject_GetAttrString(mod, "_dispatch_on_draw");
        sys->py_dispatch_on_click = PyObject_GetAttrString(mod, "_dispatch_on_click");
        sys->py_dispatch_on_drag_start = PyObject_GetAttrString(mod, "_dispatch_on_drag_start");
        sys->py_dispatch_on_drag_stop = PyObject_GetAttrString(mod, "_dispatch_on_drag_stop");
        sys->py_dispatch_on_receive_drag = PyObject_GetAttrString(mod, "_dispatch_on_receive_drag");
        sys->py_dispatch_on_enter = PyObject_GetAttrString(mod, "_dispatch_on_enter");
        sys->py_dispatch_on_leave = PyObject_GetAttrString(mod, "_dispatch_on_leave");
        Py_DECREF(mod);
    } else {
        PyErr_Print();
    }

    sys->active = true;
    return true;
}

void python_system_tick(PythonSystem *sys, void *userdata, float dt) {
    if (!sys || !sys->active) return;
    lframe_handle_keyboard(sys->frames);
    sys->mouse_wheel_consumed = false;
    sys->userdata = userdata;

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    lframe_resolve_layout(sys->frames, sw, sh);

    int cap = lframe_get_capacity(sys->frames);
    for (int i = 0; i < cap; i++) {
        LFrame *f = lframe_get(sys->frames, i);
        if (!f || !f->visible || !f->has_OnUpdate) continue;
        if (sys->py_dispatch_on_update && PyCallable_Check(sys->py_dispatch_on_update)) {
            PyObject *args = PyTuple_Pack(2, PyLong_FromLong(i), PyFloat_FromDouble(dt));
            PyObject *res = PyObject_CallObject(sys->py_dispatch_on_update, args);
            if (!res) PyErr_Print();
            Py_XDECREF(res);
            Py_DECREF(args);
        }
    }
}

void python_system_draw(PythonSystem *sys, void *userdata) {
    if (!sys || !sys->active) return;
    sys->userdata = userdata;
    sys->in_draw = true;

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    lframe_resolve_layout(sys->frames, sw, sh);
    lframe_draw(sys->frames, on_draw_cb, sys);

    sys->in_draw = false;
}

bool python_system_handle_input(PythonSystem *sys, void *userdata) {
    if (!sys || !sys->active) return false;
    sys->userdata = userdata;
    return lframe_handle_input(sys->frames, &g_input_cbs, sys);
}

bool python_system_is_mouse_over_any(const PythonSystem *sys) {
    if (!sys || !sys->active) return false;
    return lframe_is_mouse_over_any(sys->frames);
}

bool python_system_is_wheel_consumed(const PythonSystem *sys) {
    if (!sys || !sys->active) return false;
    return sys->mouse_wheel_consumed;
}

void python_system_fire_event_0(PythonSystem *sys, const char *event_name) {
    if (!sys || !sys->active) return;
    if (sys->py_dispatch_event && PyCallable_Check(sys->py_dispatch_event)) {
        PyObject *args = PyTuple_Pack(1, PyUnicode_FromString(event_name));
        PyObject *res = PyObject_CallObject(sys->py_dispatch_event, args);
        if (!res) PyErr_Print();
        Py_XDECREF(res);
        Py_DECREF(args);
    }
}

void python_system_fire_event_i(PythonSystem *sys, const char *event_name, int arg1) {
    if (!sys || !sys->active) return;
    if (sys->py_dispatch_event && PyCallable_Check(sys->py_dispatch_event)) {
        PyObject *args = PyTuple_Pack(2, PyUnicode_FromString(event_name), PyLong_FromLong(arg1));
        PyObject *res = PyObject_CallObject(sys->py_dispatch_event, args);
        if (!res) PyErr_Print();
        Py_XDECREF(res);
        Py_DECREF(args);
    }
}

void python_system_fire_event_ii(PythonSystem *sys, const char *event_name, int arg1, int arg2) {
    if (!sys || !sys->active) return;
    if (sys->py_dispatch_event && PyCallable_Check(sys->py_dispatch_event)) {
        PyObject *args = PyTuple_Pack(3, PyUnicode_FromString(event_name), PyLong_FromLong(arg1), PyLong_FromLong(arg2));
        PyObject *res = PyObject_CallObject(sys->py_dispatch_event, args);
        if (!res) PyErr_Print();
        Py_XDECREF(res);
        Py_DECREF(args);
    }
}

void python_system_fire_event_s(PythonSystem *sys, const char *event_name, const char *arg1) {
    if (!sys || !sys->active) return;
    if (sys->py_dispatch_event && PyCallable_Check(sys->py_dispatch_event)) {
        PyObject *args = PyTuple_Pack(2, PyUnicode_FromString(event_name), PyUnicode_FromString(arg1 ? arg1 : ""));
        PyObject *res = PyObject_CallObject(sys->py_dispatch_event, args);
        if (!res) PyErr_Print();
        Py_XDECREF(res);
        Py_DECREF(args);
    }
}

void python_system_fire_event(PythonSystem *sys, const char *event_name, const char *fmt, ...) {
    if (!sys || !sys->active) return;
    if (!sys->py_dispatch_event || !PyCallable_Check(sys->py_dispatch_event)) return;

    int nargs = 0;
    if (fmt) nargs = strlen(fmt);

    // Pack args dynamically
    PyObject *py_args = PyTuple_New(1 + nargs);
    PyTuple_SetItem(py_args, 0, PyUnicode_FromString(event_name));

    va_list args;
    va_start(args, fmt);
    for (int i = 0; i < nargs; i++) {
        if (fmt[i] == 'i') {
            PyTuple_SetItem(py_args, 1 + i, PyLong_FromLong(va_arg(args, int)));
        } else if (fmt[i] == 's') {
            const char *s = va_arg(args, const char *);
            PyTuple_SetItem(py_args, 1 + i, PyUnicode_FromString(s ? s : ""));
        } else if (fmt[i] == 'f') {
            PyTuple_SetItem(py_args, 1 + i, PyFloat_FromDouble(va_arg(args, double)));
        } else if (fmt[i] == 'b') {
            PyTuple_SetItem(py_args, 1 + i, PyBool_FromLong(va_arg(args, int)));
        }
    }
    va_end(args);

    PyObject *res = PyObject_CallObject(sys->py_dispatch_event, py_args);
    if (!res) PyErr_Print();
    Py_XDECREF(res);
    Py_DECREF(py_args);
}

struct LFrameSystem *python_system_get_frames(PythonSystem *sys) {
    return sys ? sys->frames : NULL;
}
