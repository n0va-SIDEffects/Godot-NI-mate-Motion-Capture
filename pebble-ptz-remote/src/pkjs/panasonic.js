/**
 * Panasonic PTZ - CGI-Schicht
 *
 * Die AW-Kameras (AW-HE, AW-UE, AW-UR ...) nehmen Befehle ueber eine
 * schlichte HTTP-Schnittstelle entgegen:
 *
 *   http://<host>/cgi-bin/aw_ptz?cmd=%23PTS5050&res=1
 *
 * Das "%23" ist das Rautezeichen, mit dem jeder Befehl beginnt. Die Kamera
 * antwortet mit einer kurzen Bestaetigung ("pTS5050") oder mit "erN" bei
 * einem Fehler.
 *
 * Geschwindigkeiten laufen von 01 bis 99, wobei 50 Stillstand bedeutet.
 */

var REQUEST_TIMEOUT_MS = 2000;
var MIN_GAP_MS = 80;           // Mindestabstand zwischen zwei Anfragen

/** Zweistellige Zahl im Bereich 01..99. */
function pad2(value) {
  var v = Math.round(value);
  if (v < 1) v = 1;
  if (v > 99) v = 99;
  return (v < 10 ? '0' : '') + v;
}

function pad2Raw(value) {
  var v = Math.round(value);
  if (v < 0) v = 0;
  if (v > 99) v = 99;
  return (v < 10 ? '0' : '') + v;
}

function Panasonic(camera) {
  this.camera = camera || {};
  this.queue = [];
  this.busy = false;
  this.lastSend = 0;
  this.timer = null;
  // Zuletzt gesendeter Zustand, damit unveraenderte Achsen die Kamera nicht
  // mit identischen Befehlen zumuellen.
  this.state = { pan: 50, tilt: 50, zoom: 50, focus: 50 };
  this.onResult = null;   // function(ok, text)
}

Panasonic.prototype.baseUrl = function () {
  var cam = this.camera;
  var host = (cam.host || '').trim();
  if (!host) return null;
  var port = parseInt(cam.port, 10) || 80;
  var scheme = /^https:\/\//i.test(host) ? 'https://' : 'http://';
  host = host.replace(/^https?:\/\//i, '').replace(/\/+$/, '');
  return scheme + host + (port === 80 ? '' : ':' + port);
};

/**
 * Befehl einreihen. Gleichartige Bewegungsbefehle ersetzen einander: wenn die
 * Uhr schneller neue Werte liefert, als die Kamera sie abarbeitet, zaehlt nur
 * der neueste. Ein Stopp oder ein Preset wird nie verworfen.
 */
Panasonic.prototype.enqueue = function (key, path, cmd, replaceable) {
  var entry = { key: key, path: path, cmd: cmd };
  if (replaceable) {
    for (var i = 0; i < this.queue.length; i++) {
      if (this.queue[i].key === key) {
        this.queue[i] = entry;
        this.pump();
        return;
      }
    }
  }
  this.queue.push(entry);
  this.pump();
};

Panasonic.prototype.pump = function () {
  if (this.busy || this.queue.length === 0) return;

  var now = Date.now();
  var wait = MIN_GAP_MS - (now - this.lastSend);
  if (wait > 0) {
    if (!this.timer) {
      var self = this;
      this.timer = setTimeout(function () {
        self.timer = null;
        self.pump();
      }, wait);
    }
    return;
  }

  var base = this.baseUrl();
  if (!base) {
    this.queue = [];
    if (this.onResult) this.onResult(false, 'Keine Adresse');
    return;
  }

  var entry = this.queue.shift();
  var url = base + entry.path + '?cmd=' + encodeURIComponent(entry.cmd) + '&res=1';
  var self = this;
  var xhr = new XMLHttpRequest();

  this.busy = true;
  this.lastSend = now;

  xhr.timeout = REQUEST_TIMEOUT_MS;
  xhr.onload = function () {
    self.busy = false;
    var body = (xhr.responseText || '').trim();
    if (xhr.status === 401) {
      self.report(false, 'Anmeldung abgelehnt');
    } else if (xhr.status !== 200) {
      self.report(false, 'HTTP ' + xhr.status);
    } else if (/^er[0-9]/i.test(body)) {
      // er1 = unbekannter Befehl, er2 = Kamera beschaeftigt, er3 = ausserhalb
      self.report(false, 'Kamera: ' + body);
    } else {
      self.report(true, body);
    }
    self.pump();
  };
  xhr.onerror = function () {
    self.busy = false;
    self.report(false, 'Nicht erreichbar');
    self.pump();
  };
  xhr.ontimeout = function () {
    self.busy = false;
    self.report(false, 'Keine Antwort');
    self.pump();
  };

  try {
    if (this.camera.user) {
      xhr.open('GET', url, true, this.camera.user, this.camera.pass || '');
    } else {
      xhr.open('GET', url, true);
    }
    xhr.send();
  } catch (err) {
    this.busy = false;
    this.report(false, 'Fehler: ' + err.message);
  }
};

Panasonic.prototype.report = function (ok, text) {
  if (this.onResult) this.onResult(ok, text);
};

Panasonic.prototype.ptzCmd = function (key, cmd, replaceable) {
  this.enqueue(key, '/cgi-bin/aw_ptz', cmd, replaceable);
};

/**
 * Sollzustand der Achsen setzen. Es gehen nur die Achsen hinaus, die sich
 * tatsaechlich geaendert haben.
 */
Panasonic.prototype.setAxes = function (pan, tilt, zoom, focus) {
  if (pan !== this.state.pan || tilt !== this.state.tilt) {
    this.state.pan = pan;
    this.state.tilt = tilt;
    this.ptzCmd('pt', '#PTS' + pad2(pan) + pad2(tilt), pan === 50 && tilt === 50 ? false : true);
  }
  if (zoom !== this.state.zoom) {
    this.state.zoom = zoom;
    this.ptzCmd('z', '#Z' + pad2(zoom), zoom !== 50);
  }
  if (focus !== this.state.focus) {
    this.state.focus = focus;
    this.ptzCmd('f', '#F' + pad2(focus), focus !== 50);
  }
};

/** Not-Stopp: alle Achsen anhalten, ohne Ruecksicht auf den Merkzustand. */
Panasonic.prototype.stopAll = function () {
  this.state.pan = 50;
  this.state.tilt = 50;
  this.state.zoom = 50;
  this.state.focus = 50;
  this.ptzCmd('stop-pt', '#PTS5050', false);
  this.ptzCmd('stop-z', '#Z50', false);
  this.ptzCmd('stop-f', '#F50', false);
};

Panasonic.prototype.recallPreset = function (index, offset) {
  this.ptzCmd('preset-' + index, '#R' + pad2Raw(index + (offset || 0)), false);
};

Panasonic.prototype.storePreset = function (index, offset) {
  this.ptzCmd('store-' + index, '#M' + pad2Raw(index + (offset || 0)), false);
};

Panasonic.prototype.setAutofocus = function (on) {
  this.ptzCmd('af', '#D1' + (on ? '1' : '0'), false);
};

Panasonic.prototype.setPower = function (on) {
  this.ptzCmd('power', '#O' + (on ? '1' : '0'), false);
};

/** Kurze Abfrage, um zu sehen, ob die Kamera ueberhaupt antwortet. */
Panasonic.prototype.ping = function () {
  this.ptzCmd('ping', '#O', false);
};

module.exports = Panasonic;
