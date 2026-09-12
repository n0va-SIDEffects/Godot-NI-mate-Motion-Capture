/*
 * Phone side of the Sampler app (PebbleKit JS, ES5 only).
 *
 *  - Renders the settings page with Clay and pushes volume / shake / touch
 *    to the watch.
 *  - Loads up to four user samples (.ima files, 16 kHz IMA ADPCM) from the
 *    configured URLs and transfers them to the watch in AppMessage chunks.
 */
var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var messageKeys = require('message_keys');
var soundNames = require('./sounds.json');
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

var MAX_SLOTS = 4;
var MAX_SAMPLE_BYTES = 24000;   // 3 s at 16 kHz ADPCM; must match phone.h on the watch
var DEFAULT_CHUNK = 500;        // until the watch tells us its inbox size

var chunkSize = DEFAULT_CHUNK;
var watchReady = false;
var queue = [];                 // slots waiting for transfer
var busy = false;
var transferSeq = 0;            // bumps when settings change so stale transfers stop

function log(msg) { console.log('[sampler] ' + msg); }

function loadSettings() {
  var s = {};
  try { s = JSON.parse(localStorage.getItem('clay-settings')) || {}; } catch (e) { s = {}; }
  return s;
}

function settingValue(settings, key, fallback) {
  var v = settings[key];
  if (v === undefined || v === null || v === '') return fallback;
  return v;
}

function sendSettings(settings) {
  var dict = {
    Volume: parseInt(settingValue(settings, 'Volume', 80), 10),
    Shake: settingValue(settings, 'Shake', true) ? 1 : 0,
    Touch: settingValue(settings, 'Touch', true) ? 1 : 0
  };
  // Which sounds to show: one 0/1 per sound, in the watch's table order.
  var enabled = settings.Enabled;
  if (Array.isArray(enabled)) {
    for (var i = 0; i < soundNames.length; i++) {
      dict[messageKeys.Enabled + i] = enabled[i] ? 1 : 0;
    }
  }
  Pebble.sendAppMessage(dict, function() { log('settings sent'); },
    function(e) { log('settings failed: ' + JSON.stringify(e)); });
}

/* ---- sample transfer --------------------------------------------------- */

function sendMessage(dict, retries, onOk, onFail) {
  Pebble.sendAppMessage(dict, onOk, function(e) {
    if (retries > 0) {
      setTimeout(function() { sendMessage(dict, retries - 1, onOk, onFail); }, 300);
    } else {
      onFail(e);
    }
  });
}

function transferSlot(slot, name, bytes, seq, done) {
  var offset = 0;
  var total = bytes.length;
  function next() {
    if (seq !== transferSeq) { done(false); return; }
    if (offset >= total) {
      sendMessage({ XferSlot: slot, XferDone: 1 }, 3, function() { done(true); }, function() { done(false); });
      return;
    }
    var n = Math.min(chunkSize, total - offset);
    var dict = { XferSlot: slot, XferTotal: total, XferOffset: offset, XferData: bytes.slice(offset, offset + n) };
    if (offset === 0) dict.XferName = name;
    sendMessage(dict, 4, function() { offset += n; next(); },
      function(e) { log('chunk failed at ' + offset + ': ' + JSON.stringify(e)); done(false); });
  }
  next();
}

// Converts whatever the XHR delivered (ArrayBuffer, or a binary string on
// runtimes without arraybuffer support) into a plain array of byte values.
function toByteArray(xhr) {
  var resp = xhr.response;
  var n, i;
  if (resp && typeof resp === 'object') {
    var view = null;
    try { view = new Uint8Array(resp); } catch (e) { view = null; }
    if (view && view.length > 0) {
      n = Math.min(view.length, MAX_SAMPLE_BYTES);
      var arr = new Array(n);
      for (i = 0; i < n; i++) arr[i] = view[i];
      return arr;
    }
  }
  var str = (typeof resp === 'string' && resp.length) ? resp : (xhr.responseText || '');
  n = Math.min(str.length, MAX_SAMPLE_BYTES);
  var out = new Array(n);
  for (i = 0; i < n; i++) out[i] = str.charCodeAt(i) & 0xFF;
  return out;
}

function fetchBytes(url, onOk, onFail) {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', url, true);
  try { xhr.responseType = 'arraybuffer'; } catch (e) {}
  if (xhr.overrideMimeType) {
    try { xhr.overrideMimeType('text/plain; charset=x-user-defined'); } catch (e2) {}
  }
  xhr.timeout = 20000;
  xhr.onload = function() {
    if (xhr.status !== 200) { onFail('HTTP ' + xhr.status); return; }
    var bytes = toByteArray(xhr);
    if (bytes.length === 0) { onFail('empty response'); return; }
    onOk(bytes);
  };
  xhr.onerror = function() { onFail('network error'); };
  xhr.ontimeout = function() { onFail('timeout'); };
  xhr.send();
}

function pump() {
  if (busy || queue.length === 0 || !watchReady) return;
  busy = true;
  var job = queue.shift();
  var seq = transferSeq;
  log('loading slot ' + job.slot + ' from ' + job.url);
  fetchBytes(job.url, function(bytes) {
    transferSlot(job.slot, job.name, bytes, seq, function(ok) {
      log('slot ' + job.slot + (ok ? ' done' : ' aborted'));
      busy = false;
      pump();
    });
  }, function(err) {
    log('slot ' + job.slot + ' fetch failed: ' + err);
    sendMessage({ XferSlot: job.slot, XferError: 1 }, 2, function() {}, function() {});
    busy = false;
    pump();
  });
}

function scheduleTransfers(settings) {
  transferSeq++;
  queue = [];
  for (var i = 0; i < MAX_SLOTS; i++) {
    var url = settingValue(settings, 'SampleUrl[' + i + ']', '');
    var name = settingValue(settings, 'SampleName[' + i + ']', 'Sample ' + (i + 1));
    if (url && /^https?:\/\//i.test(url)) {
      queue.push({ slot: i, url: url, name: String(name).substring(0, 20) });
    } else {
      // tell the watch the slot is unused
      sendMessage({ XferSlot: i, XferError: 2 }, 1, function() {}, function() {});
    }
  }
  pump();
}

/* ---- events ------------------------------------------------------------- */

Pebble.addEventListener('ready', function() {
  log('ready');
  Pebble.sendAppMessage({ Ready: 1 }, function() {}, function() {});
});

Pebble.addEventListener('appmessage', function(e) {
  var p = e.payload || {};
  if (p.Hello !== undefined) {
    // the watch reports the largest chunk it can take
    chunkSize = Math.max(100, Math.min(2000, parseInt(p.Hello, 10) || DEFAULT_CHUNK));
    watchReady = true;
    log('watch hello, chunk ' + chunkSize);
    var settings = loadSettings();
    sendSettings(settings);
    scheduleTransfers(settings);
  }
});

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) return;
  clay.getSettings(e.response, false);   // validates and stores in localStorage
  var settings = loadSettings();
  sendSettings(settings);
  scheduleTransfers(settings);
});
