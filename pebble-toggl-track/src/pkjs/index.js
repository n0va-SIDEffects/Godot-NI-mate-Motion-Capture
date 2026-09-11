/*
 * Toggl Track for Pebble - phone side.
 *
 * Talks to the Toggl Track API on behalf of the watch and mirrors the result
 * back over AppMessage. Protocol values must match src/c/comm.c.
 */

var keys = require('message_keys');
var toggl = require('./toggl');
var config = require('./config');

var CMD = {
  // watch -> phone
  REFRESH: 1,
  START: 2,
  STOP: 3,
  PREVIOUS: 4,      // stop the running entry and restart the one before it
  // phone -> watch
  STATUS: 10,
  PROJECT: 11,
  RECENT: 12,
  ERROR: 13,
  INFO: 14,
  FAVORITE: 15,
  CONFIG: 16
};

var MAX_PROJECTS = 24;      // must match model.h
var MAX_RECENT = 12;
var MAX_FAVORITES = 4;
var RECENT_DAYS = 30;
var SETTINGS_KEY = 'toggl_settings';
var CACHE_KEY = 'toggl_cache';

var DESC_BYTES = 63;        // DESC_LEN - 1
var NAME_BYTES = 31;        // NAME_LEN - 1
var MESSAGE_BYTES = 63;     // MESSAGE_LEN - 1
var CLIENT_BYTES = 47;      // CLIENT_LEN - 1

// Reminder flag bits (REMIND_FLAGS), shared with model.h
var REMIND_RUNNING = 1;     // "still running?" after N hours / late in the evening
var REMIND_NO_TIMER = 2;    // "nothing running" on weekday mornings

var settings = config.normalise(loadJson(SETTINGS_KEY));
var cache = loadJson(CACHE_KEY) || { projects: [], recent: [], status: null };
var projectsById = {};
var clientsById = {};
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

function sendError(text) {
  console.log('Error: ' + text);
  enqueue(msg(CMD.ERROR, { MESSAGE: utf8Clip(text, MESSAGE_BYTES) }));
}

function sendInfo(text) {
  enqueue(msg(CMD.INFO, { MESSAGE: utf8Clip(text, MESSAGE_BYTES) }));
}

// --- Toggl helpers -----------------------------------------------------------

function client() {
  return toggl.createClient(settings.token);
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

function startOfToday() {
  var d = new Date();
  d.setHours(0, 0, 0, 0);
  return d.getTime();
}

// Seconds an entry contributed today (running entries count up to now).
function secondsToday(e, nowMs) {
  var startMs = Date.parse(e.start);
  if (isNaN(startMs)) { return 0; }
  var endMs = e.duration < 0 ? nowMs : (e.stop ? Date.parse(e.stop) : startMs + e.duration * 1000);
  var from = Math.max(startMs, startOfToday());
  return endMs > from ? Math.round((endMs - from) / 1000) : 0;
}

function todayTotal(entries) {
  var now = Date.now();
  return (entries || []).reduce(function (sum, e) { return sum + secondsToday(e, now); }, 0);
}

// "Kunde · tag1, tag2" for the small line under the project badge.
function clientLine(entry) {
  var parts = [];
  var p = projectInfo(entry && entry.project_id);
  if (p && p.client_id && clientsById[p.client_id]) { parts.push(clientsById[p.client_id]); }
  if (entry && entry.tags && entry.tags.length) { parts.push(entry.tags.join(', ')); }
  return parts.join(' · ');
}

function statusFromEntry(entry, entries) {
  var base = {
    running: 0, description: '', projectName: '', color: 0, start: 0, entryId: 0,
    workspaceId: 0, projectId: 0, clientLine: '', today: todayTotal(entries || cache.entries)
  };
  if (!entry || !entry.id) { return base; }
  var p = projectInfo(entry.project_id);
  base.running = 1;
  base.description = entry.description || '';
  base.projectName = p ? p.name : (entry.project_name || '');
  base.color = p ? toPebbleColor(p.color) : 0;
  base.start = Math.floor(Date.parse(entry.start) / 1000) || Math.floor(Date.now() / 1000);
  base.entryId = entry.id;
  base.workspaceId = entry.workspace_id;
  base.projectId = entry.project_id || 0;
  base.clientLine = clientLine(entry);
  return base;
}

function sendStatus(status) {
  enqueue(msg(CMD.STATUS, {
    RUNNING: status.running,
    DESCRIPTION: utf8Clip(status.description, DESC_BYTES),
    PROJECT_NAME: utf8Clip(status.projectName, NAME_BYTES),
    PROJECT_COLOR: status.color,
    START_TIME: status.start,
    TODAY_SECONDS: status.today || 0,
    CLIENT_NAME: utf8Clip(status.clientLine, CLIENT_BYTES)
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
      PROJECT_COLOR: list[i].color,
      TODAY_SECONDS: list[i].today || 0
    }));
  }
}

