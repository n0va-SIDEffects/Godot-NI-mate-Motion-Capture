/*
 * Toggl Track for Pebble - phone side.
 *
 * Talks to the Toggl Track API on behalf of the watch and mirrors the result
 * back over AppMessage. Protocol values must match src/c/comm.c.
 */

var keys = require('message_keys');
var Toggl = require('./toggl');
var config = require('./config');

var CMD = {
  // watch -> phone
  REFRESH: 1,
  START: 2,
  STOP: 3,
  // phone -> watch
  STATUS: 10,
  PROJECT: 11,
  RECENT: 12,
  ERROR: 13,
  INFO: 14
};

var MAX_PROJECTS = 24;      // must match model.h
var MAX_RECENT = 12;
var RECENT_DAYS = 30;
var SETTINGS_KEY = 'toggl_settings';
var CACHE_KEY = 'toggl_cache';

var settings = loadJson(SETTINGS_KEY) || { token: '', workspaceId: '' };
var cache = loadJson(CACHE_KEY) || { projects: [], recent: [], status: null };
var projectsById = {};
var defaultWorkspaceId = null;
var busy = false;

// --- storage -----------------------------------------------------------------

function loadJson(key) {
  try {
    var raw = localStorage.getItem(key);
    return raw ? JSON.parse(raw) : null;
  } catch (e) {
    return null;
  }
}

function saveJson(key, value) {
  try {
    localStorage.setItem(key, JSON.stringify(value));
  } catch (e) {
    console.log('localStorage write failed: ' + e);
  }
}

// --- AppMessage queue --------------------------------------------------------

var queue = [];
var sending = false;

function enqueue(dict) {
  queue.push({ dict: dict, tries: 0 });
  pump();
}

function pump() {
  if (sending || queue.length === 0) { return; }
  var item = queue[0];
  sending = true;
  Pebble.sendAppMessage(item.dict, function () {
    sending = false;
    queue.shift();
    pump();
  }, function (e) {
    sending = false;
    item.tries += 1;
    if (item.tries >= 2) {
      console.log('AppMessage dropped after retries: ' + JSON.stringify(item.dict));
      queue.shift();
    }
    setTimeout(pump, 200);
  });
}

function msg(cmd, fields) {
  var d = {};
  d[keys.CMD] = cmd;
  Object.keys(fields || {}).forEach(function (name) {
    var v = fields[name];
    if (v === null || v === undefined) { return; }
    d[keys[name]] = v;
  });
  return d;
}

// Buffers on the watch are byte-sized (see model.h); cut on UTF-8 byte
// boundaries so umlauts never end up half-transmitted.
function utf8Clip(str, maxBytes) {
  str = String(str || '');
  var bytes = 0;
  for (var i = 0; i < str.length; i++) {
    var c = str.charCodeAt(i);
    var n = c < 0x80 ? 1 : c < 0x800 ? 2 : (c >= 0xD800 && c <= 0xDBFF) ? 4 : 3;
    if (n === 4) { i++; }                 // surrogate pair = one 4-byte char
    if (bytes + n > maxBytes) { return str.slice(0, n === 4 ? i - 1 : i); }
    bytes += n;
  }
  return str;
}

var DESC_BYTES = 63;      // DESC_LEN - 1
var NAME_BYTES = 31;      // NAME_LEN - 1
var MESSAGE_BYTES = 63;   // MESSAGE_LEN - 1

function sendError(text) {
  console.log('Error: ' + text);
  enqueue(msg(CMD.ERROR, { MESSAGE: utf8Clip(text, MESSAGE_BYTES) }));
}

function sendInfo(text) {
  enqueue(msg(CMD.INFO, { MESSAGE: utf8Clip(text, MESSAGE_BYTES) }));
}

// --- Toggl helpers -----------------------------------------------------------

function client() {
  return new Toggl(settings.token);
}

