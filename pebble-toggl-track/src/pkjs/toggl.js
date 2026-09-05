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
        workspace_id: p.workspace_id
      });
    });
    out.sort(function (a, b) {
      return a.name.toLowerCase() < b.name.toLowerCase() ? -1 : 1;
    });
    callback(null, out);
  });
};

// GET /me/time_entries/current -> entry or null
Toggl.prototype.current = function (callback) {
  this.request('GET', '/me/time_entries/current', null, callback);
};

// GET /me/time_entries for the last `days` days, newest first
Toggl.prototype.recentEntries = function (days, callback) {
  var end = new Date(Date.now() + 2 * 86400000);      // end_date is exclusive
  var start = new Date(Date.now() - days * 86400000);
  var path = '/me/time_entries?start_date=' + isoDate(start) + '&end_date=' + isoDate(end);
  this.request('GET', path, null, function (err, list) {
    if (err) { return callback(err); }
    list = (list || []).filter(function (e) { return !e.server_deleted_at; });
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
  this.request('POST', '/workspaces/' + workspaceId + '/time_entries', body, callback);
};

// PATCH /workspaces/{wid}/time_entries/{id}/stop -> stopped entry
Toggl.prototype.stop = function (workspaceId, entryId, callback) {
  this.request('PATCH', '/workspaces/' + workspaceId + '/time_entries/' + entryId + '/stop', null, callback);
};

module.exports = Toggl;
module.exports.base64 = base64;
