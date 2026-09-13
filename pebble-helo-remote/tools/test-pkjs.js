/*
 * Host-side test harness for src/pkjs/index.js  (node tools/test-pkjs.js):
 * mocks Pebble, XMLHttpRequest, localStorage and pebble-clay, then drives the
 * module against the HELO simulator, which it starts itself on port 18080
 * (SIM_PORT / SIM_PASSWORD override; with SIM_PASSWORD the auth flow is tested too).
 */
var http = require('http');
var path = require('path');
var Module = require('module');
var assert = require('assert');

var PORT = parseInt(process.env.SIM_PORT || '18080', 10);
var PASSWORD = process.env.SIM_PASSWORD || '';

/* ---- mocks ---- */
var store = {};
global.localStorage = {
  getItem: function (k) { return store.hasOwnProperty(k) ? store[k] : null; },
  setItem: function (k, v) { store[k] = String(v); },
  removeItem: function (k) { delete store[k]; }
};

var cookieJar = null;
function XHR() { this.headers = {}; this.status = 0; this.responseText = ''; this._resHeaders = {}; }
XHR.prototype.open = function (m, u) { this.method = m; this.url = u; };
XHR.prototype.setRequestHeader = function (k, v) { this.headers[k] = v; };
XHR.prototype.getResponseHeader = function (k) { return this._resHeaders[k.toLowerCase()] || null; };
XHR.prototype.send = function (body) {
  var self = this;
  var u = new URL(this.url);
  var opts = { method: this.method, hostname: u.hostname, port: u.port, path: u.pathname + u.search, headers: this.headers };
  if (cookieJar && !opts.headers.Cookie) opts.headers.Cookie = cookieJar;   // emulate runtime cookie handling
  var req = http.request(opts, function (res) {
    var data = '';
    res.on('data', function (c) { data += c; });
    res.on('end', function () {
      self.status = res.statusCode;
      self.responseText = data;
      if (res.headers['set-cookie']) {
        self._resHeaders['set-cookie'] = res.headers['set-cookie'][0];
        cookieJar = res.headers['set-cookie'][0].split(';')[0];
      }
      if (self.onload) self.onload();
    });
  });
  req.on('error', function () { if (self.onerror) self.onerror(); });
  if (body) req.write(body);
  req.end();
};
global.XMLHttpRequest = XHR;

var listeners = {};
var sent = [];
global.Pebble = {
  addEventListener: function (ev, fn) { listeners[ev] = fn; },
  sendAppMessage: function (dict, ok) { sent.push(dict); setTimeout(ok, 5); },
  openURL: function (u) { this.lastUrl = u; }
};

// stub pebble-clay
var origResolve = Module._resolveFilename;
Module._resolveFilename = function (request, parent) {
  if (request === 'pebble-clay') return path.join(require('os').tmpdir(), 'helo-remote-clay-stub.js');
  return origResolve.apply(this, arguments);
};
require('fs').writeFileSync(path.join(require('os').tmpdir(), 'helo-remote-clay-stub.js'),
  'function Clay(cfg, custom, opts){ this.cfg=cfg; this.opts=opts; }\n' +
  'Clay.prototype.generateUrl=function(){return "https://clay.example/?x";};\n' +
  'Clay.prototype.getSettings=function(resp, convert){ return JSON.parse(decodeURIComponent(resp)); };\n' +
  'module.exports=Clay;\n');

/* ---- start simulator ---- */
var simArgs = [path.join(__dirname, 'helo-simulator.js'), String(PORT)];
if (PASSWORD) simArgs.push('--password', PASSWORD);
var sim = require('child_process').spawn(process.execPath, simArgs, { stdio: 'ignore' });
process.on('exit', function () { try { sim.kill(); } catch (e) { /* ignore */ } });

/* ---- run ---- */
require(path.join(__dirname, '..', 'src', 'pkjs', 'index.js'));

function wait(ms) { return new Promise(function (r) { setTimeout(r, ms); }); }
function last() { return sent[sent.length - 1]; }
function lastStatus() { for (var i = sent.length - 1; i >= 0; i--) if ('CONN' in sent[i]) return sent[i]; return null; }

