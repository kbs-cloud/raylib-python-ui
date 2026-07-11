import _raylib_python_ui
import builtins
import types

_frames_cache = {}
_frame_scripts = {}  # fid -> {script_name: handler}
_event_handlers = {} # event_name -> set(fid)

class Frame:
    def __init__(self, fid):
        self.id = fid

    def __getattribute__(self, name):
        try:
            attr = object.__getattribute__(self, name)
            if isinstance(attr, types.FunctionType):
                return types.MethodType(attr, self)
            return attr
        except AttributeError:
            return None

    def __getattr__(self, name):
        return None

    def GetName(self):
        return _raylib_python_ui.GetName(self.id)

    def Hide(self):
        _raylib_python_ui.Hide(self.id)

    def Show(self):
        _raylib_python_ui.Show(self.id)

    def SetShown(self, show):
        if show:
            self.Show()
        else:
            self.Hide()

    def IsVisible(self):
        return _raylib_python_ui.IsVisible(self.id)

    def SetSize(self, w, h):
        _raylib_python_ui.SetSize(self.id, float(w), float(h))

    def GetWidth(self):
        return _raylib_python_ui.GetWidth(self.id)

    def GetHeight(self):
        return _raylib_python_ui.GetHeight(self.id)

    def GetRect(self):
        return _raylib_python_ui.GetRect(self.id)

    def SetPoint(self, point, relativeTo, relativePoint, xOfs=0.0, yOfs=0.0):
        rel_id = -1
        if relativeTo is not None:
            if isinstance(relativeTo, Frame):
                rel_id = relativeTo.id
            elif isinstance(relativeTo, int):
                rel_id = relativeTo
        _raylib_python_ui.SetPoint(self.id, point, rel_id, relativePoint, float(xOfs), float(yOfs))

    def ClearAllPoints(self):
        _raylib_python_ui.ClearAllPoints(self.id)

    def SetBackdropColor(self, r, g, b, a):
        _raylib_python_ui.SetBackdropColor(self.id, float(r), float(g), float(b), float(a))

    def SetBackdropBorderColor(self, r, g, b, a=1.0):
        _raylib_python_ui.SetBackdropBorderColor(self.id, float(r), float(g), float(b), float(a))

    def SetBackdropBorderWidth(self, w):
        _raylib_python_ui.SetBackdropBorderWidth(self.id, float(w))

    def SetMovable(self, movable):
        _raylib_python_ui.SetMovable(self.id, bool(movable))

    def EnableMouse(self, enable):
        _raylib_python_ui.EnableMouse(self.id, bool(enable))

    def CreateFontString(self, name=None, layer=None):
        sub_id = _raylib_python_ui.CreateFrame("FontString", name or "", self.id)
        return get_frame_by_id(sub_id)

    def CreateTexture(self, name=None, layer=None):
        sub_id = _raylib_python_ui.CreateFrame("Texture", name or "", self.id)
        return get_frame_by_id(sub_id)

    def CreateEditBox(self, name=None, layer=None):
        sub_id = _raylib_python_ui.CreateFrame("EditBox", name or "", self.id)
        return get_frame_by_id(sub_id)

    def SetText(self, text):
        _raylib_python_ui.SetText(self.id, str(text) if text is not None else "")

    def SetWidth(self, w):
        self.SetSize(float(w), self.GetHeight())

    def SetHeight(self, h):
        self.SetSize(self.GetWidth(), float(h))

    def SetFont(self, size):
        _raylib_python_ui.SetFont(self.id, float(size))

    def SetAllPoints(self, relativeTo=None):
        _raylib_python_ui.SetAllPoints(self.id, relativeTo)

    def GetText(self):
        return _raylib_python_ui.GetText(self.id)

    def SetTextColor(self, r, g, b, a=1.0):
        _raylib_python_ui.SetTextColor(self.id, float(r), float(g), float(b))

    def SetJustifyH(self, justify):
        _raylib_python_ui.SetJustifyH(self.id, justify)

    def SetMinMaxValues(self, min_val, max_val):
        _raylib_python_ui.SetMinMaxValues(self.id, float(min_val), float(max_val))

    def SetValue(self, val):
        _raylib_python_ui.SetValue(self.id, float(val))

    def SetStatusBarColor(self, r, g, b, a=1.0):
        _raylib_python_ui.SetStatusBarColor(self.id, float(r), float(g), float(b))

    def SetHighlightColor(self, r, g, b, a):
        _raylib_python_ui.SetHighlightColor(self.id, float(r), float(g), float(b), float(a))

    def SetResizable(self, resizable):
        _raylib_python_ui.SetResizable(self.id, bool(resizable))

    def IsResizable(self):
        return _raylib_python_ui.IsResizable(self.id)

    def SetScript(self, script_name, handler):
        if handler:
            _frame_scripts.setdefault(self.id, {})[script_name] = handler
            _raylib_python_ui.SetScript(self.id, script_name, True)
        else:
            if self.id in _frame_scripts and script_name in _frame_scripts[self.id]:
                del _frame_scripts[self.id][script_name]
            _raylib_python_ui.SetScript(self.id, script_name, False)

    def RegisterEvent(self, event_name):
        _raylib_python_ui.RegisterEvent(self.id, event_name)
        _event_handlers.setdefault(event_name, set()).add(self.id)

    def SetFrameStrata(self, strata):
        _raylib_python_ui.SetFrameStrata(self.id, strata)

    def IsShown(self):
        return self.IsVisible()

    def RegisterForDrag(self, *buttons):
        # Pass up to two buttons to the C API
        btn1 = buttons[0] if len(buttons) > 0 else None
        btn2 = buttons[1] if len(buttons) > 1 else None
        _raylib_python_ui.RegisterForDrag(self.id, btn1, btn2)

    def SetPreEscaped(self, val):
        _raylib_python_ui.SetPreEscaped(self.id, val)

    def IsPreEscaped(self):
        return _raylib_python_ui.IsPreEscaped(self.id)

