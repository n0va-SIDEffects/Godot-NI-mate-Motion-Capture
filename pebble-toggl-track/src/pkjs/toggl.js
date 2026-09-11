/*
 * Minimal Toggl Track API v9 client for PebbleKit JS.
 * Runs on the phone; ES5 only (no arrow functions, no Promises).
 */

var BASE_URL = 'https://api.track.toggl.com/api/v9';
var CREATED_WITH = 'Pebble Toggl Track';
var TIMEOUT_MS = 15000;

var B64 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';

// btoa() is not guaranteed in every PebbleKit JS runtime.
function base64(str) {
  var out = '';
  var i = 0;
  var bytes = [];
  for (i = 0; i < str.length; i++) {
    bytes.push(str.charCodeAt(i) & 0xff);
  }
  for (i = 0; i < bytes.length; i += 3) {
    var b0 = bytes[i];
    var b1 = i + 1 < bytes.length ? bytes[i + 1] : 0;
    var b2 = i + 2 < bytes.length ? bytes[i + 2] : 0;
    var n = (b0 << 16) | (b1 << 8) | b2;
    out += B64.charAt((n >> 18) & 63) + B64.charAt((n >> 12) & 63);
    out += i + 1 < bytes.length ? B64.charAt((n >> 6) & 63) : '=';
    out += i + 2 < bytes.length ? B64.charAt(n & 63) : '=';
  }
  return out;
}

function pad2(n) {
  return (n < 10 ? '0' : '') + n;
}

function isoDate(d) {
  return d.getUTCFullYear() + '-' + pad2(d.getUTCMonth() + 1) + '-' + pad2(d.getUTCDate());
}

function errorForStatus(status) {
  switch (status) {
    case 401:
    case 403: return 'API-Token ungültig';
    case 402: return 'Toggl: Funktion nicht im Plan';
    case 404: return 'Toggl: nicht gefunden';
    case 429: return 'Toggl: zu viele Anfragen';
    default: return 'Toggl-Fehler (HTTP ' + status + ')';
  }
}

// Toggl answers with `project_id`/`workspace_id` in most places but with the
// v8-style `pid`/`wid` in others (notably the reply to a POST). Normalise.
function normaliseEntry(e) {
  if (!e || typeof e !== 'object') { return e; }
  if (e.project_id === undefined || e.project_id === null) {
    e.project_id = (e.pid !== undefined && e.pid !== null) ? e.pid : null;
  }
  if (e.workspace_id === undefined || e.workspace_id === null) {
    e.workspace_id = (e.wid !== undefined && e.wid !== null) ? e.wid : null;
  }
  return e;
}

function Toggl(token) {
  this.token = token;
}

Toggl.prototype.request = function (method, path, body, callback) {
  var xhr = new XMLHttpRequest();
  var done = false;
  var finish = function (err, data) {
    if (done) { return; }
    done = true;
    callback(err, data);
  };

  xhr.open(method, BASE_URL + path, true);
  xhr.setRequestHeader('Authorization', 'Basic ' + base64(this.token + ':api_token'));
  xhr.setRequestHeader('Content-Type', 'application/json');
  try { xhr.timeout = TIMEOUT_MS; } catch (e) { /* not supported everywhere */ }

  xhr.onload = function () {
    if (xhr.status >= 200 && xhr.status < 300) {
      var data = null;
      if (xhr.responseText && xhr.responseText.length) {
        try {
          data = JSON.parse(xhr.responseText);
        } catch (e) {
          return finish('Toggl: ungültige Antwort');
        }
      }
      finish(null, data);
    } else {
      console.log('Toggl ' + method + ' ' + path + ' -> HTTP ' + xhr.status + ' ' + xhr.responseText);
      finish(errorForStatus(xhr.status));
    }
  };
  xhr.onerror = function () { finish('Keine Internetverbindung'); };
  xhr.ontimeout = function () { finish('Toggl antwortet nicht'); };

  xhr.send(body ? JSON.stringify(body) : null);
};

