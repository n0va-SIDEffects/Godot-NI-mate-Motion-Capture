#!/usr/bin/env python3
"""
NDI PTZ Bridge
==============

Small HTTP server that exposes NDI PTZ camera control as a REST API so that
the Pebble watch app (via PebbleKit JS on the phone) can drive the cameras.

    Pebble watch  --BLE-->  Pebble phone app (JS)  --HTTP-->  this bridge  --NDI SDK-->  cameras

Requirements
------------
* Python 3.8+ (standard library only)
* NDI runtime / SDK installed (https://ndi.video/tools/) - the bridge loads the
  shared library (libndi / Processing.NDI.Lib.x64.dll) via ctypes.
  Use --mock to run without any NDI installation for testing.

Usage
-----
    python ndi_ptz_bridge.py                 # discover NDI sources, serve on 0.0.0.0:8765
    python ndi_ptz_bridge.py --mock          # fake cameras, logs every command
    python ndi_ptz_bridge.py --port 9000 --extra-ips 10.0.0.12,10.0.0.13
    python ndi_ptz_bridge.py --ptz-only      # hide NDI sources without PTZ support

API (all responses JSON)
------------------------
    GET  /api/status
    GET  /api/cameras[?refresh=1]              -> {"cameras":[{"id":0,"name":"...","ptz":true}]}
    POST /api/cameras/<id>/move  {"pan":-1..1,"tilt":-1..1,"zoom":-1..1}   (speeds; all 0 = stop)
    POST /api/cameras/<id>/stop
    POST /api/cameras/<id>/preset/<n>/recall   [{"speed":0..1}]
    POST /api/cameras/<id>/preset/<n>/store
    POST /api/cameras/<id>/home
    POST /api/cameras/<id>/autofocus
    POST /api/cameras/<id>/focus  {"speed":-1..1}
    GET  /                                     -> small browser test page

Safety: if a camera is moving and no new move command arrives within
--watchdog seconds (default 1.5 s), the bridge stops it automatically.
"""

import argparse
import ctypes
import glob
import json
import logging
import os
import platform
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

log = logging.getLogger("ndi-ptz-bridge")


# ----------------------------------------------------------------------------
# NDI SDK bindings (ctypes)
# ----------------------------------------------------------------------------

class NDIlib_source_t(ctypes.Structure):
    _fields_ = [("p_ndi_name", ctypes.c_char_p),
                ("p_url_address", ctypes.c_char_p)]


class NDIlib_find_create_t(ctypes.Structure):
    _fields_ = [("show_local_sources", ctypes.c_bool),
                ("p_groups", ctypes.c_char_p),
                ("p_extra_ips", ctypes.c_char_p)]


class NDIlib_recv_create_v3_t(ctypes.Structure):
    _fields_ = [("source_to_connect_to", NDIlib_source_t),
                ("color_format", ctypes.c_int),
                ("bandwidth", ctypes.c_int),
                ("allow_video_fields", ctypes.c_bool),
                ("p_ndi_recv_name", ctypes.c_char_p)]


class NDIlib_metadata_frame_t(ctypes.Structure):
    _fields_ = [("length", ctypes.c_int),
                ("timecode", ctypes.c_int64),
                ("p_data", ctypes.c_char_p)]


NDIlib_recv_bandwidth_metadata_only = -10
NDIlib_recv_color_format_fastest = 100
NDIlib_frame_type_metadata = 3
NDIlib_frame_type_error = 4


