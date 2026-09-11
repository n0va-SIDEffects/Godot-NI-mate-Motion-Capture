/*
 * Smoke test for the phone-side JavaScript. Runs under Node:
 *   node tests/pkjs_smoke.js
 * Uses the PebbleKit JS mock from the pebble-app-dev skill (a copy lives in
 * tests/pkjs-mock.js) and a fake Toggl API, then walks through: first start
 * without token, config, refresh, start, switch, previous, stop with rounding.
 */

var assert = require('assert');
var path = require('path');
var mock = require('./pkjs-mock.js');

var running = null;     // fake Toggl state
var nextId = 500;
var PROJECTS = [
  { id: 11, name: 'Bühne', color: '#e36a00', active: true, workspace_id: 77, client_id: 5 },
  { id: 12, name: 'Admin', color: '#0b83d9', active: true, workspace_id: 77, client_id: null },
  { id: 13, name: 'Alt', color: '#000000', active: false, workspace_id: 77 }
];
var CLIENTS = [{ id: 5, name: 'Theater' }];
var today = new Date(); today.setHours(9, 0, 0, 0);
var ENTRIES = [
  { id: 1, description: 'Report schreiben', project_id: 12, start: '2026-09-01T08:00:00Z', duration: 3600, workspace_id: 77 },
  { id: 2, description: 'Video-Setup', pid: 11, wid: 77, start: '2026-09-02T08:00:00Z', duration: 3600, tags: ['Technik'] },
  { id: 3, description: 'report schreiben', project_id: 12, start: '2026-08-30T08:00:00Z', duration: 3600, workspace_id: 77 },
  { id: 4, description: '', project_id: null, start: '2026-08-29T08:00:00Z', duration: 3600, workspace_id: 77 },
  { id: 5, description: 'Video-Setup', project_id: 11, start: today.toISOString(), stop: new Date(today.getTime() + 1500000).toISOString(), duration: 1500, workspace_id: 77 }
];

function fakeToggl(method, url, body) {
  var p = url.replace('https://api.track.toggl.com/api/v9', '');
  if (method === 'GET' && p === '/me') { return [200, { default_workspace_id: 77 }]; }
  if (method === 'GET' && p === '/me/projects') { return [200, PROJECTS]; }
  if (method === 'GET' && p === '/me/clients') { return [200, CLIENTS]; }
  if (method === 'GET' && p === '/me/time_entries/current') { return [200, running]; }
  if (method === 'GET' && p.indexOf('/me/time_entries?') === 0) { return [200, ENTRIES.concat(running ? [running] : [])]; }
  var m = p.match(/^\/workspaces\/(\d+)\/time_entries$/);
  if (method === 'POST' && m) {
    assert.strictEqual(body.duration, -1);
    assert.strictEqual(body.workspace_id, 77);
    assert.ok(body.created_with);
    // Toggl answers a POST with the v8-style field names
    running = { id: nextId++, description: body.description, pid: body.project_id, wid: 77,
      start: body.start, duration: -1, tags: [] };
    return [200, running];
  }
  m = p.match(/^\/workspaces\/(\d+)\/time_entries\/(\d+)\/stop$/);
  if (method === 'PATCH' && m) {
    assert.ok(running && String(running.id) === m[2], 'stop targets the running entry');
    var stopped = running; running = null;
    stopped.stop = new Date().toISOString();
    stopped.duration = 700;   // 11:40 min -> rounds to 15
    return [200, stopped];
  }
  m = p.match(/^\/workspaces\/(\d+)\/time_entries\/(\d+)$/);
  if (method === 'PUT' && m) {
    assert.strictEqual(body.duration, 900, 'rounded to a quarter hour');
    return [200, { id: parseInt(m[2], 10), duration: body.duration, stop: body.stop }];
  }
  return [404, { error: 'unknown ' + method + ' ' + p }];
}

var rt = mock.install({
  packageJson: path.join(__dirname, '..', 'package.json'),
  fetch: function (method, url, body, headers) {
    assert.strictEqual(headers.Authorization, 'Basic ' + Buffer.from('tok123:api_token').toString('base64'));
    return fakeToggl(method, url, body);
  }
});