def get_frame_by_id(fid):
    if fid == 0:
        return UIParent
    if fid not in _frames_cache:
        _frames_cache[fid] = Frame(fid)
    return _frames_cache[fid]

UIParent = Frame(0)
_frames_cache[0] = UIParent

FrameTemplates = {}

def CreateFrame(type_str, name=None, parent=None, template=None):
    parent_id = 0
    if parent is not None:
        if isinstance(parent, Frame):
            parent_id = parent.id
        elif isinstance(parent, int):
            parent_id = parent
    
    fid = _raylib_python_ui.CreateFrame(type_str, name or "", parent_id)
    if fid < 0:
        return None
        
    frame = get_frame_by_id(fid)
    if name:
        setattr(builtins, name, frame)
        
    if template and template in FrameTemplates:
        tmpl = FrameTemplates[template]
        if "OnLoad" in tmpl:
            tmpl["OnLoad"](frame)
            
    return frame

# Callback dispatchers
def _safe_call(handler, *args):
    func = handler
    if hasattr(handler, "__func__"):
        func = handler.__func__
    if hasattr(func, "__code__"):
        co_argcount = func.__code__.co_argcount
        if hasattr(handler, "__self__"):
            limit = co_argcount - 1
        else:
            limit = co_argcount
        if func.__code__.co_flags & 0x04:
            return handler(*args)
        return handler(*args[:limit])
    return handler(*args)

def _dispatch_event(event_name, *args):
    fids = _event_handlers.get(event_name, [])
    for fid in list(fids):
        frame = get_frame_by_id(fid)
        handlers = _frame_scripts.get(fid, {})
        if "OnEvent" in handlers:
            try:
                _safe_call(handlers["OnEvent"], frame, event_name, *args)
            except Exception as e:
                import traceback
                print(f"Error in OnEvent for frame {fid}: {e}")
                traceback.print_exc()

def _dispatch_on_update(fid, elapsed):
    handlers = _frame_scripts.get(fid, {})
    if "OnUpdate" in handlers:
        try:
            _safe_call(handlers["OnUpdate"], get_frame_by_id(fid), elapsed)
        except Exception as e:
            import traceback
            print(f"Error in OnUpdate for frame {fid}: {e}")
            traceback.print_exc()

def _dispatch_on_draw(fid):
    handlers = _frame_scripts.get(fid, {})
    if "OnDraw" in handlers:
        try:
            _safe_call(handlers["OnDraw"], get_frame_by_id(fid))
        except Exception as e:
            import traceback
            print(f"Error in OnDraw for frame {fid}: {e}")
            traceback.print_exc()

