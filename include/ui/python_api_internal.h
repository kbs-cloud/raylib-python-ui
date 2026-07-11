#ifndef RAYLIB_PYTHON_UI_API_INTERNAL_H
#define RAYLIB_PYTHON_UI_API_INTERNAL_H

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "raylib.h"
#include "raymath.h"

#include "ui/python_api.h"
#include "ui/python_frame.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

struct PythonSystem {
    LFrameSystem *frames;
    void         *userdata;
    bool          active;
    bool          in_draw;
    bool          mouse_wheel_consumed;
    PyObject     *py_dispatch_on_click;
    PyObject     *py_dispatch_on_drag_start;
    PyObject     *py_dispatch_on_drag_stop;
    PyObject     *py_dispatch_on_receive_drag;
    PyObject     *py_dispatch_on_enter;
    PyObject     *py_dispatch_on_leave;
    PyObject     *py_dispatch_on_update;
    PyObject     *py_dispatch_on_draw;
    PyObject     *py_dispatch_event;
};

PyObject* PyInit__raylib_python_ui(void);

#endif /* RAYLIB_PYTHON_UI_API_INTERNAL_H */