def _candidate_library_paths():
    """Yield plausible locations of the NDI shared library."""
    system = platform.system()
    env_dirs = [os.environ.get(k) for k in ("NDI_RUNTIME_DIR_V6", "NDI_RUNTIME_DIR_V5",
                                             "NDI_RUNTIME_DIR_V4", "NDI_SDK_DIR")]
    if system == "Windows":
        names = ["Processing.NDI.Lib.x64.dll", "Processing.NDI.Lib.x86.dll"]
        dirs = [d for d in env_dirs if d]
        pf = os.environ.get("PROGRAMFILES", r"C:\Program Files")
        dirs += glob.glob(os.path.join(pf, "NDI", "*", "Runtime", "*"))
        dirs += glob.glob(os.path.join(pf, "NDI", "NDI * SDK", "Bin", "x64"))
        dirs += glob.glob(os.path.join(pf, "NDI", "NDI * Runtime", "v*"))
        for d in dirs:
            for n in names:
                yield os.path.join(d, n)
        for n in names:
            yield n  # PATH lookup
    elif system == "Darwin":
        for d in [d for d in env_dirs if d]:
            yield os.path.join(d, "libndi.dylib")
        yield "/Library/NDI SDK for Apple/lib/macOS/libndi.dylib"
        yield "/usr/local/lib/libndi.dylib"
        yield "libndi.dylib"
    else:
        for d in [d for d in env_dirs if d]:
            yield os.path.join(d, "libndi.so")
        for pat in ["/usr/lib/libndi.so*", "/usr/local/lib/libndi.so*",
                    "/usr/lib/x86_64-linux-gnu/libndi.so*", "/usr/lib/aarch64-linux-gnu/libndi.so*",
                    os.path.expanduser("~/NDI*/lib/*/libndi.so*")]:
            for p in sorted(glob.glob(pat), reverse=True):
                yield p
        yield "libndi.so.6"
        yield "libndi.so.5"
        yield "libndi.so"


def load_ndi_library(explicit_path=None):
    paths = [explicit_path] if explicit_path else list(_candidate_library_paths())
    last_err = None
    for p in paths:
        try:
            lib = ctypes.CDLL(p)
            log.info("NDI library loaded: %s", p)
            return lib
        except OSError as e:
            last_err = e
    raise RuntimeError("NDI runtime not found. Install the NDI Tools/Runtime from https://ndi.video "
                       "or point --ndi-lib to the shared library. Last error: %s" % last_err)


class NDI:
    """Thin wrapper around the NDI C API functions we need."""

    def __init__(self, lib):
        self.lib = lib
        L = lib
        L.NDIlib_initialize.restype = ctypes.c_bool
        L.NDIlib_destroy.restype = None

        L.NDIlib_find_create_v2.restype = ctypes.c_void_p
        L.NDIlib_find_create_v2.argtypes = [ctypes.POINTER(NDIlib_find_create_t)]
        L.NDIlib_find_destroy.argtypes = [ctypes.c_void_p]
        L.NDIlib_find_wait_for_sources.restype = ctypes.c_bool
        L.NDIlib_find_wait_for_sources.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
        L.NDIlib_find_get_current_sources.restype = ctypes.POINTER(NDIlib_source_t)
        L.NDIlib_find_get_current_sources.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32)]

        L.NDIlib_recv_create_v3.restype = ctypes.c_void_p
        L.NDIlib_recv_create_v3.argtypes = [ctypes.POINTER(NDIlib_recv_create_v3_t)]
        L.NDIlib_recv_destroy.argtypes = [ctypes.c_void_p]
        L.NDIlib_recv_connect.argtypes = [ctypes.c_void_p, ctypes.POINTER(NDIlib_source_t)]
        L.NDIlib_recv_capture_v3.restype = ctypes.c_int
        L.NDIlib_recv_capture_v3.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p,
                                             ctypes.POINTER(NDIlib_metadata_frame_t), ctypes.c_uint32]
        L.NDIlib_recv_free_metadata.argtypes = [ctypes.c_void_p, ctypes.POINTER(NDIlib_metadata_frame_t)]
        L.NDIlib_recv_get_no_connections.restype = ctypes.c_int
        L.NDIlib_recv_get_no_connections.argtypes = [ctypes.c_void_p]

        for name, args in [
            ("NDIlib_recv_ptz_is_supported", []),
            ("NDIlib_recv_ptz_zoom", [ctypes.c_float]),
            ("NDIlib_recv_ptz_zoom_speed", [ctypes.c_float]),
            ("NDIlib_recv_ptz_pan_tilt", [ctypes.c_float, ctypes.c_float]),
            ("NDIlib_recv_ptz_pan_tilt_speed", [ctypes.c_float, ctypes.c_float]),
            ("NDIlib_recv_ptz_store_preset", [ctypes.c_int]),
            ("NDIlib_recv_ptz_recall_preset", [ctypes.c_int, ctypes.c_float]),
            ("NDIlib_recv_ptz_auto_focus", []),
            ("NDIlib_recv_ptz_focus", [ctypes.c_float]),
            ("NDIlib_recv_ptz_focus_speed", [ctypes.c_float]),
        ]:
            fn = getattr(L, name)
            fn.restype = ctypes.c_bool
            fn.argtypes = [ctypes.c_void_p] + args

        if not L.NDIlib_initialize():
            raise RuntimeError("NDIlib_initialize() failed (CPU not supported?)")