(async function () {
  await wait(400);   // let the simulator come up
  // 1. no config -> NOCONFIG
  listeners.ready();
  await wait(50);
  assert.strictEqual(last().CONN, 4, 'expected CONN NOCONFIG');
  console.log('OK  no config -> NOCONFIG');

  // 2. configure via Clay -> polling starts, status OK
  sent.length = 0;
  var cfg = { HELO_HOST: 'http://127.0.0.1/', HELO_PORT: String(PORT), HELO_PASSWORD: PASSWORD, POLL_INTERVAL: { value: 1, precision: 0 }, VIBRATE: true };
  listeners.webviewclosed({ response: encodeURIComponent(JSON.stringify(cfg)) });
  await wait(600);
  var st = lastStatus();
  assert.ok(st, 'no status sent');
  assert.strictEqual(st.CONN, 1, 'expected CONN OK, got ' + JSON.stringify(st));
  assert.strictEqual(st.REC_STATE, 1);
  assert.strictEqual(st.REC_NAME, '', 'known states are translated on the watch');
  assert.strictEqual(st.LANGUAGE, 2, 'no watch info in test -> English (index 2)');
  assert.strictEqual(st.STREAM_STATE, 1);
  assert.strictEqual(st.REC_DUR, '00:00:00');
  assert.strictEqual(st.MEDIA_PCT, 73);
  assert.strictEqual(st.TEMP_C, 47);
  assert.strictEqual(st.SYS_NAME, 'HELO Buehne');
  assert.strictEqual(st.VIBRATE, 1);
  console.log('OK  configured -> status OK: ' + JSON.stringify(st));
  assert.strictEqual(JSON.parse(store.helo_remote_settings).host, '127.0.0.1', 'host sanitised');
  assert.strictEqual(JSON.parse(store.helo_remote_settings).port, PORT);

  // 3. record start command
  sent.length = 0;
  listeners.appmessage({ payload: { CMD: 1 } });
  await wait(3200);
  assert.ok(sent.some(function (d) { return d.MESSAGE === 'Command sent'; }), 'no command ack');
  st = lastStatus();
  assert.strictEqual(st.REC_STATE, 2, 'expected recording');
  assert.strictEqual(st.REC_NAME, '');
  assert.ok(/^00:00:0\d$/.test(st.REC_DUR), 'duration ' + st.REC_DUR);
  console.log('OK  CMD 1 -> recording: ' + JSON.stringify(st));

  // 4. stream start + record stop
  listeners.appmessage({ payload: { CMD: 3 } });
  await wait(300);
  listeners.appmessage({ payload: { CMD: 2 } });
  await wait(3200);
  st = lastStatus();
  assert.strictEqual(st.REC_STATE, 1);
  assert.strictEqual(st.STREAM_STATE, 2);
  assert.strictEqual(st.STREAM_NAME, '');
  console.log('OK  CMD 3 + CMD 2 -> stream live, rec idle');

  // 5. refresh
  sent.length = 0;
  listeners.appmessage({ payload: { CMD: 0 } });
  await wait(400);
  assert.strictEqual(lastStatus().CONN, 1);
  console.log('OK  CMD 0 refresh');

  // 6. unreachable host -> OFFLINE
  sent.length = 0;
  cfg.HELO_HOST = '127.0.0.1'; cfg.HELO_PORT = String(PORT + 1);
  listeners.webviewclosed({ response: encodeURIComponent(JSON.stringify(cfg)) });
  await wait(600);
  st = lastStatus();
  assert.strictEqual(st.CONN, 2, 'expected OFFLINE got ' + JSON.stringify(st));
  assert.ok(/not reachable/.test(st.MESSAGE), st.MESSAGE);
  console.log('OK  unreachable -> OFFLINE: ' + st.MESSAGE);

  // 7. wrong password -> AUTH (only meaningful when simulator started with --password)
  if (PASSWORD) {
    sent.length = 0; cookieJar = null;
    cfg.HELO_PORT = String(PORT); cfg.HELO_PASSWORD = 'wrong';
    listeners.webviewclosed({ response: encodeURIComponent(JSON.stringify(cfg)) });
    await wait(600);
    st = lastStatus();
    assert.strictEqual(st.CONN, 3, 'expected AUTH got ' + JSON.stringify(st));
    console.log('OK  wrong password -> AUTH: ' + st.MESSAGE);
  }

  console.log('\nALL TESTS PASSED');
  process.exit(0);
})().catch(function (e) { console.error('FAIL', e); process.exit(1); });