function favoriteList() {
  return (settings.favorites || []).filter(function (f) {
    return f && (f.description || f.projectId);
  }).slice(0, MAX_FAVORITES).map(function (f) {
    var p = projectInfo(f.projectId);
    return {
      description: f.description || '',
      projectId: p ? p.id : 0,
      projectName: p ? p.name : '',
      color: p ? toPebbleColor(p.color) : 0
    };
  });
}

function sendFavorites() {
  var list = favoriteList();
  if (list.length === 0) {
    enqueue(msg(CMD.FAVORITE, { INDEX: 0, COUNT: 0 }));
    return;
  }
  list.forEach(function (f, i) {
    enqueue(msg(CMD.FAVORITE, {
      INDEX: i,
      COUNT: list.length,
      DESCRIPTION: utf8Clip(f.description, DESC_BYTES),
      PROJECT_ID: f.projectId,
      PROJECT_NAME: utf8Clip(f.projectName, NAME_BYTES),
      PROJECT_COLOR: f.color
    }));
  });
}

function sendConfig() {
  var r = settings.remind;
  enqueue(msg(CMD.CONFIG, {
    REMIND_FLAGS: (r.running ? REMIND_RUNNING : 0) | (r.noTimer ? REMIND_NO_TIMER : 0),
    REMIND_MAX_HOURS: r.maxHours,
    REMIND_LATE_HOUR: r.lateHour,
    REMIND_START_HOUR: r.startHour
  }));
}

function indexProjects(list) {
  projectsById = {};
  list.forEach(function (p) { projectsById[p.id] = p; });
}

function indexClients(list) {
  clientsById = {};
  (list || []).forEach(function (c) { clientsById[c.id] = c.name; });
}