# ----------------------------------------------------------------------------
# Camera backends
# ----------------------------------------------------------------------------

class CameraBase:
    def __init__(self, cam_id, name):
        self.id = cam_id
        self.name = name
        self.ptz = True
        self.lock = threading.Lock()
        self.last_move = (0.0, 0.0, 0.0)
        self.last_move_time = 0.0

    def to_dict(self):
        return {"id": self.id, "name": self.name, "ptz": self.ptz}

    @property
    def moving(self):
        return any(abs(v) > 1e-6 for v in self.last_move)

    # --- to implement ---
    def move(self, pan, tilt, zoom): raise NotImplementedError
    def pan_tilt_absolute(self, pan, tilt): raise NotImplementedError
    def recall_preset(self, n, speed): raise NotImplementedError
    def store_preset(self, n): raise NotImplementedError
    def auto_focus(self): raise NotImplementedError
    def focus_speed(self, speed): raise NotImplementedError
    def close(self): pass


class MockCamera(CameraBase):
    def move(self, pan, tilt, zoom):
        log.info("[%s] move pan=%+.2f tilt=%+.2f zoom=%+.2f", self.name, pan, tilt, zoom)
        return True

    def pan_tilt_absolute(self, pan, tilt):
        log.info("[%s] pan/tilt absolute %.2f/%.2f", self.name, pan, tilt)
        return True

    def recall_preset(self, n, speed):
        log.info("[%s] recall preset %d (speed %.2f)", self.name, n, speed)
        return True

    def store_preset(self, n):
        log.info("[%s] store preset %d", self.name, n)
        return True

    def auto_focus(self):
        log.info("[%s] auto focus", self.name)
        return True

    def focus_speed(self, speed):
        log.info("[%s] focus speed %+.2f", self.name, speed)
        return True


class NDICamera(CameraBase):
    """One NDI receiver (metadata only) connected to a source, used for PTZ."""

    def __init__(self, ndi, cam_id, source_name, source_url):
        super().__init__(cam_id, source_name)
        self.ndi = ndi
        self.source_url = source_url
        self.ptz = False
        self._stop = threading.Event()
        self._name_buf = source_name.encode("utf-8")
        self._url_buf = source_url.encode("utf-8") if source_url else None
        self._recv_name = b"NDI PTZ Bridge"

        src = NDIlib_source_t(self._name_buf, self._url_buf)
        settings = NDIlib_recv_create_v3_t(src, NDIlib_recv_color_format_fastest,
                                           NDIlib_recv_bandwidth_metadata_only, False, self._recv_name)
        self.recv = ndi.lib.NDIlib_recv_create_v3(ctypes.byref(settings))
        if not self.recv:
            raise RuntimeError("NDIlib_recv_create_v3 failed for %s" % source_name)
        self._thread = threading.Thread(target=self._pump, name="ndi-recv-%d" % cam_id, daemon=True)
        self._thread.start()

    def _pump(self):
        """Keep the connection alive and learn PTZ capability from metadata."""
        meta = NDIlib_metadata_frame_t()
        while not self._stop.is_set():
            ft = self.ndi.lib.NDIlib_recv_capture_v3(self.recv, None, None, ctypes.byref(meta), 250)
            if ft == NDIlib_frame_type_metadata:
                self.ndi.lib.NDIlib_recv_free_metadata(self.recv, ctypes.byref(meta))
            supported = bool(self.ndi.lib.NDIlib_recv_ptz_is_supported(self.recv))
            if supported != self.ptz:
                self.ptz = supported
                log.info("[%s] PTZ %s", self.name, "supported" if supported else "not supported")

    def wait_for_capability(self, timeout):
        deadline = time.time() + timeout
        while time.time() < deadline and not self.ptz:
            time.sleep(0.05)
        return self.ptz

    def _call(self, fn, *args):
        with self.lock:
            ok = bool(fn(self.recv, *args))
        if not ok:
            log.warning("[%s] %s failed (camera without PTZ or not connected)", self.name, fn.__name__)
        return ok

    def move(self, pan, tilt, zoom):
        ok = self._call(self.ndi.lib.NDIlib_recv_ptz_pan_tilt_speed, ctypes.c_float(pan), ctypes.c_float(tilt))
        ok = self._call(self.ndi.lib.NDIlib_recv_ptz_zoom_speed, ctypes.c_float(zoom)) and ok
        return ok

    def pan_tilt_absolute(self, pan, tilt):
        return self._call(self.ndi.lib.NDIlib_recv_ptz_pan_tilt, ctypes.c_float(pan), ctypes.c_float(tilt))

    def recall_preset(self, n, speed):
        return self._call(self.ndi.lib.NDIlib_recv_ptz_recall_preset, ctypes.c_int(n), ctypes.c_float(speed))

    def store_preset(self, n):
        return self._call(self.ndi.lib.NDIlib_recv_ptz_store_preset, ctypes.c_int(n))

    def auto_focus(self):
        return self._call(self.ndi.lib.NDIlib_recv_ptz_auto_focus)

    def focus_speed(self, speed):
        return self._call(self.ndi.lib.NDIlib_recv_ptz_focus_speed, ctypes.c_float(speed))

    def close(self):
        self._stop.set()
        self._thread.join(timeout=1.0)
        if self.recv:
            self.ndi.lib.NDIlib_recv_destroy(self.recv)
            self.recv = None