// Toggl colours are "#rrggbb"; the watch wants a GColor8 (2 bits per channel).
function toPebbleColor(hex) {
  if (!hex || !/^#?[0-9a-fA-F]{6}$/.test(hex)) { return 0; }
  hex = hex.replace('#', '');
  var q = function (c) { return Math.min(3, Math.round(parseInt(c, 16) / 85)); };
  return 0xC0 | (q(hex.substr(0, 2)) << 4) | (q(hex.substr(2, 2)) << 2) | q(hex.substr(4, 2));
}

function projectInfo(projectId) {
  return (projectId && projectsById[projectId]) || null;
}

function statusFromEntry(entry) {
  if (!entry || !entry.id) {
    return { running: 0, description: '', projectName: '', color: 0, start: 0, entryId: 0, workspaceId: 0 };
  }
  var p = projectInfo(entry.project_id);
  return {
    running: 1,
    description: entry.description || '',
    projectName: p ? p.name : (entry.project_name || ''),
    color: p ? toPebbleColor(p.color) : 0,
    start: Math.floor(Date.parse(entry.start) / 1000) || Math.floor(Date.now() / 1000),
    entryId: entry.id,
    workspaceId: entry.workspace_id
  };
}

function sendStatus(status) {
  enqueue(msg(CMD.STATUS, {
    RUNNING: status.running,
    DESCRIPTION: utf8Clip(status.description, DESC_BYTES),
    PROJECT_NAME: utf8Clip(status.projectName, NAME_BYTES),
    PROJECT_COLOR: status.color,
    START_TIME: status.start
  }));
}

function sendProjects(list) {
  var n = Math.min(list.length, MAX_PROJECTS);
  if (n === 0) {
    enqueue(msg(CMD.PROJECT, { INDEX: 0, COUNT: 0 }));
    return;
  }
  for (var i = 0; i < n; i++) {
    enqueue(msg(CMD.PROJECT, {
      INDEX: i,
      COUNT: n,
      PROJECT_ID: list[i].id,
      PROJECT_NAME: utf8Clip(list[i].name, NAME_BYTES),
      PROJECT_COLOR: toPebbleColor(list[i].color)
    }));
  }
}

function sendRecent(list) {
  var n = Math.min(list.length, MAX_RECENT);
  if (n === 0) {
    enqueue(msg(CMD.RECENT, { INDEX: 0, COUNT: 0 }));
    return;
  }
  for (var i = 0; i < n; i++) {
    enqueue(msg(CMD.RECENT, {
      INDEX: i,
      COUNT: n,
      PROJECT_ID: list[i].projectId || 0,
      DESCRIPTION: utf8Clip(list[i].description, DESC_BYTES),
      PROJECT_NAME: utf8Clip(list[i].projectName, NAME_BYTES),
      PROJECT_COLOR: list[i].color
    }));
  }
}

function indexProjects(list) {
  projectsById = {};
  list.forEach(function (p) { projectsById[p.id] = p; });
}

// Collapse time entries to unique (description, project) pairs, newest first.
function buildRecent(entries) {
  var seen = {};
  var out = [];
  entries.forEach(function (e) {
    var desc = (e.description || '').trim();
    var pid = e.project_id || 0;
    if (!desc && !pid) { return; }
    var key = pid + '|' + desc.toLowerCase();
    if (seen[key]) { return; }
    seen[key] = true;
    var p = projectInfo(pid);
    out.push({
      description: desc,
      projectId: pid,
      projectName: p ? p.name : '',
      color: p ? toPebbleColor(p.color) : 0
    });
  });
  return out.slice(0, MAX_RECENT);
}

function workspaceFor(projectId, callback) {
  var p = projectInfo(projectId);
  if (p && p.workspace_id) { return callback(null, p.workspace_id); }
  if (settings.workspaceId) { return callback(null, parseInt(settings.workspaceId, 10)); }
  if (defaultWorkspaceId) { return callback(null, defaultWorkspaceId); }
  client().me(function (err, me) {
    if (err) { return callback(err); }
    defaultWorkspaceId = me && me.default_workspace_id;
    if (!defaultWorkspaceId) { return callback('Kein Workspace gefunden'); }
    callback(null, defaultWorkspaceId);
  });
}

function requireToken() {
  if (settings.token) { return true; }
  sendError('Kein API-Token. Bitte in der Pebble-App eintragen.');
  return false;
}

// --- Actions -----------------------------------------------------------------

// Full refresh: status first (fast), then projects and recent entries.
function refreshAll(showProgress) {
  if (!requireToken()) { return; }
  if (busy) { return; }
  busy = true;
  if (showProgress) { sendInfo('Aktualisiere…'); }
  var api = client();

  api.projects(function (err, projects) {
    if (err) { busy = false; return sendError(err); }
    indexProjects(projects);

    api.current(function (err2, entry) {
      if (err2) { busy = false; return sendError(err2); }
      var status = statusFromEntry(entry);
      sendStatus(status);

      var projectsJson = JSON.stringify(projects);
      if (projectsJson !== JSON.stringify(cache.projects)) {
        sendProjects(projects);
      }

      api.recentEntries(RECENT_DAYS, function (err3, entries) {
        busy = false;
        if (err3) { return sendError(err3); }
        var recent = buildRecent(entries);
        if (JSON.stringify(recent) !== JSON.stringify(cache.recent)) {
          sendRecent(recent);
        }
        cache = { projects: projects, recent: recent, status: status };
        saveJson(CACHE_KEY, cache);
      });
    });
  });
}

function refreshRecentOnly() {
  client().recentEntries(RECENT_DAYS, function (err, entries) {
    if (err) { return; }
    var recent = buildRecent(entries);
    if (JSON.stringify(recent) !== JSON.stringify(cache.recent)) {
      sendRecent(recent);
      cache.recent = recent;
      saveJson(CACHE_KEY, cache);
    }
  });
}

function stopRunning(api, callback) {
  api.current(function (err, entry) {
    if (err) { return callback(err); }
    if (!entry || !entry.id) { return callback(null, null); }
    api.stop(entry.workspace_id, entry.id, callback);
  });
}

function startTimer(projectId, description) {
  if (!requireToken()) { return; }
  if (busy) { return sendInfo('Bitte warten…'); }
  busy = true;
  var api = client();
  workspaceFor(projectId, function (err, workspaceId) {
    if (err) { busy = false; return sendError(err); }
    // Toggl allows only one running entry; stop the current one first.
    stopRunning(api, function (err2) {
      if (err2) { busy = false; return sendError(err2); }
      api.start(workspaceId, projectId, description, function (err3, entry) {
        busy = false;
        if (err3) { return sendError(err3); }
        var status = statusFromEntry(entry);
        sendStatus(status);
        cache.status = status;
        saveJson(CACHE_KEY, cache);
        refreshRecentOnly();
      });
    });
  });
}

function stopTimer() {
  if (!requireToken()) { return; }
  if (busy) { return sendInfo('Bitte warten…'); }
  busy = true;
  stopRunning(client(), function (err) {
    busy = false;
    if (err) { return sendError(err); }
    var status = statusFromEntry(null);
    sendStatus(status);
    cache.status = status;
    saveJson(CACHE_KEY, cache);
    refreshRecentOnly();
  });
}

module.exports = { utf8Clip: utf8Clip, toPebbleColor: toPebbleColor };

// --- Pebble events -----------------------------------------------------------

function field(payload, name) {
  if (payload[keys[name]] !== undefined) { return payload[keys[name]]; }
  return payload[name];
}

Pebble.addEventListener('ready', function () {
  console.log('Toggl Track JS ready');
  // Show cached data immediately, then refresh from the network.
  indexProjects(cache.projects || []);
  if (cache.status) { sendStatus(cache.status); }
  if (cache.projects && cache.projects.length) { sendProjects(cache.projects); }
  if (cache.recent && cache.recent.length) { sendRecent(cache.recent); }
  refreshAll(!cache.status);
});

Pebble.addEventListener('appmessage', function (e) {
  var payload = e.payload || {};
  var cmd = Number(field(payload, 'CMD'));
  switch (cmd) {
    case CMD.REFRESH:
      refreshAll(true);
      break;
    case CMD.START:
      startTimer(Number(field(payload, 'PROJECT_ID')) || 0, field(payload, 'DESCRIPTION') || '');
      break;
    case CMD.STOP:
      stopTimer();
      break;
    default:
      console.log('Unknown command from watch: ' + cmd);
  }
});

Pebble.addEventListener('showConfiguration', function () {
  Pebble.openURL(config.buildConfigUrl(settings));
});

Pebble.addEventListener('webviewclosed', function (e) {
  var cfg = config.parseConfigResponse(e && e.response);
  if (!cfg) { return; }
  settings = cfg;
  saveJson(SETTINGS_KEY, settings);
  defaultWorkspaceId = null;
  cache = { projects: [], recent: [], status: null };
  saveJson(CACHE_KEY, cache);
  sendInfo('Einstellungen gespeichert');
  refreshAll(true);
});
