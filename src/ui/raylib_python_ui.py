import _raylib_python_ui
import builtins

_frames_cache = {}
_frame_scripts = {}  # fid -> {script_name: handler}
_event_handlers = {} # event_name -> set(fid)

class Frame:
    def __init__(self, fid):
        self.id = fid

    def GetName(self):
        return _raylib_python_ui.GetName(self.id)

    def Hide(self):
        _raylib_python_ui.Hide(self.id)

    def Show(self):
        _raylib_python_ui.Show(self.id)

    def IsVisible(self):
        return _raylib_python_ui.IsVisible(self.id)

    def SetSize(self, w, h):
        _raylib_python_ui.SetSize(self.id, float(w), float(h))

    def GetWidth(self):
        return _raylib_python_ui.GetWidth(self.id)

    def GetHeight(self):
        return _raylib_python_ui.GetHeight(self.id)

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

    def SetBackdropBorderColor(self, r, g, b):
        _raylib_python_ui.SetBackdropBorderColor(self.id, float(r), float(g), float(b))

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

    def GetText(self):
        return _raylib_python_ui.GetText(self.id)

    def SetTextColor(self, r, g, b):
        _raylib_python_ui.SetTextColor(self.id, float(r), float(g), float(b))

    def SetJustifyH(self, justify):
        _raylib_python_ui.SetJustifyH(self.id, justify)

    def SetMinMaxValues(self, min_val, max_val):
        _raylib_python_ui.SetMinMaxValues(self.id, float(min_val), float(max_val))

    def SetValue(self, val):
        _raylib_python_ui.SetValue(self.id, float(val))

    def SetStatusBarColor(self, r, g, b):
        _raylib_python_ui.SetStatusBarColor(self.id, float(r), float(g), float(b))

    def SetHighlightColor(self, r, g, b, a):
        _raylib_python_ui.SetHighlightColor(self.id, float(r), float(g), float(b), float(a))

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
        # Stubs for compatibility
        pass

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
def _dispatch_event(event_name, *args):
    fids = _event_handlers.get(event_name, [])
    for fid in list(fids):
        frame = get_frame_by_id(fid)
        handlers = _frame_scripts.get(fid, {})
        if "OnEvent" in handlers:
            try:
                handlers["OnEvent"](frame, event_name, *args)
            except Exception as e:
                import traceback
                print(f"Error in OnEvent for frame {fid}: {e}")
                traceback.print_exc()

def _dispatch_on_update(fid, elapsed):
    handlers = _frame_scripts.get(fid, {})
    if "OnUpdate" in handlers:
        try:
            handlers["OnUpdate"](get_frame_by_id(fid), elapsed)
        except Exception as e:
            import traceback
            print(f"Error in OnUpdate for frame {fid}: {e}")
            traceback.print_exc()

def _dispatch_on_draw(fid):
    handlers = _frame_scripts.get(fid, {})
    if "OnDraw" in handlers:
        try:
            handlers["OnDraw"](get_frame_by_id(fid))
        except Exception as e:
            import traceback
            print(f"Error in OnDraw for frame {fid}: {e}")
            traceback.print_exc()

def _dispatch_on_click(fid, button):
    handlers = _frame_scripts.get(fid, {})
    if "OnClick" in handlers:
        try:
            handlers["OnClick"](get_frame_by_id(fid), button)
        except Exception as e:
            import traceback
            print(f"Error in OnClick for frame {fid}: {e}")
            traceback.print_exc()

def _dispatch_on_drag_start(fid, button):
    handlers = _frame_scripts.get(fid, {})
    if "OnDragStart" in handlers:
        try:
            handlers["OnDragStart"](get_frame_by_id(fid), button)
        except Exception as e:
            import traceback
            print(f"Error in OnDragStart for frame {fid}: {e}")
            traceback.print_exc()

def _dispatch_on_drag_stop(fid, drag_type, drag_data, dest_fid):
    handlers = _frame_scripts.get(fid, {})
    if "OnDragStop" in handlers:
        dest_frame = get_frame_by_id(dest_fid) if dest_fid >= 0 else None
        try:
            handlers["OnDragStop"](get_frame_by_id(fid), drag_type, drag_data, dest_frame)
        except Exception as e:
            import traceback
            print(f"Error in OnDragStop for frame {fid}: {e}")
            traceback.print_exc()

def _dispatch_on_receive_drag(fid, drag_type, drag_data, src_fid):
    handlers = _frame_scripts.get(fid, {})
    if "OnReceiveDrag" in handlers:
        src_frame = get_frame_by_id(src_fid) if src_fid >= 0 else None
        try:
            handlers["OnReceiveDrag"](get_frame_by_id(fid), drag_type, drag_data, src_frame)
        except Exception as e:
            import traceback
            print(f"Error in OnReceiveDrag for frame {fid}: {e}")
            traceback.print_exc()

def _dispatch_on_enter(fid):
    handlers = _frame_scripts.get(fid, {})
    if "OnEnter" in handlers:
        try:
            handlers["OnEnter"](get_frame_by_id(fid))
        except Exception as e:
            import traceback
            print(f"Error in OnEnter for frame {fid}: {e}")
            traceback.print_exc()

def _dispatch_on_leave(fid):
    handlers = _frame_scripts.get(fid, {})
    if "OnLeave" in handlers:
        try:
            handlers["OnLeave"](get_frame_by_id(fid))
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
    def lower(self, s):
        return s.lower() if s else ""
    def upper(self, s):
        return s.upper() if s else ""
    def len(self, s):
        return len(s) if s else 0

class _TableHelper:
    def insert(self, t, *args):
        if len(args) == 1:
            t.append(args[0])
        elif len(args) == 2:
            t.insert(int(args[0]) - 1, args[1])
    def concat(self, t, sep=""):
        return sep.join(str(x) for x in t)
    def remove(self, t, pos=None):
        if not t:
            return None
        if pos is None:
            return t.pop()
        return t.pop(int(pos) - 1)
    def sort(self, t, comp=None):
        if comp:
            from functools import cmp_to_key
            t.sort(key=cmp_to_key(comp))
        else:
            t.sort()

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

builtins.string = _StringHelper()
builtins.table = _TableHelper()
builtins.math = _MathHelper()
builtins.ipairs = _ipairs
builtins.pairs = _pairs
builtins.tostring = str
builtins.tonumber = _tonumber
builtins.select = _select