# ----------------------------------------------------------------------------
# Camera manager (discovery + watchdog)
# ----------------------------------------------------------------------------

class CameraManager:
    def __init__(self, args):
        self.args = args
        self.cameras = []          # list[CameraBase], index == id
        self.lock = threading.RLock()
        self.ndi = None
        self.finder = None
        self.last_discovery = 0.0
        self._known = {}           # source name -> NDICamera (kept across refreshes)

        if not args.mock:
            self.ndi = NDI(load_ndi_library(args.ndi_lib))
            create = NDIlib_find_create_t(True,
                                          args.groups.encode() if args.groups else None,
                                          args.extra_ips.encode() if args.extra_ips else None)
            self.finder = self.ndi.lib.NDIlib_find_create_v2(ctypes.byref(create))
            if not self.finder:
                raise RuntimeError("NDIlib_find_create_v2 failed")

        self._wd_thread = threading.Thread(target=self._watchdog, name="watchdog", daemon=True)
        self._wd_thread.start()

    # --- discovery ---

    def discover(self, wait_s=None):
        if self.args.mock:
            with self.lock:
                if not self.cameras:
                    self.cameras = [MockCamera(0, "STUDIO (PTZ Cam 1)"),
                                    MockCamera(1, "STUDIO (PTZ Cam 2)"),
                                    MockCamera(2, "BUEHNE (Totale)")]
                    self.cameras[2].ptz = False
                self.last_discovery = time.time()
            return self.cameras

        wait_s = self.args.discovery_wait if wait_s is None else wait_s
        self.ndi.lib.NDIlib_find_wait_for_sources(self.finder, int(wait_s * 1000))
        n = ctypes.c_uint32(0)
        sources = self.ndi.lib.NDIlib_find_get_current_sources(self.finder, ctypes.byref(n))
        found = []
        for i in range(n.value):
            name = sources[i].p_ndi_name.decode("utf-8", "replace") if sources[i].p_ndi_name else "?"
            url = sources[i].p_url_address.decode("utf-8", "replace") if sources[i].p_url_address else ""
            found.append((name, url))
        log.info("NDI discovery: %d source(s)", len(found))

        with self.lock:
            # create receivers for new sources, keep existing ones
            for name, url in found:
                if name not in self._known:
                    try:
                        cam = NDICamera(self.ndi, len(self._known), name, url)
                    except RuntimeError as e:
                        log.warning("%s", e)
                        continue
                    self._known[name] = cam
            # drop sources that disappeared (only when they are not moving)
            for name in list(self._known):
                if name not in dict(found) and not self._known[name].moving:
                    self._known[name].close()
                    del self._known[name]

            cams = list(self._known.values())
            # give fresh receivers a moment to report PTZ capability
            deadline = time.time() + self.args.capability_wait
            for cam in cams:
                remaining = deadline - time.time()
                if remaining > 0:
                    cam.wait_for_capability(remaining)
            if self.args.ptz_only:
                cams = [c for c in cams if c.ptz]
            cams.sort(key=lambda c: (not c.ptz, c.name.lower()))
            for idx, cam in enumerate(cams):
                cam.id = idx
            self.cameras = cams
            self.last_discovery = time.time()
        return self.cameras

    def get(self, cam_id):
        with self.lock:
            for c in self.cameras:
                if c.id == cam_id:
                    return c
        return None

    # --- safety ---

    def _watchdog(self):
        while True:
            time.sleep(0.2)
            now = time.time()
            with self.lock:
                cams = list(self.cameras)
            for cam in cams:
                if cam.moving and now - cam.last_move_time > self.args.watchdog:
                    log.warning("[%s] watchdog: no update for %.1fs -> STOP", cam.name, now - cam.last_move_time)
                    try:
                        cam.move(0.0, 0.0, 0.0)
                    finally:
                        cam.last_move = (0.0, 0.0, 0.0)

    def stop_all(self):
        with self.lock:
            cams = list(self.cameras)
        for cam in cams:
            if cam.moving:
                cam.move(0.0, 0.0, 0.0)
                cam.last_move = (0.0, 0.0, 0.0)

    def close(self):
        self.stop_all()
        with self.lock:
            for cam in self._known.values():
                cam.close()
            self._known.clear()
            self.cameras = []
        if self.finder:
            self.ndi.lib.NDIlib_find_destroy(self.finder)
            self.finder = None
        if self.ndi:
            self.ndi.lib.NDIlib_destroy()