function settle(cb) { setTimeout(cb, 80); }
function ofCmd(cmd) { return rt.sent.filter(function (m) { return m.CMD === cmd; }); }
function last(cmd) { var l = ofCmd(cmd); return l[l.length - 1]; }
function reqs(method) { return rt.requests.filter(function (r) { return r.method === method; }); }

var app = require('../src/pkjs/index.js');

// utf8Clip must never split a multi-byte character.
assert.strictEqual(app.utf8Clip('Bühne', 2), 'B');
assert.strictEqual(app.utf8Clip('Bühne', 3), 'Bü');
assert.strictEqual(app.utf8Clip('Bühne', 99), 'Bühne');
assert.strictEqual(Buffer.byteLength(app.utf8Clip('ääääääääää', 7)), 6);
assert.strictEqual(app.toPebbleColor('#ffffff'), 0xFF);
assert.strictEqual(app.toPebbleColor('#000000'), 0xC0);
assert.strictEqual(app.toPebbleColor(''), 0);

// 1. No token yet: the watch gets config + an error, no API call.
rt.fire('ready');
settle(function () {
  assert.strictEqual(ofCmd(13).length, 1, 'error sent without token');
  assert.ok(/API-Token/.test(last(13).MESSAGE));
  assert.strictEqual(ofCmd(16).length, 1, 'config sent on ready');
  assert.strictEqual(last(16).REMIND_FLAGS, 1, 'default: running reminder on');
  assert.strictEqual(rt.requests.length, 0);
  rt.sent = [];

  // 2. Settings page saves token, favourites, reminders, rounding.
  rt.fire('showConfiguration');
  assert.ok(/^data:text\/html/.test(rt.lastUrl), 'config page is a data URL');
  rt.fire('webviewclosed', { response: encodeURIComponent(JSON.stringify({
    token: ' tok123 ', workspaceId: '',
    favorites: [{ description: 'Probe', projectId: 11 }, { description: '', projectId: 12 }, { description: '', projectId: 0 }],
    remind: { running: true, maxHours: '3', lateHour: '21', noTimer: true, startHour: '8' },
    roundMinutes: '15'
  })) });
  settle(function () {
    var saved = JSON.parse(rt.storage.toggl_settings);
    assert.strictEqual(saved.token, 'tok123');
    assert.strictEqual(saved.roundMinutes, 15);
    assert.deepStrictEqual(saved.remind, { running: true, maxHours: 3, lateHour: 21, noTimer: true, startHour: 8 });
    assert.strictEqual(last(16).REMIND_FLAGS, 3);
    assert.strictEqual(last(16).REMIND_MAX_HOURS, 3);

    var st = last(10);
    assert.strictEqual(st.RUNNING, 0, 'status: nothing running');
    assert.strictEqual(st.TODAY_SECONDS, 1500, "today's total from entries");

    var projects = ofCmd(11);
    assert.strictEqual(projects.length, 2, 'only active projects');
    assert.deepStrictEqual(projects.map(function (p) { return p.PROJECT_NAME; }), ['Admin', 'Bühne']);
    assert.strictEqual(projects[1].PROJECT_COLOR, 0xC0 | (3 << 4) | (1 << 2) | 0, 'colour quantised to GColor8');

    var favs = ofCmd(15).filter(function (f) { return f.COUNT > 0; });
    assert.strictEqual(favs[favs.length - 1].COUNT, 2, 'two usable favourites');
    assert.strictEqual(favs[favs.length - 2].DESCRIPTION, 'Probe');
    assert.strictEqual(favs[favs.length - 2].PROJECT_NAME, 'Bühne');
    assert.strictEqual(favs[favs.length - 1].PROJECT_NAME, 'Admin');

    var recent = ofCmd(12);
    assert.strictEqual(recent.length, 2, 'recent entries deduplicated, empty ones dropped');
    assert.strictEqual(recent[0].DESCRIPTION, 'Video-Setup');
    assert.strictEqual(recent[0].PROJECT_NAME, 'Bühne', 'pid/wid normalised to project_id');
    assert.strictEqual(recent[0].TODAY_SECONDS, 1500, 'today per entry');
    assert.strictEqual(recent[1].DESCRIPTION, 'Report schreiben');
    rt.sent = []; rt.requests = [];

    // 3. Start a timer from the watch: POST reply uses pid/wid -> still a project name.
    rt.fromWatch({ CMD: 2, PROJECT_ID: 11, DESCRIPTION: 'Probe' });
    settle(function () {
      var post = reqs('POST');
      assert.strictEqual(post.length, 1, 'one POST to start');
      assert.strictEqual(post[0].body.project_id, 11);
      var st2 = ofCmd(10)[0];
      assert.strictEqual(st2.RUNNING, 1);
      assert.strictEqual(st2.DESCRIPTION, 'Probe');
      assert.strictEqual(st2.PROJECT_NAME, 'Bühne', 'project name despite pid-only reply');
      assert.strictEqual(st2.CLIENT_NAME, 'Theater', 'client line');
      assert.ok(st2.START_TIME > 1700000000);
      rt.sent = []; rt.requests = [];

      // 4. Swipe: previous entry (stops current, starts the most recent different one).
      rt.fromWatch({ CMD: 4 });
      settle(function () {
        var methods = rt.requests.map(function (r) { return r.method + ' ' + r.url.split('/v9')[1]; });
        assert.ok(methods.some(function (x) { return /PATCH .*\/stop$/.test(x); }), 'old entry stopped: ' + methods);
        assert.ok(methods.some(function (x) { return /^PUT /.test(x); }), 'stopped entry rounded');
        var post2 = reqs('POST')[0];
        assert.strictEqual(post2.body.description, 'Video-Setup', 'previous = newest entry that differs');
        assert.strictEqual(post2.body.project_id, 11);
        rt.sent = []; rt.requests = [];

        // 5. Stop from the watch, rounding applied.
        rt.fromWatch({ CMD: 3 });
        settle(function () {
          assert.ok(reqs('PATCH').length === 1, 'PATCH stop sent');
          assert.ok(reqs('PUT').length === 1, 'PUT rounding sent');
          assert.strictEqual(last(10).RUNNING, 0);
          assert.strictEqual(running, null);
          rt.sent = []; rt.requests = [];

          // 6. Refresh with the cache warm: status always, lists only when changed.
          rt.fromWatch({ CMD: 1 });
          settle(function () {
            assert.strictEqual(ofCmd(10).length, 1);
            assert.strictEqual(ofCmd(11).length, 0, 'unchanged project list not resent');
            rt.sent = []; rt.requests = [];

            // 7. Polling: a timer started on the phone/web shows up without any watch action.
            running = { id: 900, description: 'Vom Handy', pid: 12, wid: 77, start: new Date().toISOString(), duration: -1 };
            app.pollStatus();
            settle(function () {
              assert.strictEqual(last(10).RUNNING, 1, 'poll picked up the external start');
              assert.strictEqual(last(10).DESCRIPTION, 'Vom Handy');
              assert.strictEqual(last(10).PROJECT_NAME, 'Admin');
              rt.sent = [];
              app.pollStatus();                 // nothing changed -> nothing sent
              settle(function () {
                assert.strictEqual(ofCmd(10).length, 0, 'unchanged status not resent');
                running = null;                 // stopped on the phone
                app.pollStatus();
                settle(function () {
                  assert.strictEqual(last(10).RUNNING, 0, 'poll picked up the external stop');
                  var diag = JSON.parse(rt.storage.toggl_diag);
                  assert.ok(/project_id=11/.test(diag.lastStartReply), 'diagnostics recorded: ' + diag.lastStartReply);
                  app.stopPolling();
                  rt.cleanup();
                  console.log('pkjs smoke test: OK');
                });
              });
            });
          });
        });
      });
    });
  });
});