def _dispatch_on_click(fid, button):
    handlers = _frame_scripts.get(fid, {})
    if "OnClick" in handlers:
        try:
            _safe_call(handlers["OnClick"], get_frame_by_id(fid), button)
        except Exception as e:
            import traceback
            print(f"Error in OnClick for frame {fid}: {e}")
            traceback.print_exc()

def _dispatch_on_drag_start(fid, button):
    handlers = _frame_scripts.get(fid, {})
    if "OnDragStart" in handlers:
        try:
            _safe_call(handlers["OnDragStart"], get_frame_by_id(fid), button)
        except Exception as e:
            import traceback
            print(f"Error in OnDragStart for frame {fid}: {e}")
            traceback.print_exc()

def _dispatch_on_drag_stop(fid, drag_type, drag_data, dest_fid):
    handlers = _frame_scripts.get(fid, {})
    if "OnDragStop" in handlers:
        dest_frame = get_frame_by_id(dest_fid) if dest_fid >= 0 else None
        try:
            _safe_call(handlers["OnDragStop"], get_frame_by_id(fid), drag_type, drag_data, dest_frame)
        except Exception as e:
            import traceback
            print(f"Error in OnDragStop for frame {fid}: {e}")
            traceback.print_exc()

def _dispatch_on_receive_drag(fid, drag_type, drag_data, src_fid):
    handlers = _frame_scripts.get(fid, {})
    if "OnReceiveDrag" in handlers:
        src_frame = get_frame_by_id(src_fid) if src_fid >= 0 else None
        try:
            _safe_call(handlers["OnReceiveDrag"], get_frame_by_id(fid), drag_type, drag_data, src_frame)
        except Exception as e:
            import traceback
            print(f"Error in OnReceiveDrag for frame {fid}: {e}")
            traceback.print_exc()

def _dispatch_on_enter(fid):
    handlers = _frame_scripts.get(fid, {})
    if "OnEnter" in handlers:
        try:
            _safe_call(handlers["OnEnter"], get_frame_by_id(fid))
        except Exception as e:
            import traceback
            print(f"Error in OnEnter for frame {fid}: {e}")
            traceback.print_exc()

def _dispatch_on_leave(fid):
    handlers = _frame_scripts.get(fid, {})
    if "OnLeave" in handlers:
        try:
            _safe_call(handlers["OnLeave"], get_frame_by_id(fid))
        except Exception as e:
            import traceback
            print(f"Error in OnLeave for frame {fid}: {e}")
            traceback.print_exc()


# =========================================================================
# Lua Compatibility Helpers for Python
# =========================================================================
import math as _math
import random as _random

class _StringHelper:
    def len(self, s):
        if s is None:
            return 0
        return len(s)
    def format(self, fmt, *args):
        if not args:
            return fmt
        if len(args) == 1 and isinstance(args[0], (list, tuple)):
            return fmt % tuple(args[0])
        return fmt % args
    def sub(self, s, start, end=None):
        if s is None:
            return ""
        start = int(start)
        if start > 0:
            start_idx = start - 1
        elif start < 0:
            start_idx = len(s) + start
        else:
            start_idx = 0
            
        if end is None:
            return s[start_idx:]
        
        end = int(end)
        if end > 0:
            end_idx = end
        elif end < 0:
            end_idx = len(s) + end + 1
        else:
            end_idx = 0
        return s[start_idx:end_idx]
    def find(self, s, pattern, init=1, plain=False):
        if s is None or pattern is None:
            return None
        idx = s.find(pattern, init - 1)
        if idx == -1:
            return None
        return idx + 1, idx + len(pattern)
    def gmatch(self, s, pattern):
        import re as _re
        classes = {
            'a': '[a-zA-Z]',
            'c': '[\\x00-\\x1f\\x7f]',
            'd': '\\d',
            'l': '[a-z]',
            'p': '[!-/:-@\\[-`{-~]',
            's': '\\s',
            'u': '[A-Z]',
            'w': '\\w',
            'x': '[0-9a-fA-F]',
            'z': '\\x00'
        }
        res = []
        i = 0
        while i < len(pattern):
            c = pattern[i]
            if c == '%':
                if i + 1 < len(pattern):
                    nc = pattern[i+1]
                    nl = nc.lower()
                    if nl in classes:
                        repl = classes[nl]
                        if nc.isupper():
                            if repl.startswith('[') and not repl.startswith('[^'):
                                repl = '[^' + repl[1:]
                            elif repl.startswith('\\'):
                                repl = '[^' + repl + ']'
                        res.append(repl)
                    else:
                        if nc in '.^$*+?()[]{}|\\-':
                            res.append('\\' + nc)
                        else:
                            res.append(nc)
                    i += 2
                else:
                    res.append('%')
                    i += 1
            elif c in '{}|\\':
                res.append('\\' + c)
                i += 1
            else:
                res.append(c)
                i += 1
        py_pattern = "".join(res)
        matches = _re.findall(py_pattern, s)
        return matches
    def lower(self, s):
        return s.lower() if s else ""
    def upper(self, s):
        return s.upper() if s else ""
    def sub(self, s, i, j=None):
        if not s:
            return ""
        i = int(i)
        if i > 0:
            start = i - 1
        elif i < 0:
            start = len(s) + i
        else:
            start = 0
            
        if j is None:
            return s[start:]
        else:
            j = int(j)
            if j > 0:
                end = j
            elif j < 0:
                end = len(s) + j + 1
            else:
                end = 0
            return s[start:end]