# ----------------------------------------------------------------------------
# HTTP API
# ----------------------------------------------------------------------------

def clamp(v, lo=-1.0, hi=1.0):
    try:
        v = float(v)
    except (TypeError, ValueError):
        return 0.0
    return max(lo, min(hi, v))


TEST_PAGE = """<!doctype html><html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>NDI PTZ Bridge</title>
<style>body{font-family:system-ui,sans-serif;margin:1.5rem;max-width:40rem}button{font-size:1.1rem;padding:.6rem 1rem;margin:.2rem}
.pad{display:grid;grid-template-columns:repeat(3,4rem);gap:.3rem;margin:1rem 0}.pad button{height:3.2rem}
select{font-size:1rem;padding:.4rem}#log{background:#f3f3f3;padding:.6rem;white-space:pre-wrap;font-family:monospace;font-size:.85rem;min-height:4rem}</style></head>
<body><h1>NDI PTZ Bridge</h1><p>Testseite - Taste gedrückt halten = bewegen, loslassen = stopp.</p>
<label>Kamera <select id="cam"></select></label> <button onclick="refresh()">Neu suchen</button>
<div class="pad"><span></span><button data-m="0,1,0">&#9650;</button><span></span>
<button data-m="-1,0,0">&#9664;</button><button onclick="cmd('home')">Home</button><button data-m="1,0,0">&#9654;</button>
<span></span><button data-m="0,-1,0">&#9660;</button><span></span></div>
<button data-m="0,0,1">Zoom +</button><button data-m="0,0,-1">Zoom -</button> <button onclick="cmd('autofocus')">Autofokus</button>
<p>Preset: <button onclick="cmd('preset/1/recall')">1</button><button onclick="cmd('preset/2/recall')">2</button><button onclick="cmd('preset/3/recall')">3</button>
&nbsp;speichern: <button onclick="cmd('preset/1/store')">1</button><button onclick="cmd('preset/2/store')">2</button><button onclick="cmd('preset/3/store')">3</button></p>
<div id="log"></div>
<script>
var logEl=document.getElementById('log');function log(s){logEl.textContent=(new Date().toLocaleTimeString()+' '+s+'\\n'+logEl.textContent).slice(0,4000)}
function api(m,p,b){return fetch('/api'+p,{method:m,headers:{'Content-Type':'application/json'},body:b?JSON.stringify(b):null}).then(function(r){return r.json().then(function(j){if(!r.ok)throw new Error(j.error||r.status);return j})}).catch(function(e){log('Fehler: '+e.message)})}
function cam(){return document.getElementById('cam').value}
function cmd(p,b){api('POST','/cameras/'+cam()+'/'+p,b).then(function(){log(p+' ok')})}
function move(p,t,z){api('POST','/cameras/'+cam()+'/move',{pan:p,tilt:t,zoom:z})}
function refresh(){api('GET','/cameras?refresh=1').then(fill)}
function fill(j){var s=document.getElementById('cam');s.innerHTML='';(j&&j.cameras||[]).forEach(function(c){var o=document.createElement('option');o.value=c.id;o.textContent=c.name+(c.ptz?'':' (kein PTZ)');s.appendChild(o)});log((j&&j.cameras||[]).length+' Kamera(s)')}
document.querySelectorAll('button[data-m]').forEach(function(b){var v=b.dataset.m.split(',').map(Number);
function down(e){e.preventDefault();move(v[0],v[1],v[2])}function up(e){e.preventDefault();move(0,0,0)}
b.addEventListener('mousedown',down);b.addEventListener('touchstart',down,{passive:false});
b.addEventListener('mouseup',up);b.addEventListener('mouseleave',up);b.addEventListener('touchend',up)});
api('GET','/cameras').then(fill);
</script></body></html>"""


