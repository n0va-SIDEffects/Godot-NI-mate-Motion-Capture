/**
 * Prueft die CGI-Schicht ohne Kamera und ohne Uhr.
 *
 *   node test/panasonic.test.js
 *
 * Statt echter HTTP-Anfragen zeichnet ein Platzhalter die URLs auf, sodass
 * sich die erzeugten Panasonic-Befehle Zeichen fuer Zeichen pruefen lassen.
 */

var assert = require('assert');

// --- Platzhalter fuer XMLHttpRequest ---------------------------------------
var sent = [];
var nextStatus = 200;
var nextBody = 'OK';

function FakeXHR() {
  this.status = 0;
  this.responseText = '';
}
FakeXHR.prototype.open = function (method, url, async, user, pass) {
  this._url = url;
  this._user = user;
  this._pass = pass;
};
FakeXHR.prototype.send = function () {
  sent.push({ url: this._url, user: this._user, pass: this._pass });
  this.status = nextStatus;
  this.responseText = nextBody;
  var self = this;
  // Antwort asynchron zustellen, wie es ein echter Aufruf tut.
  setTimeout(function () { if (self.onload) self.onload(); }, 0);
};
global.XMLHttpRequest = FakeXHR;

var Panasonic = require('../src/pkjs/panasonic');

function reset() {
  sent = [];
  nextStatus = 200;
  nextBody = 'OK';
}

function urls() {
  return sent.map(function (s) { return s.url; });
}

// Die Warteschlange haelt 80 ms Abstand ein; der Test wartet entsprechend.
function afterQueue(fn) {
  setTimeout(fn, 400);
}

var tests = [];
function test(name, fn) { tests.push({ name: name, fn: fn }); }

// ---------------------------------------------------------------------------

test('Adresse und Befehlsform', function (done) {
  reset();
  var p = new Panasonic({ host: '192.168.0.10', port: 80 });
  p.setAxes(50, 50, 50, 50);          // unveraendert: nichts senden
  assert.strictEqual(sent.length, 0, 'Stillstand darf nichts ausloesen');

  p.setAxes(70, 30, 50, 50);
  afterQueue(function () {
    assert.strictEqual(urls()[0],
      'http://192.168.0.10/cgi-bin/aw_ptz?cmd=%23PTS7030&res=1');
    done();
  });
});

test('Port nur wenn abweichend', function (done) {
  reset();
  var p = new Panasonic({ host: '10.0.0.5', port: 8080 });
  p.setAxes(60, 50, 50, 50);
  afterQueue(function () {
    assert.ok(urls()[0].indexOf('http://10.0.0.5:8080/cgi-bin/aw_ptz') === 0,
      'abweichender Port gehoert in die Adresse: ' + urls()[0]);
    done();
  });
});

test('Nur geaenderte Achsen gehen hinaus', function (done) {
  reset();
  var p = new Panasonic({ host: 'cam.local' });
  p.setAxes(60, 50, 50, 50);
  afterQueue(function () {
    var before = sent.length;
    p.setAxes(60, 50, 99, 50);        // nur Zoom aendert sich
    afterQueue(function () {
      var neu = urls().slice(before);
      assert.strictEqual(neu.length, 1, 'genau ein zusaetzlicher Aufruf');
      assert.ok(neu[0].indexOf('%23Z99') > -1, 'Zoombefehl erwartet: ' + neu[0]);
      done();
    });
  });
});

test('Stopp haelt alle drei Achsen an', function (done) {
  reset();
  var p = new Panasonic({ host: 'cam.local' });
  p.setAxes(80, 20, 90, 50);
  afterQueue(function () {
    reset();
    p.stopAll();
    setTimeout(function () {
      var all = urls().join(' ');
      assert.ok(all.indexOf('%23PTS5050') > -1, 'Pan/Tilt-Stopp fehlt');
      assert.ok(all.indexOf('%23Z50') > -1, 'Zoom-Stopp fehlt');
      assert.ok(all.indexOf('%23F50') > -1, 'Fokus-Stopp fehlt');
      done();
    }, 600);
  });
});