class _TableHelper:
    def insert(self, t, *args):
        if isinstance(t, list):
            if len(args) == 1:
                t.append(args[0])
            elif len(args) == 2:
                t.insert(int(args[0]) - 1, args[1])
        elif isinstance(t, dict):
            if len(args) == 1:
                idx = 1
                while idx in t:
                    idx += 1
                t[idx] = args[0]
            elif len(args) == 2:
                pos = int(args[0])
                idx = 1
                while idx in t:
                    idx += 1
                while idx > pos:
                    t[idx] = t[idx - 1]
                    idx -= 1
                t[pos] = args[1]
    def concat(self, t, sep=""):
        if isinstance(t, dict):
            res = []
            i = 1
            while i in t:
                res.append(t[i])
                i += 1
            return sep.join(str(x) for x in res)
        return sep.join(str(x) for x in t)
    def remove(self, t, pos=None):
        if not t:
            return None
        if isinstance(t, list):
            if pos is None:
                return t.pop()
            return t.pop(int(pos) - 1)
        elif isinstance(t, dict):
            if pos is None:
                idx = 1
                while idx in t:
                    idx += 1
                return t.pop(idx - 1, None)
            val = t.pop(int(pos), None)
            # shift left
            idx = int(pos)
            while idx + 1 in t:
                t[idx] = t[idx + 1]
                idx += 1
            t.pop(idx, None)
            return val
    def sort(self, t, comp=None):
        if isinstance(t, list):
            if comp:
                from functools import cmp_to_key
                t.sort(key=cmp_to_key(comp))
            else:
                t.sort()
        elif isinstance(t, dict):
            # Sort the values and re-key them 1..N
            keys = sorted(t.keys())
            vals = [t[k] for k in keys]
            if comp:
                from functools import cmp_to_key
                vals.sort(key=cmp_to_key(comp))
            else:
                vals.sort()
            t.clear()
            for i, v in enumerate(vals, 1):
                t[i] = v

def _ipairs(t):
    if t is None:
        return []
    if isinstance(t, dict):
        res = []
        i = 1
        while i in t:
            res.append((i, t[i]))
            i += 1
        return res
    if isinstance(t, (list, tuple)):
        return list(enumerate(t, 1))
    return []

def _pairs(t):
    if t is None:
        return []
    if isinstance(t, dict):
        return list(t.items())
    if isinstance(t, (list, tuple)):
        return list(enumerate(t, 1))
    return []

def _tonumber(x):
    try:
        s = str(x)
        if '.' in s:
            return float(s)
        return int(s)
    except:
        return None

def _select(index, *args):
    if index == '#':
        return len(args)
    idx = int(index)
    if idx > 0:
        return args[idx-1:]
    return ()