// GET /me -> { default_workspace_id, fullname, ... }
Toggl.prototype.me = function (callback) {
  this.request('GET', '/me', null, callback);
};

// GET /me/projects -> active projects across all workspaces
Toggl.prototype.projects = function (callback) {
  this.request('GET', '/me/projects', null, function (err, list) {
    if (err) { return callback(err); }
    var out = [];
    (list || []).forEach(function (p) {
      if (p.active === false || p.server_deleted_at) { return; }
      out.push({
        id: p.id,
        name: p.name || '',
        color: p.color || '',
        workspace_id: p.workspace_id,
        client_id: p.client_id || null
      });
    });
    out.sort(function (a, b) {
      return a.name.toLowerCase() < b.name.toLowerCase() ? -1 : 1;
    });
    callback(null, out);
  });
};

// GET /me/clients -> [{id, name}]
Toggl.prototype.clients = function (callback) {
  this.request('GET', '/me/clients', null, function (err, list) {
    if (err) { return callback(err); }
    callback(null, (list || []).filter(function (c) { return !c.server_deleted_at && !c.archived; })
      .map(function (c) { return { id: c.id, name: c.name || '' }; }));
  });
};

// GET /me/time_entries/current -> entry or null
Toggl.prototype.current = function (callback) {
  this.request('GET', '/me/time_entries/current', null, function (err, entry) {
    callback(err, normaliseEntry(entry));
  });
};

// GET /me/time_entries for the last `days` days, newest first
Toggl.prototype.recentEntries = function (days, callback) {
  var end = new Date(Date.now() + 2 * 86400000);      // end_date is exclusive
  var start = new Date(Date.now() - days * 86400000);
  var path = '/me/time_entries?start_date=' + isoDate(start) + '&end_date=' + isoDate(end);
  this.request('GET', path, null, function (err, list) {
    if (err) { return callback(err); }
    list = (list || []).filter(function (e) { return !e.server_deleted_at; }).map(normaliseEntry);
    list.sort(function (a, b) { return a.start < b.start ? 1 : -1; });
    callback(null, list);
  });
};

// POST /workspaces/{wid}/time_entries -> running entry
Toggl.prototype.start = function (workspaceId, projectId, description, callback) {
  var body = {
    created_with: CREATED_WITH,
    description: description || '',
    start: new Date().toISOString(),
    duration: -1,
    workspace_id: workspaceId,
    project_id: projectId || null
  };
  this.request('POST', '/workspaces/' + workspaceId + '/time_entries', body, function (err, entry) {
    if (!err && entry) {
      entry = normaliseEntry(entry);
      // Belt and braces: what we asked for, in case the reply omits it.
      if (!entry.project_id && projectId) { entry.project_id = projectId; }
      if (!entry.workspace_id) { entry.workspace_id = workspaceId; }
    }
    callback(err, entry);
  });
};

// PATCH /workspaces/{wid}/time_entries/{id}/stop -> stopped entry
Toggl.prototype.stop = function (workspaceId, entryId, callback) {
  this.request('PATCH', '/workspaces/' + workspaceId + '/time_entries/' + entryId + '/stop', null,
    function (err, entry) { callback(err, normaliseEntry(entry)); });
};

// PUT /workspaces/{wid}/time_entries/{id} -> updated entry (used for rounding)
Toggl.prototype.update = function (workspaceId, entryId, fields, callback) {
  var body = { created_with: CREATED_WITH, workspace_id: workspaceId };
  Object.keys(fields).forEach(function (k) { body[k] = fields[k]; });
  this.request('PUT', '/workspaces/' + workspaceId + '/time_entries/' + entryId, body,
    function (err, entry) { callback(err, normaliseEntry(entry)); });
};

// --- Demo backend -----------------------------------------------------------
// Token "demo" runs the app against an in-memory Toggl with sample data, so the
// watch UI can be tried (and screenshotted in the emulator) without an account.