class Handler(BaseHTTPRequestHandler):
    manager = None  # type: CameraManager
    server_version = "NDIPTZBridge/1.0"

    def log_message(self, fmt, *args):
        log.debug("%s - " + fmt, self.address_string(), *args)

    # --- helpers ---

    def _json(self, code, payload):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _error(self, code, msg):
        self._json(code, {"ok": False, "error": msg})

    def _read_body(self):
        """Return the JSON body as dict, or None if the body is not valid JSON."""
        if "chunked" in (self.headers.get("Transfer-Encoding") or "").lower():
            raw = b""
            while True:
                size_line = self.rfile.readline().strip()
                try:
                    size = int(size_line.split(b";")[0], 16)
                except ValueError:
                    return None
                if size == 0:
                    self.rfile.readline()  # trailing CRLF
                    break
                raw += self.rfile.read(size)
                self.rfile.readline()      # CRLF after each chunk
        else:
            length = int(self.headers.get("Content-Length") or 0)
            if length <= 0:
                return {}
            raw = self.rfile.read(length)
        try:
            data = json.loads(raw.decode("utf-8"))
            return data if isinstance(data, dict) else None
        except ValueError:
            return None

    def _camera(self, cam_id_str):
        try:
            cam_id = int(cam_id_str)
        except ValueError:
            self._error(400, "Ungültige Kamera-ID")
            return None
        cam = self.manager.get(cam_id)
        if cam is None:
            self._error(404, "Kamera %d nicht gefunden" % cam_id)
        return cam

    # --- routes ---

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def do_GET(self):
        url = urlparse(self.path)
        parts = [p for p in url.path.split("/") if p]
        qs = parse_qs(url.query)

        if not parts:
            body = TEST_PAGE.encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        if parts == ["api", "status"]:
            with self.manager.lock:
                cams = [c.to_dict() for c in self.manager.cameras]
            self._json(200, {"ok": True, "mock": self.manager.args.mock, "cameras": cams,
                             "last_discovery": self.manager.last_discovery, "time": time.time()})
            return

        if parts == ["api", "cameras"]:
            refresh = qs.get("refresh", ["0"])[0] not in ("0", "", "false")
            stale = time.time() - self.manager.last_discovery > self.manager.args.rediscover_after
            if refresh or stale or not self.manager.cameras:
                cams = self.manager.discover(wait_s=self.manager.args.discovery_wait if (refresh or not self.manager.cameras) else 0.2)
            else:
                cams = self.manager.cameras
            self._json(200, {"ok": True, "cameras": [c.to_dict() for c in cams]})
            return

        self._error(404, "Unbekannter Pfad")

    def do_POST(self):
        url = urlparse(self.path)
        parts = [p for p in url.path.split("/") if p]
        body = self._read_body()
        if body is None:
            self._error(400, "Body ist kein gültiges JSON")
            return

        if parts == ["api", "refresh"]:
            cams = self.manager.discover()
            self._json(200, {"ok": True, "cameras": [c.to_dict() for c in cams]})
            return

        if parts == ["api", "stop_all"]:
            self.manager.stop_all()
            self._json(200, {"ok": True})
            return

        if len(parts) < 4 or parts[0] != "api" or parts[1] != "cameras":
            self._error(404, "Unbekannter Pfad")
            return

        cam = self._camera(parts[2])
        if cam is None:
            return
        action = parts[3]

        try:
            if action == "move":
                pan, tilt, zoom = clamp(body.get("pan", 0)), clamp(body.get("tilt", 0)), clamp(body.get("zoom", 0))
                ok = cam.move(pan, tilt, zoom)
                cam.last_move = (pan, tilt, zoom)
                cam.last_move_time = time.time()
            elif action == "stop":
                ok = cam.move(0.0, 0.0, 0.0)
                cam.last_move = (0.0, 0.0, 0.0)
                cam.last_move_time = time.time()
            elif action == "home":
                ok = cam.pan_tilt_absolute(0.0, 0.0)
            elif action == "autofocus":
                ok = cam.auto_focus()
            elif action == "focus":
                ok = cam.focus_speed(clamp(body.get("speed", 0)))
            elif action == "preset" and len(parts) == 6:
                n = int(parts[4])
                if not 0 <= n <= 99:
                    self._error(400, "Preset muss 0..99 sein")
                    return
                if parts[5] == "recall":
                    ok = cam.recall_preset(n, clamp(body.get("speed", 1.0), 0.0, 1.0))
                elif parts[5] == "store":
                    ok = cam.store_preset(n)
                else:
                    self._error(404, "Unbekannte Preset-Aktion")
                    return
            else:
                self._error(404, "Unbekannte Aktion")
                return
        except ValueError:
            self._error(400, "Ungültiger Parameter")
            return
        except Exception as e:  # never let a camera error kill the server
            log.exception("command failed")
            self._error(500, "Kamerafehler: %s" % e)
            return

        if ok:
            self._json(200, {"ok": True})
        else:
            self._error(502, "Kamera hat den Befehl abgelehnt (PTZ nicht unterstützt?)")


