/**
 * Ende-zu-Ende-Pruefung der Telefon-Seite gegen eine nachgebaute Kamera.
 *
 *   node test/bridge.test.js
 *
 * Der Test startet einen echten HTTP-Server, der wie eine Panasonic-Kamera
 * antwortet, laedt src/pkjs/index.js in einer nachgebauten Pebble-Umgebung
 * und schickt dieselben Nachrichten hinein, die die Uhr senden wuerde.
 */

var assert = require('assert');
var http = require('http');
var path = require('path');
var Module = require('module');

// --- Nachgebaute Kamera ----------------------------------------------------

var received = [];
var server = http.createServer(function (req, res) {
  received.push(req.url);
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  // Die echten Kameras spiegeln den Befehl in ihrer Antwort.
  var match = /cmd=%23([^&]*)/.exec(req.url);
  res.end(match ? match[1].toLowerCase() : 'ok');
});

// --- Nachgebaute Pebble-Umgebung ------------------------------------------

var toWatch = [];
var appMessageHandler = null;
var readyHandler = null;

global.Pebble = {
  addEventListener: function (name, fn) {
    if (name === 'ready') readyHandler = fn;
    if (name === 'appmessage') appMessageHandler = fn;
  },
  sendAppMessage: function (dict) { toWatch.push(dict); },
  openURL: function () {}
};

global.localStorage = {
  _data: {},
  getItem: function (k) { return this._data[k] || null; },
  setItem: function (k, v) { this._data[k] = String(v); }
};

// XMLHttpRequest auf Basis des echten HTTP-Moduls.
function NodeXHR() { this.status = 0; this.responseText = ''; }
NodeXHR.prototype.open = function (method, url, async, user, pass) {
  this._url = url; this._user = user; this._pass = pass;
};
NodeXHR.prototype.send = function () {
  var self = this;
  var opts = require('url').parse(this._url);
  if (this._user) {
    opts.auth = this._user + ':' + (this._pass || '');
  }
  var req = http.get(opts, function (res) {
    var body = '';
    res.on('data', function (c) { body += c; });
    res.on('end', function () {
      self.status = res.statusCode;
      self.responseText = body;
      if (self.onload) self.onload();
    });
  });
  req.on('error', function () { if (self.onerror) self.onerror(); });
};
global.XMLHttpRequest = NodeXHR;

// pebble-clay durch einen Platzhalter ersetzen: die Konfigurationsseite
// gehoert nicht zu dem, was hier geprueft wird.
var realLoad = Module._load;
Module._load = function (request, parent, isMain) {
  if (request === 'pebble-clay') {
    return function Clay() {
      this.generateUrl = function () { return 'about:blank'; };
      this.getSettings = function (response) { return JSON.parse(response); };
    };
  }
  return realLoad.apply(this, arguments);
};

// --- Ablauf ----------------------------------------------------------------

var PORT = 0;
server.listen(0, '127.0.0.1', function () {
  PORT = server.address().port;

  // Eine Kamera einrichten, bevor index.js die Einstellungen liest.
  global.localStorage.setItem('ptz_settings', JSON.stringify({
    cam1Name: 'Test', cam1Host: '127.0.0.1', cam1Port: String(PORT),
    cfgSpeed: 3
  }));

  require(path.join(__dirname, '..', 'src', 'pkjs', 'index.js'));
  readyHandler();
  setTimeout(run, 200);
});

function send(payload) {
  appMessageHandler({ payload: payload });
}

function urlsWith(fragment) {
  return received.filter(function (u) { return u.indexOf(fragment) > -1; });
}

var failed = 0;
function check(name, condition, detail) {
  if (condition) {
    console.log('  ok   ' + name);
  } else {
    failed++;
    console.log('  FEHL ' + name + (detail ? ': ' + detail : ''));
  }
}

function run() {
  console.log('Telefon-Seite gegen nachgebaute Kamera');

  var listMsg = toWatch.filter(function (m) { return m.CAM_NAMES !== undefined; })[0];
  check('Kameraliste geht an die Uhr',
        listMsg && listMsg.CAM_NAMES === 'Test' && listMsg.CAM_COUNT === 1,
        JSON.stringify(listMsg));

  received.length = 0;
  // Eine Fahrt nach rechts, wie sie die Neigungssteuerung ausloest.
  send({ CMD: 1, PAN: 70, TILT: 50, ZOOM: 50, FOCUS: 50, CAM: 0 });

  setTimeout(function () {
    check('Bewegung erreicht die Kamera',
          urlsWith('PTS7050').length === 1, received.join(' '));

    received.length = 0;
    send({ CMD: 2, VALUE: 2, CAM: 0 });     // Preset 3 abrufen

    setTimeout(function () {
      check('Preset haelt zuerst an', urlsWith('PTS5050').length >= 1, received.join(' '));
      check('Preset 3 ruft Speicher 2 ab', urlsWith('R02').length === 1, received.join(' '));

      // Der Wachhund: die Uhr verstummt mitten in der Fahrt.
      received.length = 0;
      send({ CMD: 1, PAN: 80, TILT: 50, ZOOM: 50, FOCUS: 50, CAM: 0 });

      setTimeout(function () {
        check('Wachhund haelt die Kamera an',
              urlsWith('PTS5050').length >= 1,
              'gesendet: ' + received.join(' '));

        var errMsg = toWatch.filter(function (m) { return m.STATUS === 3; }).pop();
        check('Uhr erfaehrt vom Abbruch', !!errMsg, JSON.stringify(toWatch.slice(-3)));

        server.close();
        console.log(failed === 0 ? '\nAlle Pruefungen bestanden.'
                                 : '\n' + failed + ' fehlgeschlagen.');
        process.exit(failed === 0 ? 0 : 1);
      }, 2600);   // laenger als SILENCE_LIMIT_MS
    }, 500);
  }, 500);
}
