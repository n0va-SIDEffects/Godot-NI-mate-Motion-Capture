/*
 * Smoke test for the phone-side JavaScript. Runs under Node:
 *   node tests/pkjs_smoke.js
 * Mocks the PebbleKit JS runtime (Pebble, localStorage, XMLHttpRequest) and
 * a fake Toggl API, then walks through: first start without token, config,
 * refresh, start timer, stop timer.
 */

var assert = require('assert');
var path = require('path');
var Module = require('module');

// --- mocks ------------------------------------------------------------------

var KEYS = { CMD: 10000, RUNNING: 10001, DESCRIPTION: 10002, PROJECT_ID: 10003, PROJECT_NAME: 10004,
  PROJECT_COLOR: 10005, START_TIME: 10006, INDEX: 10007, COUNT: 10008, MESSAGE: 10009 };
var NAME_BY_KEY = {};
Object.keys(KEYS).forEach(function (k) { NAME_BY_KEY[KEYS[k]] = k; });

var origResolve = Module._resolveFilename;
Module._resolveFilename = function (request, parent) {
  if (request === 'message_keys') { return path.join(__dirname, 'mock_message_keys.js'); }
  return origResolve.apply(this, arguments);
};
require('fs').writeFileSync(path.join(__dirname, 'mock_message_keys.js'),
  'module.exports = ' + JSON.stringify(KEYS) + ';');

var listeners = {};
var sent = [];          // decoded messages to the watch
var storage = {};
var requests = [];      // recorded HTTP requests
var running = null;     // fake Toggl state
var nextId = 500;

global.localStorage = {
  getItem: function (k) { return storage.hasOwnProperty(k) ? storage[k] : null; },
  setItem: function (k, v) { storage[k] = String(v); }
};

global.Pebble = {
  addEventListener: function (name, fn) { listeners[name] = fn; },
  sendAppMessage: function (dict, ok) {
    var decoded = {};
    Object.keys(dict).forEach(function (k) { decoded[NAME_BY_KEY[k] || k] = dict[k]; });
    sent.push(decoded);
    setTimeout(ok, 0);
  },
  openURL: function (url) { global.lastUrl = url; }
};

var PROJECTS = [
  { id: 11, name: 'Bühne', color: '#e36a00', active: true, workspace_id: 77 },
  { id: 12, name: 'Admin', color: '#0b83d9', active: true, workspace_id: 77 },
  { id: 13, name: 'Alt', color: '#000000', active: false, workspace_id: 77 }
];
var ENTRIES = [
  { id: 1, description: 'Report schreiben', project_id: 12, start: '2026-09-01T08:00:00Z', duration: 3600, workspace_id: 77 },
  { id: 2, description: 'Video-Setup', project_id: 11, start: '2026-09-02T08:00:00Z', duration: 3600, workspace_id: 77 },
  { id: 3, description: 'report schreiben', project_id: 12, start: '2026-08-30T08:00:00Z', duration: 3600, workspace_id: 77 },
  { id: 4, description: '', project_id: null, start: '2026-08-29T08:00:00Z', duration: 3600, workspace_id: 77 }
];

function fakeToggl(method, url, body) {
  var p = url.replace('https://api.track.toggl.com/api/v9', '');
  if (method === 'GET' && p === '/me') { return [200, { default_workspace_id: 77 }]; }
  if (method === 'GET' && p === '/me/projects') { return [200, PROJECTS]; }
  if (method === 'GET' && p === '/me/time_entries/current') { return [200, running]; }
  if (method === 'GET' && p.indexOf('/me/time_entries?') === 0) { return [200, ENTRIES.concat(running ? [running] : [])]; }
  var m = p.match(/^\/workspaces\/(\d+)\/time_entries$/);
  if (method === 'POST' && m) {
    assert.strictEqual(body.duration, -1);
    assert.strictEqual(body.workspace_id, 77);
    assert.ok(body.created_with);
    running = { id: nextId++, description: body.description, project_id: body.project_id,
      start: body.start, duration: -1, workspace_id: 77 };
    return [200, running];
  }
  m = p.match(/^\/workspaces\/(\d+)\/time_entries\/(\d+)\/stop$/);
  if (method === 'PATCH' && m) {
    assert.ok(running && String(running.id) === m[2], 'stop targets the running entry');
    var stopped = running; running = null;
    return [200, stopped];
  }
  return [404, { error: 'unknown ' + method + ' ' + p }];
}

global.XMLHttpRequest = function () {
  var self = this;
  this.headers = {};
  this.open = function (method, url) { self.method = method; self.url = url; };
  this.setRequestHeader = function (k, v) { self.headers[k] = v; };
  this.send = function (body) {
    assert.strictEqual(self.headers.Authorization, 'Basic ' + Buffer.from('tok123:api_token').toString('base64'));
    requests.push({ method: self.method, url: self.url, body: body ? JSON.parse(body) : null });
    var r = fakeToggl(self.method, self.url, body ? JSON.parse(body) : null);
    setTimeout(function () {
      self.status = r[0];
      self.responseText = JSON.stringify(r[1]);
      self.onload();
    }, 0);
  };
};

