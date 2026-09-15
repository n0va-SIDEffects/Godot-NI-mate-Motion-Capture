/**
 * PTZ Remote - Telefon-Seite
 *
 * Die Uhr schickt den gewuenschten Zustand aller Achsen, dieses Skript macht
 * daraus Panasonic-CGI-Aufrufe und meldet zurueck, ob die Kamera antwortet.
 */

var Clay = require('pebble-clay');
var clayConfig = require('./config');
var Panasonic = require('./panasonic');

var CAMERA_SLOTS = 8;
var STORAGE_KEY = 'ptz_settings';
var WATCHDOG_MS = 600;
var SILENCE_LIMIT_MS = 1600;

// Muss zu PtzCommand in src/c/ptz.h passen.
var CMD = {
  MOVE: 1, PRESET_RECALL: 2, PRESET_STORE: 3, FOCUS: 4,
  AUTOFOCUS: 5, POWER: 6, SELECT_CAM: 7, HELLO: 8, STOP_ALL: 9
};
// Muss zu PtzStatus passen.
var STATUS = { UNKNOWN: 0, OK: 1, BUSY: 2, ERROR: 3, NOCONFIG: 4 };

var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

var settings = {};
var cameras = [];
var links = [];          // ein Panasonic-Objekt je Kamera
var activeCam = 0;
var lastMoveAt = 0;
var moving = false;
var lastStatus = -1;
var lastMessage = '';

// ---------------------------------------------------------------------------
// Einstellungen
// ---------------------------------------------------------------------------

function loadSettings() {
  try {
    settings = JSON.parse(localStorage.getItem(STORAGE_KEY) || '{}');
  } catch (err) {
    settings = {};
  }
  buildCameras();
}

function value(key, fallback) {
  var v = settings[key];
  if (v && typeof v === 'object' && 'value' in v) v = v.value;
  return (v === undefined || v === null || v === '') ? fallback : v;
}

function buildCameras() {
  cameras = [];
  links = [];
  for (var i = 1; i <= CAMERA_SLOTS; i++) {
    var host = ('' + value('cam' + i + 'Host', '')).trim();
    if (!host) continue;
    var cam = {
      name: ('' + value('cam' + i + 'Name', 'Kamera ' + i)).trim() || ('Kamera ' + i),
      host: host,
      port: parseInt(value('cam' + i + 'Port', 80), 10) || 80,
      user: ('' + value('cam' + i + 'User', '')).trim(),
      pass: '' + value('cam' + i + 'Pass', '')
    };
    cameras.push(cam);
    links.push(makeLink(cam));
  }
  if (activeCam >= cameras.length) activeCam = 0;
}

function makeLink(cam) {
  var link = new Panasonic(cam);
  link.onResult = function (ok, text) {
    if (ok) {
      report(STATUS.OK, cam.name);
    } else {
      report(STATUS.ERROR, text);
    }
  };
  return link;
}

function presetOffset() {
  return parseInt(value('presetOffset', '0'), 10) || 0;
}

function link() {
  return links[activeCam] || null;
}

// ---------------------------------------------------------------------------
// Rueckmeldung an die Uhr
// ---------------------------------------------------------------------------

function report(status, message) {
  message = message || '';
  if (status === lastStatus && message === lastMessage) {
    return;   // nichts Neues, die Funkstrecke bleibt frei
  }
  lastStatus = status;
  lastMessage = message;

  var dict = {};
  dict.STATUS = status;
  dict.MSG = message.substring(0, 60);
  Pebble.sendAppMessage(dict, null, function (err) {
    console.log('Statusmeldung nicht zugestellt: ' + JSON.stringify(err));
  });
}

function sendCameraList() {
  var names = [];
  for (var i = 0; i < cameras.length; i++) {
    names.push(cameras[i].name.substring(0, 24));
  }
  var dict = {
    CAM_NAMES: names.join('|'),
    CAM_COUNT: cameras.length,
    CAM_ACTIVE: activeCam,
    READY: 1
  };
  Pebble.sendAppMessage(dict, null, function (err) {
    console.log('Kameraliste nicht zugestellt: ' + JSON.stringify(err));
  });

  if (cameras.length === 0) {
    lastStatus = -1;
    report(STATUS.NOCONFIG, 'Keine Kamera eingerichtet');
  }
}

function sendWatchSettings() {
  var dict = {};
  var keys = ['cfgSpeed', 'cfgGyroSens', 'cfgGyroDead', 'cfgPresetCount'];
  for (var i = 0; i < keys.length; i++) {
    var v = value(keys[i], null);
    if (v !== null) dict[keys[i]] = parseInt(v, 10);
  }
  var toggles = ['cfgInvertPan', 'cfgInvertTilt', 'cfgVibrate'];
  for (var j = 0; j < toggles.length; j++) {
    var t = value(toggles[j], null);
    if (t !== null) dict[toggles[j]] = t ? 1 : 0;
  }
  var lock = value('cfgGyroLock', null);
  if (lock !== null) dict.cfgGyroLock = parseInt(lock, 10) || 0;

  if (Object.keys(dict).length > 0) {
    Pebble.sendAppMessage(dict, null, function (err) {
      console.log('Einstellungen nicht zugestellt: ' + JSON.stringify(err));
    });
  }
}