# ----------------------------------------------------------------------------
# main
# ----------------------------------------------------------------------------

def parse_args(argv=None):
    p = argparse.ArgumentParser(description="HTTP bridge for controlling NDI PTZ cameras from a Pebble watch")
    p.add_argument("--host", default="0.0.0.0", help="bind address (default: all interfaces)")
    p.add_argument("--port", type=int, default=8765)
    p.add_argument("--mock", action="store_true", help="simulate cameras, no NDI SDK needed")
    p.add_argument("--ndi-lib", help="explicit path to the NDI shared library")
    p.add_argument("--extra-ips", help="comma separated IPs for NDI discovery across subnets")
    p.add_argument("--groups", help="NDI groups to search (default: public)")
    p.add_argument("--ptz-only", action="store_true", help="only list sources that report PTZ support")
    p.add_argument("--discovery-wait", type=float, default=3.0, help="seconds to wait for NDI sources (default 3)")
    p.add_argument("--capability-wait", type=float, default=2.0, help="seconds to wait for PTZ capability info (default 2)")
    p.add_argument("--rediscover-after", type=float, default=30.0, help="re-run discovery if the list is older than N seconds")
    p.add_argument("--watchdog", type=float, default=1.5, help="stop a moving camera if no update arrives within N seconds")
    p.add_argument("-v", "--verbose", action="store_true")
    return p.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO,
                        format="%(asctime)s %(levelname)-7s %(message)s", datefmt="%H:%M:%S")
    try:
        manager = CameraManager(args)
    except RuntimeError as e:
        log.error("%s", e)
        log.error("Tip: use --mock to test without an NDI installation.")
        return 1

    log.info("Initial NDI discovery...")
    cams = manager.discover()
    for c in cams:
        log.info("  [%d] %s%s", c.id, c.name, "" if c.ptz else "  (kein PTZ)")
    if not cams:
        log.warning("No NDI sources found yet - the watch can trigger a new search.")

    Handler.manager = manager
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    server.daemon_threads = True
    log.info("Bridge listening on http://%s:%d  (test page: open in a browser)", args.host, args.port)
    log.info("Enter this computer's IP and port %d in the watch app settings.", args.port)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        log.info("Shutting down...")
    finally:
        server.server_close()
        manager.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
