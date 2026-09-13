/*
 * HELO Remote - PebbleKit JS side.
 *
 * Runs on the phone. Talks to the AJA HELO REST API over HTTP and relays a
 * compact status dictionary to the watch. Receives CMD values from the watch
 * and translates them into eParamID_ReplicatorCommand set-requests.
 *
 * HELO REST API cheat sheet (AJA "REST API Developer Guide"):
 *   GET  http://<helo>/config?action=get&paramid=<eParamID>
 *        -> {"paramid":"...","name":"...","value":"2","value_name":"eRRSRecording"}
 *   GET  http://<helo>/config?action=set&paramid=<eParamID>&value=<n>
 *   POST http://<helo>/authenticator/login   (password_provided=<pw>)  when auth is on
 *
 *   eParamID_ReplicatorCommand: 1 = Start Record, 2 = Stop Record,
 *                               3 = Start Stream, 4 = Stop Stream
 *   eParamID_ReplicatorRecordState / eParamID_ReplicatorStreamState:
 *        0 Uninitialized, 1 Idle, 2 Recording/Streaming,
 *        3 Failed (idle), 4 Failed (while active), 5 Shutdown
 */
var Clay = require('pebble-clay');
var buildConfig = require('./config');
var customClay = require('./custom-clay');
var i18n = require('./i18n');
var clay = new Clay(buildConfig('en'), customClay, { autoHandleEvents: false });

/* ---- Protocol constants (must match src/c/main.c) ---------------------- */
var CMD = { REFRESH: 0, REC_START: 1, REC_STOP: 2, STREAM_START: 3, STREAM_STOP: 4 };
var CONN = { UNKNOWN: 0, OK: 1, OFFLINE: 2, AUTH: 3, NOCONFIG: 4 };

var PARAM = {
  COMMAND:       'eParamID_ReplicatorCommand',
  REC_STATE:     'eParamID_ReplicatorRecordState',
  STREAM_STATE:  'eParamID_ReplicatorStreamState',
  REC_DURATION:  'eParamID_RecordingDuration',
  STREAM_DURATION: 'eParamID_StreamingDuration',
  MEDIA_AVAIL:   'eParamID_CurrentMediaAvailable',
  TEMPERATURE:   'eParamID_Temperature',
  SYS_NAME:      'eParamID_SysName'
};

/* States 0..5 are translated on the watch; the phone only forwards names of
 * enum values it does not know (see stateName). */
var KNOWN_STATES = { 0: true, 1: true, 2: true, 3: true, 4: true, 5: true };

var SETTINGS_KEY = 'helo_remote_settings';
var REQUEST_TIMEOUT_MS = 4000;

/* ---- Settings ----------------------------------------------------------- */
var settings = {
  host: '',
  port: 80,
  password: '',
  poll: 3,
  vibrate: true,
  lang: 0            // 0 = automatic, 1..n = index into i18n.langs + 1
};

/* ---- Language ----------------------------------------------------------- */
function langFromTag(tag) {
  var code = null;
  if (tag) {
    i18n.langs.forEach(function (c) { if (!code && new RegExp('^' + c, 'i').test(String(tag))) code = c; });
  }
  return code;
}

/* Resolved language code: fixed setting, else the watch's language, else the phone's, else English. */
function currentLang() {
  if (settings.lang >= 1 && settings.lang <= i18n.langs.length) return i18n.langs[settings.lang - 1];
  var code = null;
  try {
    var info = Pebble.getActiveWatchInfo && Pebble.getActiveWatchInfo();
    code = langFromTag(info && info.language);
  } catch (e) { /* older runtimes */ }
  if (!code && typeof navigator !== 'undefined') code = langFromTag(navigator.language);
  return code || 'en';
}

function T() { return i18n.phone[currentLang()]; }

/* Language index sent to the watch so both sides show the same language. */
function langIndex() { return i18n.langs.indexOf(currentLang()) + 1; }

function loadSettings() {
  try {
    var raw = localStorage.getItem(SETTINGS_KEY);
    if (raw) {
      var stored = JSON.parse(raw);
      for (var k in stored) {
        if (stored.hasOwnProperty(k)) settings[k] = stored[k];
      }
    }
  } catch (e) {
    console.log('settings load failed: ' + e);
  }
  sanitizeSettings();
}

