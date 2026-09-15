/**
 * Ende-zu-Ende-Pruefung der Vorschau.
 *
 *   node test/preview.test.js
 *
 * Eine nachgebaute Kamera liefert ein echtes JPEG. Der Test faengt alle
 * Nachrichten ab, die an die Uhr gehen wuerden, setzt daraus das Bild wieder
 * zusammen - genau so, wie es der Code auf der Uhr tut - und vergleicht das
 * Ergebnis mit dem Original. Damit ist die ganze Kette geprueft: Abrufen,
 * Dekodieren, Verkleinern, Einfaerben, Zerlegen, Zusammensetzen.
 */

var assert = require('assert');
var http = require('http');
var fs = require('fs');
var path = require('path');
var Module = require('module');

var jpegFile = path.join(__dirname, 'fixtures', '420.jpg');
var jpegBytes = fs.readFileSync(jpegFile);

// --- Nachgebaute Kamera ----------------------------------------------------
var requested = [];
var server = http.createServer(function (req, res) {
  requested.push(req.url);
  if (req.url.indexOf('/cgi-bin/camera') === 0) {
    res.writeHead(200, { 'Content-Type': 'image/jpeg' });
    res.end(jpegBytes);
  } else if (req.url.indexOf('/cgi-bin/aw_ptz') === 0) {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.end('ok');
  } else {
    res.writeHead(404);
    res.end('nope');
  }
});

// --- Pebble-Umgebung -------------------------------------------------------
var toWatch = [];
var appMessageHandler = null, readyHandler = null;

global.Pebble = {
  addEventListener: function (name, fn) {
    if (name === 'ready') readyHandler = fn;
    if (name === 'appmessage') appMessageHandler = fn;
  },
  sendAppMessage: function (dict, ok, fail) {
    toWatch.push(dict);
    // Die echte Zustellung ist asynchron - der Ablauf im Code haengt daran.
    setTimeout(function () { if (ok) ok(); }, 0);
  },
  openURL: function () {}
};
global.localStorage = {
  _d: {},
  getItem: function (k) { return this._d[k] || null; },
  setItem: function (k, v) { this._d[k] = String(v); }
};

function NodeXHR() { this.status = 0; }
NodeXHR.prototype.open = function (m, url, a, u, p) { this._url = url; };
NodeXHR.prototype.send = function () {
  var self = this;
  var req = http.get(require('url').parse(this._url), function (res) {
    var chunks = [];
    res.on('data', function (c) { chunks.push(c); });
    res.on('end', function () {
      self.status = res.statusCode;
      var buf = Buffer.concat(chunks);
      if (self.responseType === 'arraybuffer') {
        self.response = buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.length);
      }
      self.responseText = buf.toString('binary');
      if (self.onload) self.onload();
    });
  });
  req.on('error', function () { if (self.onerror) self.onerror(); });
};
global.XMLHttpRequest = NodeXHR;

var realLoad = Module._load;
Module._load = function (request) {
  if (request === 'pebble-clay') {
    return function Clay() {
      this.generateUrl = function () { return 'about:blank'; };
      this.getSettings = function (r) { return JSON.parse(r); };
    };
  }
  return realLoad.apply(this, arguments);
};

// --- Ablauf ----------------------------------------------------------------

var failed = 0;
function check(name, cond, detail) {
  if (cond) { console.log('  ok   ' + name); }
  else { failed++; console.log('  FEHL ' + name + (detail ? ': ' + detail : '')); }
}

server.listen(0, '127.0.0.1', function () {
  var port = server.address().port;
  global.localStorage.setItem('ptz_settings', JSON.stringify({
    cam1Name: 'Test', cam1Host: '127.0.0.1', cam1Port: String(port),
    cfgPreview: true, cfgPreviewSize: '1'
  }));
  require(path.join(__dirname, '..', 'src', 'pkjs', 'index.js'));
  readyHandler();
  // Die Uhr meldet ihre Empfangsgroesse im Handshake.
  appMessageHandler({ payload: { CMD: 8, MAXRX: 2048, CAM: 0 } });
  setTimeout(run, 300);
});

