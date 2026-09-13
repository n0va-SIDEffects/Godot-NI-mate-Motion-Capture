/*
 * Timeline pins for HELO Remote.
 * One pin per recording / stream: created when the state changes to active,
 * updated with the duration when it goes back to idle. Pins are pushed through
 * the public timeline web API with the user's timeline token; the watch cannot
 * create pins locally. Open pins survive a restart of the JS side (localStorage).
 */
var STORE_KEY = 'helo_remote_pins';
var ACTIVE = 2;                       /* eParamID_Replicator*State: 2 = recording / streaming */
var ICON_REC = 'system://images/NOTIFICATION_FLAG';
var ICON_STREAM = 'system://images/GENERIC_CONFIRMATION';

var settings = null;                  /* shared with index.js */
var texts = null;                     /* function returning the phone string table */
var token = null;
var open = { rec: null, stream: null };
var last = { rec: -1, stream: -1 };

function log(m) { console.log('timeline: ' + m); }

function load() {
  try {
    var raw = localStorage.getItem(STORE_KEY);
    if (raw) open = JSON.parse(raw);
  } catch (e) { open = { rec: null, stream: null }; }
  if (!open || typeof open !== 'object') open = { rec: null, stream: null };
}

function save() {
  try { localStorage.setItem(STORE_KEY, JSON.stringify(open)); } catch (e) { /* ignore */ }
}

function pad2(n) { return (n < 10 ? '0' : '') + n; }
function hhmm(ms) { var d = new Date(ms); return pad2(d.getHours()) + ':' + pad2(d.getMinutes()); }

function withToken(cb) {
  if (token) return cb(token);
  if (typeof Pebble === 'undefined' || !Pebble.getTimelineToken) return cb(null);
  try {
    Pebble.getTimelineToken(function (t) { token = t; cb(t); },
                            function (err) { log('no timeline token: ' + err); cb(null); });
  } catch (e) { cb(null); }
}

function putPin(pin) {
  withToken(function (t) {
    if (!t) return;
    var host = String(settings.timelineHost || '').replace(/\/+$/, '');
    if (!host) return;
    var xhr = new XMLHttpRequest();
    xhr.open('PUT', host + '/v1/user/pins/' + encodeURIComponent(pin.id), true);
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.setRequestHeader('X-User-Token', t);
    xhr.onload = function () {
      if (xhr.status >= 200 && xhr.status < 300) log('pin ' + pin.id + ' ok');
      else log('pin ' + pin.id + ' rejected: ' + xhr.status + ' ' + String(xhr.responseText).substr(0, 80));
    };
    xhr.onerror = function () { log('pin ' + pin.id + ' network error'); };
    xhr.send(JSON.stringify(pin));
  });
}

function buildPin(kind, startMs, stopMs, sysName) {
  var t = texts();
  var body = t.pin_started + ' ' + hhmm(startMs);
  var pin = {
    id: 'helo-' + kind + '-' + startMs,
    time: new Date(startMs).toISOString(),
    layout: {
      type: 'genericPin',
      title: kind === 'rec' ? t.pin_rec : t.pin_stream,
      subtitle: sysName || 'AJA HELO',
      tinyIcon: kind === 'rec' ? ICON_REC : ICON_STREAM
    }
  };
  if (stopMs) {
    var minutes = Math.max(1, Math.round((stopMs - startMs) / 60000));
    pin.duration = minutes;
    body = hhmm(startMs) + ' - ' + hhmm(stopMs) + ' (' + minutes + ' ' + t.pin_min + ')';
  }
  pin.layout.body = body;
  return pin;
}

function transition(kind, state, sysName) {
  var prev = last[kind];
  last[kind] = state;
  if (prev === -1) {                          /* first status after start: no transition known */
    if (state !== ACTIVE && open[kind]) {     /* was active before the JS restarted, close it */
      putPin(buildPin(kind, open[kind], Date.now(), sysName));
      open[kind] = null; save();
    }
    return;
  }
  if (state === ACTIVE && prev !== ACTIVE && !open[kind]) {
    open[kind] = Date.now(); save();
    putPin(buildPin(kind, open[kind], null, sysName));
  } else if (state !== ACTIVE && prev === ACTIVE && open[kind]) {
    putPin(buildPin(kind, open[kind], Date.now(), sysName));
    open[kind] = null; save();
  }
}

module.exports = {
  init: function (settingsRef, textsFn) { settings = settingsRef; texts = textsFn; load(); },
  /* called with every successful status poll */
  onStatus: function (recState, streamState, sysName) {
    if (!settings || !settings.timeline) { last.rec = recState; last.stream = streamState; return; }
    transition('rec', recState, sysName);
    transition('stream', streamState, sysName);
  },
  resetToken: function () { token = null; }
};
