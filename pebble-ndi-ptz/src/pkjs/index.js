/*
 * NDI PTZ Remote - phone side (PebbleKit JS).
 *
 * Receives commands from the watch over AppMessage and forwards them as
 * HTTP requests to the NDI PTZ bridge on the local network.
 */
var Clay = require('pebble-clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

/* ---- commands / status (must match src/c/ptz_app.h) ---- */
var CMD = {
  LIST_CAMERAS: 0, MOVE: 1, STOP: 2, PRESET_RECALL: 3, PRESET_STORE: 4,
  HOME: 5, AUTOFOCUS: 6, FOCUS: 7, REFRESH: 8
};
var STATUS = {
  OK: 0, BRIDGE_UNREACHABLE: 1, NO_CAMERAS: 2, LOADING: 3, CMD_FAILED: 4, NOT_CONFIGURED: 5
};

var CONFIG_STORAGE_KEY = 'ndi-ptz-config';
var REQUEST_TIMEOUT_MS = 2500;
var MOVE_TIMEOUT_MS = 1200;

var config = loadConfig();
var cameras = [];

/* ------------------------------------------------------------------ */
/* Config                                                              */
/* ------------------------------------------------------------------ */

function loadConfig() {
  var cfg = { host: '', port: 8765, speed: 1, invertPan: false, invertTilt: false, sensitivity: 5, deadzone: 8 };
  try {
    var raw = localStorage.getItem(CONFIG_STORAGE_KEY);
    if (raw) {
      var saved = JSON.parse(raw);
      for (var k in saved) { if (saved.hasOwnProperty(k)) cfg[k] = saved[k]; }
    }
  } catch (e) {
    console.log('config load failed: ' + e);
  }
  return cfg;
}

function saveConfig() {
  try { localStorage.setItem(CONFIG_STORAGE_KEY, JSON.stringify(config)); } catch (e) {}
}

function baseUrl() {
  if (!config.host) return null;
  var host = String(config.host).trim().replace(/^https?:\/\//, '').replace(/\/+$/, '');
  var port = parseInt(config.port, 10) || 8765;
  if (host.indexOf(':') >= 0) return 'http://' + host;     // host already contains a port
  return 'http://' + host + ':' + port;
}

/* Clay returns values either as plain values or as { value: ... } objects
 * depending on the Clay version, so unwrap defensively. */
function unwrap(v) {
  if (v && typeof v === 'object' && v.hasOwnProperty('value')) return v.value;
  return v;
}

function applyClaySettings(raw) {
  if (!raw) return;
  if (raw.CFG_HOST !== undefined) config.host = String(unwrap(raw.CFG_HOST) || '');
  if (raw.CFG_PORT !== undefined) config.port = parseInt(unwrap(raw.CFG_PORT), 10) || 8765;
  if (raw.CFG_SPEED !== undefined) config.speed = parseInt(unwrap(raw.CFG_SPEED), 10) || 0;
  if (raw.CFG_INVERT_PAN !== undefined) config.invertPan = !!unwrap(raw.CFG_INVERT_PAN);
  if (raw.CFG_INVERT_TILT !== undefined) config.invertTilt = !!unwrap(raw.CFG_INVERT_TILT);
  if (raw.CFG_SENSITIVITY !== undefined) config.sensitivity = parseInt(unwrap(raw.CFG_SENSITIVITY), 10) || 5;
  if (raw.CFG_DEADZONE !== undefined) config.deadzone = parseInt(unwrap(raw.CFG_DEADZONE), 10);
  if (isNaN(config.deadzone)) config.deadzone = 8;
  saveConfig();
}

function sendSettingsToWatch() {
  sendToWatch({
    CFG_SPEED: config.speed,
    CFG_INVERT_PAN: config.invertPan ? 1 : 0,
    CFG_INVERT_TILT: config.invertTilt ? 1 : 0,
    CFG_SENSITIVITY: config.sensitivity,
    CFG_DEADZONE: config.deadzone
  });
}

/* ------------------------------------------------------------------ */
/* AppMessage helpers (sequential queue - one message at a time)       */
/* ------------------------------------------------------------------ */

var outQueue = [];
var outBusy = false;

function sendToWatch(dict) {
  outQueue.push(dict);
  pumpQueue();
}

function pumpQueue() {
  if (outBusy || outQueue.length === 0) return;
  outBusy = true;
  var dict = outQueue.shift();
  Pebble.sendAppMessage(dict,
    function () { outBusy = false; pumpQueue(); },
    function (e) {
      console.log('sendAppMessage failed: ' + JSON.stringify(e));
      outBusy = false;
      pumpQueue();
    });
}

function sendStatus(status, text) {
  sendToWatch({ STATUS: status, STATUS_TEXT: (text || '').substring(0, 40) });
}

function sendCameraList() {
  sendToWatch({ CAM_COUNT: cameras.length });
  for (var i = 0; i < cameras.length; i++) {
    sendToWatch({
      CAM_INDEX: i,
      CAM_NAME: String(cameras[i].name || ('Kamera ' + (i + 1))).substring(0, 36),
      CAM_PTZ: cameras[i].ptz ? 1 : 0
    });
  }
  if (cameras.length === 0) {
    sendStatus(STATUS.NO_CAMERAS, 'Keine NDI-Quellen gefunden');
  } else {
    sendStatus(STATUS.OK, '');
  }
}

/* ------------------------------------------------------------------ */
/* HTTP                                                                */
/* ------------------------------------------------------------------ */

function request(method, path, body, timeoutMs, cb) {
  var base = baseUrl();
  if (!base) {
    cb(new Error('not configured'), null);
    return;
  }
  var xhr = new XMLHttpRequest();
  var done = false;
  var finish = function (err, data) {
    if (done) return;
    done = true;
    cb(err, data);
  };
  xhr.open(method, base + path, true);
  xhr.timeout = timeoutMs || REQUEST_TIMEOUT_MS;
  xhr.setRequestHeader('Content-Type', 'application/json');
  xhr.onload = function () {
    if (xhr.status >= 200 && xhr.status < 300) {
      var data = null;
      try { data = xhr.responseText ? JSON.parse(xhr.responseText) : null; } catch (e) {}
      finish(null, data);
    } else {
      var msg = 'HTTP ' + xhr.status;
      try { var j = JSON.parse(xhr.responseText); if (j && j.error) msg = j.error; } catch (e) {}
      finish(new Error(msg), null);
    }
  };
  xhr.onerror = function () { finish(new Error('network error'), null); };
  xhr.ontimeout = function () { finish(new Error('timeout'), null); };
  try {
    xhr.send(body ? JSON.stringify(body) : null);
  } catch (e) {
    finish(e, null);
  }
}

function fetchCameras(forceRefresh) {
  if (!baseUrl()) {
    sendStatus(STATUS.NOT_CONFIGURED, 'Bridge-IP in Pebble-App setzen');
    return;
  }
  sendStatus(STATUS.LOADING, 'Frage Bridge...');
  var path = forceRefresh ? '/api/cameras?refresh=1' : '/api/cameras';
  request('GET', path, null, forceRefresh ? 8000 : 4000, function (err, data) {
    if (err) {
      console.log('camera list failed: ' + err.message);
      cameras = [];
      sendToWatch({ CAM_COUNT: 0 });
      sendStatus(STATUS.BRIDGE_UNREACHABLE, shortError(err));
      return;
    }
    cameras = (data && data.cameras) ? data.cameras : [];
    // PTZ-capable sources first, keep the bridge's order otherwise
    cameras.sort(function (a, b) { return (b.ptz ? 1 : 0) - (a.ptz ? 1 : 0); });
    if (cameras.length > 12) cameras = cameras.slice(0, 12);
    sendCameraList();
  });
}

function shortError(err) {
  var m = (err && err.message) ? err.message : String(err);
  if (m === 'timeout' || m === 'network error') return 'Bridge antwortet nicht';
  return m;
}

/* Movement: coalesce - at most one MOVE request in flight per camera,
 * the newest values win. */
var moveInFlight = {};
var movePending = {};

function sendMove(camId, pan, tilt, zoom) {
  movePending[camId] = { pan: pan, tilt: tilt, zoom: zoom };
  pumpMove(camId);
}

function pumpMove(camId) {
  if (moveInFlight[camId] || !movePending[camId]) return;
  var m = movePending[camId];
  delete movePending[camId];
  moveInFlight[camId] = true;
  request('POST', '/api/cameras/' + camId + '/move', m, MOVE_TIMEOUT_MS, function (err) {
    moveInFlight[camId] = false;
    if (err) {
      console.log('move failed: ' + err.message);
      // a failed *stop* must be retried so the camera never keeps running
      if (m.pan === 0 && m.tilt === 0 && m.zoom === 0 && !movePending[camId]) {
        movePending[camId] = m;
      }
      sendStatus(STATUS.BRIDGE_UNREACHABLE, shortError(err));
    }
    pumpMove(camId);
  });
}

function simpleCommand(camId, path, body) {
  request('POST', '/api/cameras/' + camId + path, body || {}, REQUEST_TIMEOUT_MS, function (err) {
    if (err) {
      console.log('command ' + path + ' failed: ' + err.message);
      sendStatus(STATUS.CMD_FAILED, shortError(err));
    } else {
      sendStatus(STATUS.OK, '');
    }
  });
}

/* Camera ids: the watch addresses cameras by list index; the bridge uses
 * stable ids, so translate. */
function camIdForIndex(idx) {
  if (idx === undefined || idx === null || idx < 0 || idx >= cameras.length) return null;
  var c = cameras[idx];
  return (c.id !== undefined && c.id !== null) ? c.id : idx;
}

/* ------------------------------------------------------------------ */
/* Watch -> phone                                                      */
/* ------------------------------------------------------------------ */

Pebble.addEventListener('appmessage', function (e) {
  var p = e.payload || {};
  if (p.CMD === undefined) return;
  var cmd = p.CMD;
  var camId = camIdForIndex(p.CAM_INDEX);

  switch (cmd) {
    case CMD.LIST_CAMERAS:
      fetchCameras(false);
      break;
    case CMD.REFRESH:
      fetchCameras(true);
      break;
    case CMD.MOVE:
      if (camId === null) return;
      sendMove(camId, clamp(p.PAN) / 100, clamp(p.TILT) / 100, clamp(p.ZOOM) / 100);
      break;
    case CMD.STOP:
      if (camId === null) return;
      sendMove(camId, 0, 0, 0);
      break;
    case CMD.PRESET_RECALL:
      if (camId === null) return;
      simpleCommand(camId, '/preset/' + (p.PRESET || 1) + '/recall');
      break;
    case CMD.PRESET_STORE:
      if (camId === null) return;
      simpleCommand(camId, '/preset/' + (p.PRESET || 1) + '/store');
      break;
    case CMD.HOME:
      if (camId === null) return;
      simpleCommand(camId, '/home');
      break;
    case CMD.AUTOFOCUS:
      if (camId === null) return;
      simpleCommand(camId, '/autofocus');
      break;
    case CMD.FOCUS:
      if (camId === null) return;
      simpleCommand(camId, '/focus', { speed: clamp(p.ZOOM) / 100 });
      break;
    default:
      console.log('unknown command ' + cmd);
  }
});

function clamp(v) {
  v = parseInt(v, 10);
  if (isNaN(v)) return 0;
  if (v > 100) return 100;
  if (v < -100) return -100;
  return v;
}

/* ------------------------------------------------------------------ */
/* Lifecycle & configuration page                                      */
/* ------------------------------------------------------------------ */

Pebble.addEventListener('ready', function () {
  console.log('NDI PTZ Remote JS ready, bridge=' + baseUrl());
  sendSettingsToWatch();
  // The watch may have asked for the camera list before this JS was ready
  // (that message is lost), so always deliver the list once on startup.
  fetchCameras(false);
});

Pebble.addEventListener('showConfiguration', function () {
  // Pre-fill the page with the stored values.
  clay.setSettings('CFG_HOST', config.host || '');
  clay.setSettings('CFG_PORT', String(config.port || 8765));
  clay.setSettings('CFG_SPEED', String(config.speed));
  clay.setSettings('CFG_INVERT_PAN', !!config.invertPan);
  clay.setSettings('CFG_INVERT_TILT', !!config.invertTilt);
  clay.setSettings('CFG_SENSITIVITY', config.sensitivity);
  clay.setSettings('CFG_DEADZONE', config.deadzone);
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;
  var raw = clay.getSettings(e.response, false);
  applyClaySettings(raw);
  sendSettingsToWatch();
  fetchCameras(false);
});