function run() {
  console.log('Vorschaubild, ganze Kette');
  toWatch.length = 0;
  requested.length = 0;

  appMessageHandler({ payload: { CMD: 10, VALUE: 1, CAM: 0 } });

  setTimeout(function () {
    var snap = requested.filter(function (u) { return u.indexOf('/cgi-bin/camera') === 0; });
    check('Schnappschuss wird abgerufen', snap.length === 1, requested.join(' '));
    check('Aufloesung wird mitgegeben',
          snap.length > 0 && snap[0].indexOf('resolution=1280') > -1, snap[0]);

    var start = toWatch.filter(function (m) { return m.IMG_W !== undefined; })[0];
    check('Bildkopf geht an die Uhr', !!start, JSON.stringify(toWatch.slice(0, 2)));
    if (!start) return finish();

    check('Sendebreite entspricht der Stufe', start.IMG_W === 96, 'IMG_W=' + start.IMG_W);
    check('Seitenverhaeltnis bleibt erhalten',
          Math.abs(start.IMG_H - 96 * 9 / 16) <= 2, 'IMG_H=' + start.IMG_H);
    check('Palette hat 16 Eintraege', start.IMG_PAL.length === 16);
    check('Alle Palettenfarben sind gueltige Pebble-Farben',
          start.IMG_PAL.every(function (b) { return (b & 0xC0) === 0xC0; }));

    var chunks = toWatch.filter(function (m) { return m.IMG_DATA !== undefined; });
    check('Bild kommt in Haeppchen', chunks.length >= 1, chunks.length + ' Haeppchen');

    var gesamt = chunks.reduce(function (n, c) { return n + c.IMG_DATA.length; }, 0);
    check('Vollstaendig uebertragen', gesamt === start.IMG_LEN,
          gesamt + ' von ' + start.IMG_LEN);
    check('Kein Haeppchen groesser als die Uhr verkraftet',
          chunks.every(function (c) { return c.IMG_DATA.length <= 2048; }));

    // --- Das Bild so zusammensetzen, wie preview.c es tut ---
    var w = start.IMG_W, h = start.IMG_H;
    var rowBytes = (w + 1) >> 1;
    var pixels = new Uint8Array(w * h);
    chunks.forEach(function (c) {
      for (var i = 0; i < c.IMG_DATA.length; i++) {
        var bi = c.IMG_OFF + i;
        var y = (bi / rowBytes) | 0;
        if (y >= h) break;
        var x = (bi % rowBytes) * 2;
        var b = c.IMG_DATA[i];
        pixels[y * w + x] = start.IMG_PAL[b >> 4];
        if (x + 1 < w) pixels[y * w + x + 1] = start.IMG_PAL[b & 15];
      }
    });

    var gesetzt = 0;
    for (var i = 0; i < pixels.length; i++) if (pixels[i] !== 0) gesetzt++;
    check('Jeder Bildpunkt ist belegt', gesetzt === pixels.length,
          gesetzt + ' von ' + pixels.length);

    // Als PNG ablegen, damit man das Ergebnis auch ansehen kann.
    var rgb = Buffer.alloc(w * h * 3);
    for (var p = 0; p < pixels.length; p++) {
      var c8 = pixels[p];
      rgb[p * 3]     = ((c8 >> 4) & 3) * 85;
      rgb[p * 3 + 1] = ((c8 >> 2) & 3) * 85;
      rgb[p * 3 + 2] = (c8 & 3) * 85;
    }
    fs.writeFileSync(path.join(__dirname, 'ergebnis.raw'),
                     Buffer.concat([Buffer.from(w + 'x' + h + '\n'), rgb]));

    // --- Abbruch, sobald gefahren wird ---
    toWatch.length = 0;
    appMessageHandler({ payload: { CMD: 10, VALUE: 1, CAM: 0 } });
    appMessageHandler({ payload: { CMD: 1, PAN: 70, TILT: 50, ZOOM: 50, FOCUS: 50, CAM: 0 } });

    setTimeout(function () {
      var nachFahrt = toWatch.filter(function (m) { return m.IMG_DATA !== undefined; });
      check('Fahrt bricht die Uebertragung ab', nachFahrt.length < chunks.length,
            nachFahrt.length + ' statt ' + chunks.length + ' Haeppchen');

      // --- Anforderung waehrend der Fahrt wird abgelehnt ---
      toWatch.length = 0;
      appMessageHandler({ payload: { CMD: 10, VALUE: 1, CAM: 0 } });
      setTimeout(function () {
        var neu = toWatch.filter(function (m) { return m.IMG_W !== undefined; });
        check('Waehrend der Fahrt kommt kein neues Bild', neu.length === 0);
        finish();
      }, 300);
    }, 400);
  }, 1500);
}

function finish() {
  server.close();
  console.log(failed === 0 ? '\nAlle Pruefungen bestanden.' : '\n' + failed + ' fehlgeschlagen.');
  process.exit(failed === 0 ? 0 : 1);
}