class _MathHelper:
    def floor(self, x):
        return _math.floor(x)
    def ceil(self, x):
        return _math.ceil(x)
    def abs(self, x):
        return abs(x)
    def sqrt(self, x):
        return _math.sqrt(x)
    def sin(self, x):
        return _math.sin(x)
    def cos(self, x):
        return _math.cos(x)
    def atan2(self, y, x):
        return _math.atan2(y, x)
    def random(self, *args):
        if not args:
            return _random.random()
        if len(args) == 1:
            return _random.randint(1, int(args[0]))
        return _random.randint(int(args[0]), int(args[1]))
    def max(self, *args):
        return max(*args)
    def min(self, *args):
        return min(*args)
    @property
    def pi(self):
        return _math.pi

class _JsonHelper:
    def gmatch(self, s, pattern):
        import re as _re
        py_pattern = pattern.replace('%d', '\\d').replace('%s', '\\s').replace('%w', '\\w')
        return _re.findall(py_pattern, s)

class Table(dict):
    def __init__(self, *args, **kwargs):
        super().__init__()
        if args:
            if isinstance(args[0], (list, tuple, set)):
                for i, val in enumerate(args[0], 1):
                    self[i] = val
            elif isinstance(args[0], dict):
                self.update(args[0])
            else:
                self.update(args[0])
        self.update(kwargs)

    def __getitem__(self, key):
        return self.get(key, None)
    def __getattr__(self, key):
        return self.get(key, None)
    def __setattr__(self, key, value):
        self[key] = value

class _G_Class:
    def __getitem__(self, key):
        return getattr(builtins, key, None)
    def __setitem__(self, key, value):
        setattr(builtins, key, value)
    def get(self, key, default=None):
        return getattr(builtins, key, default)

builtins.string = _StringHelper()
builtins.table = _TableHelper()
builtins.math = _MathHelper()
builtins.ipairs = _ipairs
builtins.pairs = _pairs
builtins._G = _G_Class()
builtins.tostring = str
builtins.tonumber = _tonumber
builtins.select = _select
builtins.json = _JsonHelper()
builtins.Table = Table

def _unpack(*args):
    if not args:
        return ()
    n = args[-1]
    vals = args[:-1]
    if len(vals) == 1:
        val = vals[0]
        if isinstance(val, (list, tuple)):
            res = list(val[:n])
            while len(res) < n:
                res.append(None)
            return tuple(res)
        elif isinstance(val, dict):
            res = []
            for i in range(1, n + 1):
                res.append(val.get(i, None))
            return tuple(res)
        res = [val]
        while len(res) < n:
            res.append(None)
        return tuple(res)
    else:
        res = list(vals[:n])
        while len(res) < n:
            res.append(None)
        return tuple(res)

builtins._unpack = _unpack

# Shared game state globals
builtins.INVENTORY_SLOTS = 24
builtins.GLIMMERWOOD_BAG_OPEN = False
builtins.GLIMMERWOOD_HELD_ITEM = None
builtins.GLIMMERWOOD_HOVERED_INV_SLOT = 0
builtins.earlyLogBuffer = {}
builtins.GLIMMERWOOD_AB_PROFILES = {}

# Intercept and wrap glimmerwood C-extension dict return values
import sys
if "glimmerwood" in sys.modules:
    import functools
    
    def wrap_to_table(val):
        if isinstance(val, dict):
            if type(val) is Table:
                return val
            t = Table()
            for k, v in val.items():
                t[k] = wrap_to_table(v)
            return t
        elif isinstance(val, list):
            return [wrap_to_table(x) for x in val]
        elif isinstance(val, tuple):
            return tuple(wrap_to_table(x) for x in val)
        return val

    def wrap_function(func):
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            return wrap_to_table(func(*args, **kwargs))
        return wrapper

    glimmerwood = sys.modules["glimmerwood"]
    for name in dir(glimmerwood):
        if name.startswith("_"):
            continue
        obj = getattr(glimmerwood, name)
        for attr_name in dir(obj):
            if attr_name.startswith("_"):
                continue
            try:
                attr = getattr(obj, attr_name)
                if callable(attr):
                    setattr(obj, attr_name, wrap_function(attr))
            except Exception:
                pass
        if callable(obj):
            try:
                setattr(glimmerwood, name, wrap_function(obj))
            except Exception:
                pass