// ---------------------------------------------------------------------------
// Befehle von der Uhr
// ---------------------------------------------------------------------------

function handleMove(payload) {
  var l = link();
  if (!l) {
    report(STATUS.NOCONFIG, 'Keine Kamera eingerichtet');
    return;
  }
  var pan   = payload.PAN   !== undefined ? payload.PAN   : 50;
  var tilt  = payload.TILT  !== undefined ? payload.TILT  : 50;
  var zoom  = payload.ZOOM  !== undefined ? payload.ZOOM  : 50;
  var focus = payload.FOCUS !== undefined ? payload.FOCUS : 50;

  lastMoveAt = Date.now();
  moving = !(pan === 50 && tilt === 50 && zoom === 50 && focus === 50);
  l.setAxes(pan, tilt, zoom, focus);
}

function handleMessage(payload) {
  var cmd = payload.CMD;
  if (cmd === undefined) return;

  if (payload.CAM !== undefined && payload.CAM < cameras.length) {
    activeCam = payload.CAM;
  }
  var l = link();

  switch (cmd) {
    case CMD.MOVE:
      handleMove(payload);
      break;

    case CMD.PRESET_RECALL:
      if (l) {
        // Vor einem Preset immer anhalten: eine laufende Fahrt und eine
        // Positionsfahrt gleichzeitig verwirrt die Kamera.
        l.stopAll();
        l.recallPreset(payload.VALUE || 0, presetOffset());
        report(STATUS.BUSY, 'Preset ' + ((payload.VALUE || 0) + 1));
      }
      break;

    case CMD.PRESET_STORE:
      if (l) {
        l.storePreset(payload.VALUE || 0, presetOffset());
        report(STATUS.BUSY, 'Preset ' + ((payload.VALUE || 0) + 1) + ' gespeichert');
      }
      break;

    case CMD.AUTOFOCUS:
      if (l) l.setAutofocus(payload.VALUE === 1);
      break;

    case CMD.POWER:
      if (l) l.setPower(payload.VALUE === 1);
      break;

    case CMD.SELECT_CAM:
      if (payload.VALUE !== undefined && payload.VALUE < cameras.length) {
        stopAllCameras();
        activeCam = payload.VALUE;
        lastStatus = -1;
        report(STATUS.BUSY, cameras[activeCam].name);
        if (link()) link().ping();
      }
      break;

    case CMD.HELLO:
      sendCameraList();
      sendWatchSettings();
      if (l) l.ping();
      break;

    case CMD.STOP_ALL:
      stopAllCameras();
      break;

    default:
      break;
  }
}

function stopAllCameras() {
  moving = false;
  for (var i = 0; i < links.length; i++) {
    links[i].stopAll();
  }
}

// ---------------------------------------------------------------------------
// Wachhund
//
// Bleibt die Uhr stumm, waehrend eine Achse laeuft - ausser Reichweite,
// leerer Akku, abgestuerzte App - dann haelt dieses Skript die Kamera an.
// Eine Kamera, die von allein weiterfaehrt, ist im Vorstellungsbetrieb der
// schlimmste Fall.
// ---------------------------------------------------------------------------

function watchdog() {
  if (!moving) return;
  if (Date.now() - lastMoveAt < SILENCE_LIMIT_MS) return;

  console.log('Wachhund: keine Meldung von der Uhr, halte an');
  stopAllCameras();
  report(STATUS.ERROR, 'Verbindung unterbrochen');
}

// ---------------------------------------------------------------------------
// Ereignisse
// ---------------------------------------------------------------------------

Pebble.addEventListener('ready', function () {
  loadSettings();
  // Falls eine fruehere Sitzung etwas hinterlassen hat: sauberer Anfang.
  stopAllCameras();
  sendCameraList();
  sendWatchSettings();
  if (link()) link().ping();
  setInterval(watchdog, WATCHDOG_MS);
});

Pebble.addEventListener('appmessage', function (e) {
  handleMessage(e.payload || {});
});

Pebble.addEventListener('showConfiguration', function () {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;
  var incoming = clay.getSettings(e.response, false);
  for (var key in incoming) {
    if (incoming.hasOwnProperty(key)) {
      settings[key] = incoming[key];
    }
  }
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(settings));
  } catch (err) {
    console.log('Einstellungen nicht gespeichert: ' + err.message);
  }
  stopAllCameras();
  buildCameras();
  sendCameraList();
  sendWatchSettings();
  if (link()) link().ping();
});
