#ifndef RAYLIB_PYTHON_UI_API_H
#define RAYLIB_PYTHON_UI_API_H

#include <stdbool.h>
#include <stdint.h>

typedef struct PythonSystem PythonSystem;

void          python_system_register_inittab(const char *name, void *initfunc);
PythonSystem *python_system_create(void);
void          python_system_destroy(PythonSystem *sys);

/* Pass generic userdata to the PythonSystem. This pointer will be available
 * during updates, drawing, and custom C bindings. */
void          python_system_set_userdata(PythonSystem *sys, void *userdata);
void         *python_system_get_userdata(PythonSystem *sys);

/* Load addons from <addons_dir>/ directory. Returns true if any addons loaded. */
bool python_system_load_addons(PythonSystem *sys, void *userdata, const char *addons_dir);

/* Per-frame update: tick OnUpdate scripts, fire queued events */
void python_system_tick(PythonSystem *sys, void *userdata, float dt);

/* 2D draw pass: draw all frames */
void python_system_draw(PythonSystem *sys, void *userdata);

/* Handle mouse/keyboard input for Python frames. Returns true if input consumed. */
bool python_system_handle_input(PythonSystem *sys, void *userdata);
bool python_system_is_mouse_over_any(const PythonSystem *sys);
bool python_system_is_wheel_consumed(const PythonSystem *sys);

/* Returns true if the Python system is active (addons loaded successfully) */
bool python_system_is_active(const PythonSystem *sys);

/* Dispatch events */
void python_system_fire_event_0(PythonSystem *sys, const char *event_name);
void python_system_fire_event_i(PythonSystem *sys, const char *event_name, int arg1);
void python_system_fire_event_ii(PythonSystem *sys, const char *event_name, int arg1, int arg2);
void python_system_fire_event_s(PythonSystem *sys, const char *event_name, const char *arg1);
void python_system_fire_event(PythonSystem *sys, const char *event_name, const char *fmt, ...);

struct LFrameSystem;
struct LFrameSystem *python_system_get_frames(PythonSystem *sys);

#endif /* RAYLIB_PYTHON_UI_API_H */