test('Presets zaehlen ab null, Versatz wirkt', function (done) {
  reset();
  var p = new Panasonic({ host: 'cam.local' });
  p.recallPreset(0, 0);
  afterQueue(function () {
    assert.ok(urls()[0].indexOf('%23R00') > -1, 'Preset 1 ist #R00: ' + urls()[0]);
    reset();
    p.recallPreset(0, 1);
    afterQueue(function () {
      assert.ok(urls()[0].indexOf('%23R01') > -1, 'mit Versatz ist es #R01');
      reset();
      p.storePreset(9, 0);
      afterQueue(function () {
        assert.ok(urls()[0].indexOf('%23M09') > -1, 'Speichern auf Platz 10');
        done();
      });
    });
  });
});

test('Geschwindigkeiten bleiben im gueltigen Bereich', function (done) {
  reset();
  var p = new Panasonic({ host: 'cam.local' });
  p.setAxes(0, 200, 50, 50);          // absichtlich ausserhalb
  afterQueue(function () {
    assert.ok(urls()[0].indexOf('%23PTS0199') > -1,
      'auf 01..99 begrenzen: ' + urls()[0]);
    done();
  });
});

test('Fehlermeldung der Kamera wird erkannt', function (done) {
  reset();
  nextBody = 'er1';
  var p = new Panasonic({ host: 'cam.local' });
  var result = null;
  p.onResult = function (ok, text) { result = { ok: ok, text: text }; };
  p.setAxes(60, 50, 50, 50);
  afterQueue(function () {
    assert.ok(result && result.ok === false, 'er1 muss als Fehler gelten');
    assert.ok(result.text.indexOf('er1') > -1);
    done();
  });
});

test('Abgelehnte Anmeldung wird gemeldet', function (done) {
  reset();
  nextStatus = 401;
  var p = new Panasonic({ host: 'cam.local', user: 'admin', pass: 'geheim' });
  var result = null;
  p.onResult = function (ok, text) { result = { ok: ok, text: text }; };
  p.setAxes(60, 50, 50, 50);
  afterQueue(function () {
    assert.strictEqual(sent[0].user, 'admin', 'Benutzer muss mitgehen');
    assert.ok(result && result.ok === false && /Anmeldung/.test(result.text));
    done();
  });
});

test('Ohne Adresse wird nichts gesendet', function (done) {
  reset();
  var p = new Panasonic({ host: '' });
  var result = null;
  p.onResult = function (ok, text) { result = { ok: ok, text: text }; };
  p.setAxes(60, 50, 50, 50);
  afterQueue(function () {
    assert.strictEqual(sent.length, 0);
    assert.ok(result && result.ok === false);
    done();
  });
});

// ---------------------------------------------------------------------------

var index = 0, failed = 0;

// Eine Zusicherung, die in einem Rueckruf bricht, landet sonst als Absturz
// ohne Bezug zum Test. So bleibt sie ein normaler Fehlschlag.
process.on('uncaughtException', function (err) {
  failed++;
  console.log('  FEHL ' + (tests[index - 1] ? tests[index - 1].name : '?') +
              ': ' + err.message);
  next();
});

function next() {
  if (index >= tests.length) {
    console.log(failed === 0
      ? '\nAlle ' + tests.length + ' Pruefungen bestanden.'
      : '\n' + failed + ' von ' + tests.length + ' fehlgeschlagen.');
    process.exit(failed === 0 ? 0 : 1);
  }
  var t = tests[index++];
  try {
    t.fn(function () {
      console.log('  ok   ' + t.name);
      next();
    });
  } catch (err) {
    failed++;
    console.log('  FEHL ' + t.name + ': ' + err.message);
    next();
  }
}
console.log('Panasonic-CGI-Schicht');
next();