// --- helpers ----------------------------------------------------------------

function settle(cb) { setTimeout(cb, 50); }
function ofCmd(cmd) { return sent.filter(function (m) { return m.CMD === cmd; }); }
function last(cmd) { var l = ofCmd(cmd); return l[l.length - 1]; }
function fromWatch(fields) {
  var payload = {};
  Object.keys(fields).forEach(function (k) { payload[KEYS[k]] = fields[k]; });
  listeners.appmessage({ payload: payload });
}

// --- scenario ---------------------------------------------------------------

var app = require('../src/pkjs/index.js');

// utf8Clip must never split a multi-byte character.
assert.strictEqual(app.utf8Clip('Bühne', 2), 'B');
assert.strictEqual(app.utf8Clip('Bühne', 3), 'Bü');
assert.strictEqual(app.utf8Clip('Bühne', 99), 'Bühne');
assert.strictEqual(Buffer.byteLength(app.utf8Clip('ääääääääää', 7)), 6);
assert.strictEqual(app.toPebbleColor('#ffffff'), 0xFF);
assert.strictEqual(app.toPebbleColor('#000000'), 0xC0);
assert.strictEqual(app.toPebbleColor(''), 0);

// 1. No token yet: the watch must get an error and nothing else.
listeners.ready();
settle(function () {
  assert.strictEqual(ofCmd(13).length, 1, 'error sent without token');
  assert.ok(/API-Token/.test(last(13).MESSAGE));
  assert.strictEqual(requests.length, 0);
  sent = [];

  // 2. Settings page saves a token.
  var url = global.lastUrl; assert.ok(!url);
  listeners.showConfiguration();
  assert.ok(/^data:text\/html/.test(global.lastUrl), 'config page is a data URL');
  listeners.webviewclosed({ response: encodeURIComponent(JSON.stringify({ token: ' tok123 ', workspaceId: '' })) });
  settle(function () {
    assert.deepStrictEqual(JSON.parse(storage.toggl_settings), { token: 'tok123', workspaceId: '' });
    assert.strictEqual(last(10).RUNNING, 0, 'status: nothing running');
    var projects = ofCmd(11);
    assert.strictEqual(projects.length, 2, 'only active projects');
    assert.deepStrictEqual(projects.map(function (p) { return p.PROJECT_NAME; }), ['Admin', 'Bühne']);
    assert.strictEqual(projects[1].PROJECT_COLOR, 0xC0 | (3 << 4) | (1 << 2) | 0, 'colour quantised to GColor8');
    var recent = ofCmd(12);
    assert.strictEqual(recent.length, 2, 'recent entries deduplicated, empty ones dropped');
    assert.strictEqual(recent[0].DESCRIPTION, 'Video-Setup');
    assert.strictEqual(recent[0].PROJECT_NAME, 'Bühne');
    assert.strictEqual(recent[1].DESCRIPTION, 'Report schreiben');
    sent = []; requests = [];

    // 3. Start a timer from the watch.
    fromWatch({ CMD: 2, PROJECT_ID: 11, DESCRIPTION: 'Probe' });
    settle(function () {
      var post = requests.filter(function (r) { return r.method === 'POST'; });
      assert.strictEqual(post.length, 1, 'one POST to start');
      assert.strictEqual(post[0].body.project_id, 11);
      assert.strictEqual(post[0].body.description, 'Probe');
      var st = last(10);
      assert.strictEqual(st.RUNNING, 1);
      assert.strictEqual(st.DESCRIPTION, 'Probe');
      assert.strictEqual(st.PROJECT_NAME, 'Bühne');
      assert.ok(st.START_TIME > 1700000000);
      sent = []; requests = [];

      // 4. Starting another one stops the running entry first.
      fromWatch({ CMD: 2, PROJECT_ID: 0, DESCRIPTION: '' });
      settle(function () {
        var methods = requests.map(function (r) { return r.method + ' ' + r.url.split('/v9')[1]; });
        assert.ok(methods.indexOf('PATCH /workspaces/77/time_entries/500/stop') >= 0, 'old entry stopped: ' + methods);
        var post2 = requests.filter(function (r) { return r.method === 'POST'; })[0];
        assert.strictEqual(post2.body.project_id, null, 'no project -> null');
        assert.strictEqual(last(10).PROJECT_NAME, '');
        sent = []; requests = [];

        // 5. Stop from the watch.
        fromWatch({ CMD: 3 });
        settle(function () {
          assert.ok(requests.some(function (r) { return r.method === 'PATCH'; }), 'PATCH stop sent');
          assert.strictEqual(last(10).RUNNING, 0);
          assert.strictEqual(running, null);
          sent = []; requests = [];

          // 6. Refresh with the cache warm: status always, lists only when changed.
          fromWatch({ CMD: 1 });
          settle(function () {
            assert.strictEqual(ofCmd(10).length, 1);
            assert.strictEqual(ofCmd(11).length, 0, 'unchanged project list not resent');
            console.log('pkjs smoke test: OK (' + requests.length + ' API calls in last step)');
            require('fs').unlinkSync(path.join(__dirname, 'mock_message_keys.js'));
          });
        });
      });
    });
  });
});