function saveSettings() {
  try {
    localStorage.setItem(SETTINGS_KEY, JSON.stringify(settings));
  } catch (e) {
    console.log('settings save failed: ' + e);
  }
}

function sanitizeSettings() {
  settings.host = String(settings.host || '').trim().replace(/^https?:\/\//, '').replace(/\/+$/, '');
  var port = parseInt(settings.port, 10);
  settings.port = (port > 0 && port < 65536) ? port : 80;
  var poll = parseInt(settings.poll, 10);
  settings.poll = (poll >= 1 && poll <= 60) ? poll : 3;
  settings.password = String(settings.password || '');
  settings.vibrate = !!settings.vibrate;
  var lang = parseInt(settings.lang, 10);
  settings.lang = (lang >= 0 && lang <= i18n.langs.length) ? lang : 0;
}

function clayValue(v) {
  // Clay returns { value: x, precision: y } for some items, plain values otherwise.
  return (v !== null && typeof v === 'object' && 'value' in v) ? v.value : v;
}

function applyClaySettings(dict) {
  if (!dict) return;
  if ('HELO_HOST' in dict)     settings.host = clayValue(dict.HELO_HOST);
  if ('HELO_PORT' in dict)     settings.port = clayValue(dict.HELO_PORT);
  if ('HELO_PASSWORD' in dict) settings.password = clayValue(dict.HELO_PASSWORD);
  if ('POLL_INTERVAL' in dict) settings.poll = clayValue(dict.POLL_INTERVAL);
  if ('VIBRATE' in dict)       settings.vibrate = clayValue(dict.VIBRATE);
  if ('LANGUAGE' in dict)      settings.lang = clayValue(dict.LANGUAGE);
  sanitizeSettings();
  saveSettings();
}

/* ---- AppMessage queue --------------------------------------------------- */
var outQueue = [];
var sending = false;

function sendToWatch(dict) {
  // Status messages supersede each other: keep the queue short.
  if (outQueue.length >= 3) outQueue.shift();
  outQueue.push(dict);
  pumpQueue();
}

function pumpQueue() {
  if (sending || outQueue.length === 0) return;
  sending = true;
  var dict = outQueue.shift();
  Pebble.sendAppMessage(dict,
    function () { sending = false; pumpQueue(); },
    function (e) {
      console.log('sendAppMessage failed: ' + JSON.stringify(e && e.error));
      sending = false;
      pumpQueue();
    });
}

function sendStatus(conn, extra) {
  var dict = { CONN: conn, VIBRATE: settings.vibrate ? 1 : 0, LANGUAGE: langIndex() };
  if (extra) {
    for (var k in extra) {
      if (extra.hasOwnProperty(k)) dict[k] = extra[k];
    }
  }
  sendToWatch(dict);
}

function sendMessage(text) {
  sendToWatch({ MESSAGE: String(text).substr(0, 40) });
}

/* ---- HTTP --------------------------------------------------------------- */
var sessionCookie = null;

function baseUrl() {
  return 'http://' + settings.host + ':' + settings.port;
}

function httpRequest(method, path, body, cb) {
  var xhr = new XMLHttpRequest();
  var done = false;
  function finish(err, status, text) {
    if (done) return;
    done = true;
    cb(err, status, text, xhr);
  }
  try {
    xhr.open(method, baseUrl() + path, true);
    xhr.timeout = REQUEST_TIMEOUT_MS;
    xhr.onload = function () { finish(null, xhr.status, xhr.responseText); };
    xhr.onerror = function () { finish(new Error('network')); };
    xhr.ontimeout = function () { finish(new Error('timeout')); };
    if (body) xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
    if (sessionCookie) {
      // Some JS runtimes forbid setting Cookie manually; they handle cookies themselves.
      try { xhr.setRequestHeader('Cookie', sessionCookie); } catch (e) { /* ignore */ }
    }
    xhr.send(body || null);
  } catch (e) {
    finish(e);
  }
}

function parseJson(text) {
  if (!text) return null;
  try { return JSON.parse(text); } catch (e) { return null; }
}

function looksLikeLoginPage(status, text) {
  if (status === 401 || status === 403) return true;
  if (!text) return false;
  var t = text.trim();
  return t.charAt(0) === '<' || /authenticator/i.test(t.substr(0, 400));
}

function login(cb) {
  if (!settings.password) return cb(new Error('auth'));
  var body = 'password_provided=' + encodeURIComponent(settings.password);
  httpRequest('POST', '/authenticator/login', body, function (err, status, text, xhr) {
    if (err) return cb(err);
    var json = parseJson(text);
    if (!json || json.login !== 'success') return cb(new Error('auth'));
    try {
      var setCookie = xhr.getResponseHeader('Set-Cookie');
      if (setCookie) sessionCookie = setCookie.split(';')[0];
    } catch (e) { /* header not exposed, runtime handles cookies */ }
    cb(null);
  });
}

/* GET one parameter. Retries once after logging in when the HELO asks for auth. */
function getParam(paramId, cb, isRetry) {
  httpRequest('GET', '/config?action=get&paramid=' + paramId, null, function (err, status, text) {
    if (err) return cb(err);
    var json = parseJson(text);
    if (json && (json.value !== undefined)) return cb(null, json);
    if (!isRetry && looksLikeLoginPage(status, text)) {
      return login(function (lerr) {
        if (lerr) return cb(lerr);
        getParam(paramId, cb, true);
      });
    }
    cb(new Error('badresponse'));
  });
}

function setParam(paramId, value, cb, isRetry) {
  var path = '/config?action=set&paramid=' + paramId + '&value=' + encodeURIComponent(value);
  httpRequest('GET', path, null, function (err, status, text) {
    if (err) return cb(err);
    if (!isRetry && looksLikeLoginPage(status, text)) {
      return login(function (lerr) {
        if (lerr) return cb(lerr);
        setParam(paramId, value, cb, true);
      });
    }
    if (status >= 200 && status < 300) return cb(null, parseJson(text));
    cb(new Error('http' + status));
  });
}

/* ---- Value formatting --------------------------------------------------- */
function toInt(v, fallback) {
  var n = parseInt(v, 10);
  return isNaN(n) ? fallback : n;
}

function stateName(json) {
  var n = toInt(json.value, -1);
  if (KNOWN_STATES[n]) return '';                       // watch translates these itself
  var vn = String(json.value_name || '');
  return vn.replace(/^e(RRS|RSS|RS)/, '') || ('?' + n);
}

function pad2(n) { return (n < 10 ? '0' : '') + n; }

/* HELO reports durations as "HH:MM:SS:FF" (timecode) or seconds; normalise to HH:MM:SS. */
function formatDuration(v) {
  if (v === undefined || v === null) return '';
  var s = String(v).trim();
  var m = s.match(/^(\d+):(\d\d):(\d\d)(?::\d+)?$/);
  if (m) return m[1] + ':' + m[2] + ':' + m[3];
  var secs = parseInt(s, 10);
  if (!isNaN(secs) && /^\d+$/.test(s)) {
    var h = Math.floor(secs / 3600), mi = Math.floor((secs % 3600) / 60), se = secs % 60;
    return pad2(h) + ':' + pad2(mi) + ':' + pad2(se);
  }
  return s.substr(0, 12);
}

/* ---- Polling ------------------------------------------------------------ */
var pollTimer = null;
var polling = false;
var sysNameCache = null;
var consecutiveErrors = 0;

function series(tasks, done) {
  var i = 0;
  function next() {
    if (i >= tasks.length) return done();
    tasks[i++](next);
  }
  next();
}

function pollOnce(reason) {
  if (!settings.host) {
    sendStatus(CONN.NOCONFIG);
    return;
  }
  if (polling) return;
  polling = true;

  var status = {};
  var essentialError = null;
  var authError = false;

  function essential(paramId, apply) {
    return function (next) {
      if (essentialError) return next();
      getParam(paramId, function (err, json) {
        if (err) {
          essentialError = err;
          if (err.message === 'auth') authError = true;
        } else {
          apply(json);
        }
        next();
      });
    };
  }
  function optional(paramId, apply) {
    return function (next) {
      if (essentialError) return next();
      getParam(paramId, function (err, json) {
        if (!err) apply(json);
        next();
      });
    };
  }

  var tasks = [
    essential(PARAM.REC_STATE, function (j) {
      status.REC_STATE = toInt(j.value, -1);
      status.REC_NAME = stateName(j);
    }),
    essential(PARAM.STREAM_STATE, function (j) {
      status.STREAM_STATE = toInt(j.value, -1);
      status.STREAM_NAME = stateName(j);
    }),
    optional(PARAM.REC_DURATION, function (j) { status.REC_DUR = formatDuration(j.value); }),
    optional(PARAM.STREAM_DURATION, function (j) { status.STREAM_DUR = formatDuration(j.value); }),
    optional(PARAM.MEDIA_AVAIL, function (j) { status.MEDIA_PCT = toInt(j.value, -1); }),
    optional(PARAM.TEMPERATURE, function (j) { status.TEMP_C = toInt(j.value, -1000); })
  ];
  if (sysNameCache === null) {
    tasks.push(optional(PARAM.SYS_NAME, function (j) {
      sysNameCache = String(j.value || '').substr(0, 30);
    }));
  }

  series(tasks, function () {
    polling = false;
    if (essentialError) {
      consecutiveErrors++;
      var conn = authError ? CONN.AUTH : CONN.OFFLINE;
      var t = T();
      var msg = authError ? t.msg_check_password
              : (essentialError.message === 'timeout' ? t.msg_timeout + settings.host
              : t.msg_unreachable + settings.host);
      console.log('poll failed (' + reason + '): ' + essentialError.message);
      sendStatus(conn, { MESSAGE: msg.substr(0, 40) });
      return;
    }
    consecutiveErrors = 0;
    status.SYS_NAME = sysNameCache || settings.host;
    sendStatus(CONN.OK, status);
  });
}

function schedulePolling() {
  if (pollTimer) { clearInterval(pollTimer); pollTimer = null; }
  if (!settings.host) {
    sendStatus(CONN.NOCONFIG);
    return;
  }
  pollOnce('start');
  pollTimer = setInterval(function () {
    // Back off a little while the HELO is unreachable to keep the phone radio calm.
    if (consecutiveErrors > 5 && (consecutiveErrors % 3) !== 0) {
      consecutiveErrors++;
      return;
    }
    pollOnce('interval');
  }, settings.poll * 1000);
}

/* ---- Commands from the watch ------------------------------------------- */
function runCommand(cmd) {
  if (cmd === CMD.REFRESH) {
    sysNameCache = null;      // also re-read the device name
    pollOnce('refresh');
    return;
  }
  if (!settings.host) {
    sendStatus(CONN.NOCONFIG);
    return;
  }
  var value = cmd;            // watch CMD values 1..4 equal HELO ReplicatorCommand values
  if (value < 1 || value > 4) {
    console.log('unknown command ' + cmd);
    return;
  }
  setParam(PARAM.COMMAND, value, function (err) {
    if (err) {
      var auth = err.message === 'auth';
      sendStatus(auth ? CONN.AUTH : CONN.OFFLINE,
        { MESSAGE: auth ? T().msg_check_password : T().msg_cmd_failed });
      return;
    }
    sendMessage(T().msg_cmd_sent);
    // The HELO needs a moment to change state; poll twice to catch it.
    setTimeout(function () { pollOnce('after-cmd-1'); }, 700);
    setTimeout(function () { pollOnce('after-cmd-2'); }, 2500);
  });
}

/* ---- Pebble events ------------------------------------------------------ */
Pebble.addEventListener('ready', function () {
  loadSettings();
  console.log('HELO Remote ready, host=' + (settings.host || '(none)') + ' poll=' + settings.poll + 's lang=' + currentLang());
  schedulePolling();
});

Pebble.addEventListener('appmessage', function (e) {
  var payload = e && e.payload ? e.payload : {};
  if (payload.CMD !== undefined) {
    runCommand(toInt(payload.CMD, -1));
  }
});

Pebble.addEventListener('showConfiguration', function () {
  clay.config = buildConfig(currentLang());   // page in the current language
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;
  var dict = clay.getSettings(e.response, false);
  applyClaySettings(dict);
  sessionCookie = null;
  sysNameCache = null;
  consecutiveErrors = 0;
  schedulePolling();
});