// Collapse time entries to unique (description, project) pairs, newest first,
// with the time booked on them today.
function buildRecent(entries) {
  var seen = {};
  var out = [];
  var now = Date.now();
  entries.forEach(function (e) {
    var desc = (e.description || '').trim();
    var pid = e.project_id || 0;
    if (!desc && !pid) { return; }
    var key = pid + '|' + desc.toLowerCase();
    if (seen[key]) {
      seen[key].today += secondsToday(e, now);
      return;
    }
    var p = projectInfo(pid);
    var item = {
      description: desc,
      projectId: pid,
      projectName: p ? p.name : '',
      color: p ? toPebbleColor(p.color) : 0,
      today: secondsToday(e, now)
    };
    seen[key] = item;
    out.push(item);
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

function saveCache() {
  saveJson(CACHE_KEY, cache);
}

// --- Actions -----------------------------------------------------------------

// Full refresh: projects, clients and entries first (needed for names and the
// daily total), then the status, then the lists if they changed.
function refreshAll(showProgress) {
  if (!requireToken()) { return; }
  if (busy) { return; }
  busy = true;
  if (showProgress) { sendInfo('Aktualisiere…'); }
  var api = client();

  api.projects(function (err, projects) {
    if (err) { busy = false; return sendError(err); }
    indexProjects(projects);

    api.clients(function (errC, clients) {
      if (errC) { clients = []; }          // clients are decoration only
      indexClients(clients);

      api.recentEntries(RECENT_DAYS, function (err3, entries) {
        if (err3) { busy = false; return sendError(err3); }

        api.current(function (err2, entry) {
          busy = false;
          if (err2) { return sendError(err2); }
          cache.entries = entries;
          var status = statusFromEntry(entry, entries);
          sendStatus(status);

          if (JSON.stringify(projects) !== JSON.stringify(cache.projects)) {
            sendProjects(projects);
            sendFavorites();                 // favourites carry project names
          }
          var recent = buildRecent(entries);
          if (JSON.stringify(recent) !== JSON.stringify(cache.recent)) {
            sendRecent(recent);
          }
          cache.projects = projects;
          cache.clients = clients;
          cache.recent = recent;
          cache.status = status;
          saveCache();
        });
      });
    });
  });
}

// After a start/stop: fresh entries -> status (daily total) and recent list.
function refreshAfterChange(entry) {
  client().recentEntries(RECENT_DAYS, function (err, entries) {
    if (err) { return; }
    cache.entries = entries;
    var status = statusFromEntry(entry, entries);
    sendStatus(status);
    var recent = buildRecent(entries);
    if (JSON.stringify(recent) !== JSON.stringify(cache.recent)) {
      sendRecent(recent);
    }
    cache.recent = recent;
    cache.status = status;
    saveCache();
  });
}

function stopRunning(api, callback) {
  api.current(function (err, entry) {
    if (err) { return callback(err); }
    if (!entry || !entry.id) { return callback(null, null); }
    api.stop(entry.workspace_id, entry.id, function (err2, stopped) {
      if (err2) { return callback(err2); }
      roundStopped(api, stopped || entry, callback);
    });
  });
}

// Optional: round the finished entry to the nearest N minutes (min. one block).
function roundStopped(api, entry, callback) {
  var block = (settings.roundMinutes || 0) * 60;
  if (!block || !entry || !entry.id) { return callback(null, entry); }
  var startMs = Date.parse(entry.start);
  var duration = entry.duration > 0 ? entry.duration : Math.round((Date.now() - startMs) / 1000);
  var rounded = Math.max(block, Math.round(duration / block) * block);
  if (rounded === duration) { return callback(null, entry); }
  var stop = new Date(startMs + rounded * 1000).toISOString();
  api.update(entry.workspace_id, entry.id, { stop: stop, duration: rounded }, function (err, updated) {
    callback(null, err ? entry : (updated || entry));   // rounding is best effort
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
        saveCache();
        refreshAfterChange(entry);
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
    saveCache();
    refreshAfterChange(null);
  });
}

// Swipe: back to the entry before the running one (or the last one, if idle).
function previousTimer() {
  if (!requireToken()) { return; }
  var api = client();
  api.current(function (err, current) {
    if (err) { return sendError(err); }
    api.recentEntries(RECENT_DAYS, function (err2, entries) {
      if (err2) { return sendError(err2); }
      var candidates = buildRecent(entries);
      var cur = current ? ((current.project_id || 0) + '|' + (current.description || '').trim().toLowerCase()) : null;
      var prev = null;
      for (var i = 0; i < candidates.length; i++) {
        var key = candidates[i].projectId + '|' + candidates[i].description.toLowerCase();
        if (key !== cur) { prev = candidates[i]; break; }
      }
      if (!prev) { return sendInfo('Kein vorheriger Eintrag'); }
      startTimer(prev.projectId, prev.description);
    });
  });
}

// --- Pebble events -----------------------------------------------------------

function field(payload, name) {
  if (payload[keys[name]] !== undefined) { return payload[keys[name]]; }
  return payload[name];
}

Pebble.addEventListener('ready', function () {
  console.log('Toggl Track JS ready');
  // Show cached data immediately, then refresh from the network.
  indexProjects(cache.projects || []);
  indexClients(cache.clients || []);
  sendConfig();
  if (cache.status) { sendStatus(cache.status); }
  if (cache.projects && cache.projects.length) { sendProjects(cache.projects); }
  sendFavorites();
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
    case CMD.PREVIOUS:
      previousTimer();
      break;
    default:
      console.log('Unknown command from watch: ' + cmd);
  }
});

Pebble.addEventListener('showConfiguration', function () {
  Pebble.openURL(config.buildConfigUrl(settings, cache.projects || []));
});

Pebble.addEventListener('webviewclosed', function (e) {
  var cfg = config.parseConfigResponse(e && e.response);
  if (!cfg) { return; }
  var tokenChanged = cfg.token !== settings.token || cfg.workspaceId !== settings.workspaceId;
  settings = cfg;
  saveJson(SETTINGS_KEY, settings);
  if (tokenChanged) {
    defaultWorkspaceId = null;
    cache = { projects: [], recent: [], status: null };
    saveCache();
  }
  sendInfo('Einstellungen gespeichert');
  sendConfig();
  sendFavorites();
  refreshAll(true);
});

module.exports = { utf8Clip: utf8Clip, toPebbleColor: toPebbleColor, secondsToday: secondsToday };
