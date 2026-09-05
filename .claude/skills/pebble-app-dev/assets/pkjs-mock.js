/*
 * Minimal mock of the PebbleKit JS runtime for Node smoke tests.
 *
 *   var mock = require('.../assets/pkjs-mock.js');
 *   var rt = mock.install({
 *     packageJson: 'package.json',                    // for message_keys
 *     fetch: function (method, url, body, headers) {  // fake server
 *       return [200, { hello: 'world' }];             // [status, jsonBody]
 *     }
 *   });
 *   require('./src/pkjs/index.js');
 *   rt.fire('ready');
 *   rt.fromWatch({ CMD: 1 });                          // as if the watch sent it
 *   // later: rt.sent -> decoded dicts the app sent to the watch ({CMD: 10, ...})
 *   //        rt.requests -> [{method, url, body, headers}]
 *   //        rt.storage  -> localStorage contents
 *   //        rt.lastUrl  -> argument of the last Pebble.openURL
 */
var fs = require('fs');
var path = require('path');
var Module = require('module');

function loadKeys(packageJson) {
  var pkg = JSON.parse(fs.readFileSync(packageJson, 'utf8'));
  var raw = (pkg.pebble || {}).messageKeys || [];
  var keys = {};
  if (Array.isArray(raw)) {
    var next = 10000;
    raw.forEach(function (entry) {
      var m = /^([A-Za-z_][A-Za-z0-9_]*)(?:\[(\d+)\])?$/.exec(entry);
      keys[m[1]] = next;
      next += parseInt(m[2] || '1', 10);
    });
  } else {
    Object.keys(raw).forEach(function (k) { keys[k] = raw[k]; });
  }
  return keys;
}

function install(opts) {
  opts = opts || {};
  var keys = opts.packageJson ? loadKeys(opts.packageJson) : (opts.keys || {});
  var nameByKey = {};
  Object.keys(keys).forEach(function (k) { nameByKey[keys[k]] = k; });

  var mockKeysPath = path.join(require('os').tmpdir(), 'pkjs-mock-message-keys-' + process.pid + '.js');
  fs.writeFileSync(mockKeysPath, 'module.exports = ' + JSON.stringify(keys) + ';');
  var origResolve = Module._resolveFilename;
  Module._resolveFilename = function (request) {
    if (request === 'message_keys') { return mockKeysPath; }
    return origResolve.apply(this, arguments);
  };

  var rt = {
    keys: keys,
    listeners: {},
    sent: [],
    requests: [],
    storage: {},
    lastUrl: null,
    fire: function (name, event) {
      if (rt.listeners[name]) { rt.listeners[name](event || {}); }
    },
    fromWatch: function (fields) {
      var payload = {};
      Object.keys(fields).forEach(function (k) { payload[keys[k] !== undefined ? keys[k] : k] = fields[k]; });
      rt.fire('appmessage', { payload: payload });
    },
    cleanup: function () {
      try { fs.unlinkSync(mockKeysPath); } catch (e) {}
      Module._resolveFilename = origResolve;
    }
  };

  global.localStorage = {
    getItem: function (k) { return rt.storage.hasOwnProperty(k) ? rt.storage[k] : null; },
    setItem: function (k, v) { rt.storage[k] = String(v); },
    removeItem: function (k) { delete rt.storage[k]; }
  };

  global.Pebble = {
    addEventListener: function (name, fn) { rt.listeners[name] = fn; },
    sendAppMessage: function (dict, ok, fail) {
      var decoded = {};
      Object.keys(dict).forEach(function (k) { decoded[nameByKey[k] || k] = dict[k]; });
      rt.sent.push(decoded);
      setTimeout(function () { if (ok) { ok({ data: { transactionId: rt.sent.length } }); } }, 0);
    },
    openURL: function (url) { rt.lastUrl = url; },
    getActiveWatchInfo: function () { return opts.watchInfo || { platform: 'emery', model: 'pebble_time_2', language: 'de_DE', firmware: { major: 4, minor: 9 } }; },
    getTimelineToken: function (ok) { setTimeout(function () { ok('mock-timeline-token'); }, 0); },
    showSimpleNotificationOnPebble: function () {}
  };

  global.XMLHttpRequest = function () {
    var self = this;
    self.headers = {};
    self.open = function (method, url) { self.method = method; self.url = url; };
    self.setRequestHeader = function (k, v) { self.headers[k] = v; };
    self.send = function (body) {
      var parsed = body ? JSON.parse(body) : null;
      rt.requests.push({ method: self.method, url: self.url, body: parsed, headers: self.headers });
      var r = opts.fetch ? opts.fetch(self.method, self.url, parsed, self.headers) : [404, null];
      setTimeout(function () {
        if (r === 'error') { return self.onerror && self.onerror(); }
        if (r === 'timeout') { return self.ontimeout && self.ontimeout(); }
        self.status = r[0];
        self.responseText = r[1] === undefined ? '' : (typeof r[1] === 'string' ? r[1] : JSON.stringify(r[1]));
        if (self.onload) { self.onload(); }
      }, 0);
    };
  };

  return rt;
}

module.exports = { install: install, loadKeys: loadKeys };