function DemoToggl() {
  var now = Date.now();
  this.projects_ = [
    { id: 101, name: 'Bühne', color: '#e36a00', active: true, workspace_id: 1, client_id: 7 },
    { id: 102, name: 'Admin', color: '#0b83d9', active: true, workspace_id: 1, client_id: null },
    { id: 103, name: 'Video', color: '#9e5bd9', active: true, workspace_id: 1, client_id: 7 },
    { id: 104, name: 'Meetings', color: '#c9806b', active: true, workspace_id: 1, client_id: null }
  ];
  this.clients_ = [{ id: 7, name: 'Theater St.Gallen' }];
  this.entries_ = [
    { id: 1, description: 'Probe Hamlet', project_id: 101, workspace_id: 1, start: new Date(now - 3 * 3600000).toISOString(), stop: new Date(now - 2 * 3600000).toISOString(), duration: 3600, tags: ['Probe'] },
    { id: 2, description: 'Dienstplan', project_id: 102, workspace_id: 1, start: new Date(now - 26 * 3600000).toISOString(), stop: new Date(now - 25 * 3600000).toISOString(), duration: 3600, tags: [] },
    { id: 3, description: 'Video-Setup Saal', project_id: 103, workspace_id: 1, start: new Date(now - 50 * 3600000).toISOString(), stop: new Date(now - 47 * 3600000).toISOString(), duration: 10800, tags: ['Technik'] },
    { id: 4, description: 'Teamsitzung', project_id: 104, workspace_id: 1, start: new Date(now - 72 * 3600000).toISOString(), stop: new Date(now - 71 * 3600000).toISOString(), duration: 3600, tags: [] }
  ];
  this.running_ = null;
  this.nextId_ = 100;
}
DemoToggl.prototype.later_ = function (cb, err, data) { setTimeout(function () { cb(err, data); }, 60); };
DemoToggl.prototype.me = function (cb) { this.later_(cb, null, { default_workspace_id: 1, fullname: 'Demo' }); };
DemoToggl.prototype.projects = function (cb) {
  this.later_(cb, null, this.projects_.map(function (p) { return { id: p.id, name: p.name, color: p.color, workspace_id: p.workspace_id, client_id: p.client_id }; }));
};
DemoToggl.prototype.clients = function (cb) { this.later_(cb, null, this.clients_.slice()); };
DemoToggl.prototype.current = function (cb) { this.later_(cb, null, this.running_); };
DemoToggl.prototype.recentEntries = function (days, cb) {
  var all = this.entries_.concat(this.running_ ? [this.running_] : []);
  all.sort(function (a, b) { return a.start < b.start ? 1 : -1; });
  this.later_(cb, null, all);
};
DemoToggl.prototype.start = function (workspaceId, projectId, description, cb) {
  this.running_ = { id: this.nextId_++, description: description || '', project_id: projectId || null,
    workspace_id: workspaceId, start: new Date().toISOString(), stop: null, duration: -1, tags: [] };
  this.later_(cb, null, this.running_);
};
DemoToggl.prototype.stop = function (workspaceId, entryId, cb) {
  var e = this.running_;
  if (!e || e.id !== entryId) { return this.later_(cb, 'Toggl: nicht gefunden'); }
  e.stop = new Date().toISOString();
  e.duration = Math.max(1, Math.round((Date.parse(e.stop) - Date.parse(e.start)) / 1000));
  this.entries_.push(e); this.running_ = null;
  this.later_(cb, null, e);
};
DemoToggl.prototype.update = function (workspaceId, entryId, fields, cb) {
  var e = this.entries_.filter(function (x) { return x.id === entryId; })[0];
  if (!e) { return this.later_(cb, 'Toggl: nicht gefunden'); }
  Object.keys(fields).forEach(function (k) { e[k] = fields[k]; });
  this.later_(cb, null, e);
};

var demoInstance = null;

// Factory: returns a real client or the shared demo backend.
function createClient(token) {
  if (token === 'demo') {
    if (!demoInstance) { demoInstance = new DemoToggl(); }
    return demoInstance;
  }
  return new Toggl(token);
}

module.exports = Toggl;
module.exports.base64 = base64;
module.exports.createClient = createClient;
module.exports.normaliseEntry = normaliseEntry;
