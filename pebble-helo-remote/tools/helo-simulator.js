#!/usr/bin/env node
/*
 * Minimal AJA HELO REST API simulator for testing HELO Remote without hardware.
 *
 *   node tools/helo-simulator.js [port] [--password secret]
 *
 * Then point the app (Clay settings) at http://<this machine>:<port>.
 * Implements just enough of /config?action=get|set and /authenticator/login
 * for the watch app: record/stream state machine, durations, media, temperature.
 */
var http = require('http');
var url = require('url');

var args = process.argv.slice(2);
var port = 8080;
var password = null;
for (var i = 0; i < args.length; i++) {
  if (args[i] === '--password') password = args[++i];
  else if (/^\d+$/.test(args[i])) port = parseInt(args[i], 10);
}

var state = {
  recState: 1,         // 1 idle, 2 recording
  streamState: 1,
  recStart: null,
  streamStart: null,
  mediaPct: 73,
  tempC: 47,
  sysName: 'HELO Buehne'
};
var sessions = {};
var pins = {};

function tc(startMs) {
  if (!startMs) return '00:00:00:00';
  var s = Math.floor((Date.now() - startMs) / 1000);
  var frames = Math.floor(((Date.now() - startMs) % 1000) / 40);
  function p(n) { return (n < 10 ? '0' : '') + n; }
  return p(Math.floor(s / 3600)) + ':' + p(Math.floor((s % 3600) / 60)) + ':' + p(s % 60) + ':' + p(frames);
}

var STATE_NAMES = { 0: 'eRRSUninitialized', 1: 'eRRSIdle', 2: 'eRRSRecording', 3: 'eRRSFailedIdle', 4: 'eRRSFailed', 5: 'eRRSShutdown' };
var STREAM_NAMES = { 0: 'eRSSUninitialized', 1: 'eRSSIdle', 2: 'eRSSStreaming', 3: 'eRSSFailedIdle', 4: 'eRSSFailed', 5: 'eRSSShutdown' };

function param(id, value, valueName) {
  var out = { paramid: id, name: id.replace('eParamID_', ''), value: String(value) };
  if (valueName) out.value_name = valueName;
  return out;
}

function getParam(id) {
  switch (id) {
    case 'eParamID_ReplicatorRecordState': return param(id, state.recState, STATE_NAMES[state.recState]);
    case 'eParamID_ReplicatorStreamState': return param(id, state.streamState, STREAM_NAMES[state.streamState]);
    case 'eParamID_RecordingDuration': return param(id, tc(state.recStart));
    case 'eParamID_StreamingDuration': return param(id, tc(state.streamStart));
    case 'eParamID_CurrentMediaAvailable': return param(id, state.mediaPct);
    case 'eParamID_Temperature': return param(id, state.tempC);
    case 'eParamID_SysName': return param(id, state.sysName);
    case 'eParamID_ReplicatorCommand': return param(id, 0);
    default: return null;
  }
}

function applyCommand(value) {
  switch (String(value)) {
    case '1': if (state.recState !== 2) { state.recState = 2; state.recStart = Date.now(); } break;
    case '2': state.recState = 1; state.recStart = null; break;
    case '3': if (state.streamState !== 2) { state.streamState = 2; state.streamStart = Date.now(); } break;
    case '4': state.streamState = 1; state.streamStart = null; break;
    default: return false;
  }
  return true;
}

function isAuthed(req) {
  if (!password) return true;
  var cookie = req.headers.cookie || '';
  var m = cookie.match(/helo_session=([a-z0-9]+)/);
  return !!(m && sessions[m[1]]);
}

function send(res, code, obj, headers) {
  var body = typeof obj === 'string' ? obj : JSON.stringify(obj);
  var h = { 'Content-Type': typeof obj === 'string' ? 'text/html' : 'application/json', 'Content-Length': Buffer.byteLength(body) };
  for (var k in (headers || {})) h[k] = headers[k];
  res.writeHead(code, h);
  res.end(body);
}

http.createServer(function (req, res) {
  var u = url.parse(req.url, true);
  console.log(new Date().toISOString(), req.method, req.url);

  if (u.pathname === '/authenticator/login' && req.method === 'POST') {
    var data = '';
    req.on('data', function (c) { data += c; });
    req.on('end', function () {
      var m = data.match(/password_provided=([^&]*)/);
      var given = m ? decodeURIComponent(m[1].replace(/\+/g, ' ')) : '';
      if (password === null || given === password) {
        var token = Math.random().toString(36).substr(2);
        sessions[token] = true;
        send(res, 200, { login: 'success' }, { 'Set-Cookie': 'helo_session=' + token + '; Path=/' });
      } else {
        send(res, 200, { login: 'failed' });
      }
    });
    return;
  }

  // Timeline web API stand-in: PUT /v1/user/pins/<id> stores the pin, GET /__pins lists them (for tests).
  var pinMatch = u.pathname.match(/^\/v1\/user\/pins\/([^/]+)$/);
  if (pinMatch && req.method === 'PUT') {
    if (!req.headers['x-user-token']) return send(res, 401, { error: 'missing X-User-Token' });
    var pinData = '';
    req.on('data', function (c) { pinData += c; });
    req.on('end', function () {
      try { pins[decodeURIComponent(pinMatch[1])] = JSON.parse(pinData); } catch (e) { return send(res, 400, { error: 'bad json' }); }
      console.log('   -> pin ' + pinMatch[1] + ' ' + JSON.stringify(pins[decodeURIComponent(pinMatch[1])].layout));
      send(res, 200, { status: 'ok' });
    });
    return;
  }
  if (u.pathname === '/__pins') return send(res, 200, pins);

  if (u.pathname === '/config') {
    if (!isAuthed(req)) {
      return send(res, 200, '<html><body>Please log in via /authenticator</body></html>');
    }
    var q = u.query;
    if (q.action === 'get') {
      var p = getParam(q.paramid);
      if (!p) return send(res, 404, { error: 'unknown paramid ' + q.paramid });
      return send(res, 200, p);
    }
    if (q.action === 'set') {
      if (q.paramid === 'eParamID_ReplicatorCommand') {
        if (!applyCommand(q.value)) return send(res, 400, { error: 'bad value' });
        console.log('   -> rec=' + state.recState + ' stream=' + state.streamState);
        return send(res, 200, param(q.paramid, q.value));
      }
      return send(res, 200, param(q.paramid, q.value));
    }
  }
  send(res, 404, { error: 'not found' });
}).listen(port, function () {
  console.log('HELO simulator listening on http://0.0.0.0:' + port + (password ? ' (password required)' : ''));
});
