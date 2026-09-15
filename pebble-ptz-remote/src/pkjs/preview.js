/**
 * Vorschaubild: Schnappschuss holen, auf das Pebble-Display eindampfen und
 * in Haeppchen an die Uhr schicken.
 *
 * Der Weg eines Bildes:
 *   JPEG von der Kamera  ->  DC-Ebene (ein Achtel der Kantenlaenge)
 *   ->  auf Zielgroesse mitteln  ->  16 Farben aus dem Pebble-Farbraum
 *   ->  zwei Bildpunkte je Byte  ->  AppMessage-Haeppchen
 *
 * Die Bluetooth-Strecke ist der Engpass, nicht das WLAN. Deshalb darf die
 * Kamera ruhig ein grosses JPEG liefern - nur was zur Uhr geht, muss klein
 * sein.
 */

var jpeg = require('./jpeg');

var SNAPSHOT_TIMEOUT_MS = 4000;

// Zwei Stellschrauben der Bildaufbereitung, an einem Vergleich mit
// Buehnenmotiven eingestellt (siehe test/palette.compare.js).
var TUNING = {
  // Mindestabstand zwischen zwei Palettenfarben, quadriert. Der Farbraum
  // der Uhr kennt je Kanal nur vier Stufen im Abstand 85; ein Wert um
  // 85*85/4 haelt die Palette gespreizt, ohne feine Abstufungen im
  // Lichtkegel zu verbieten.
  minPaletteDist: 1800,
  // Wie viel vom Rundungsfehler an die Nachbarn weitergereicht wird.
  // 4 ist die volle Weitergabe nach Floyd-Steinberg.
  ditherShift: 4,
  // Obergrenze fuer den weitergereichten Fehler je Kanal. Im Schwarzbereich
  // springt der Farbraum der Uhr von 0 auf 85; ohne Deckel schaukelt sich
  // der Fehler ueber die Nachbarn zu bunten Tupfern auf einer dunklen
  // Buehne auf. Mit Deckel bleiben Farbtoene richtig und das Rauschen
  // bleibt lokal.
  errorLimit: 48
};
var MAX_RETRIES = 3;

/**
 * Aus der Antwort ein Byte-Feld machen.
 *
 * Der uebliche Weg ist responseType 'arraybuffer', und so arbeitet die
 * Pebble-App auf dem Telefon. Das Ergebnis wird trotzdem geprueft, bevor es
 * weitergereicht wird, und notfalls aus der Zeichenkettenfassung derselben
 * Antwort gewonnen. Denn eine Umgebung, die den Puffer nicht durchreicht,
 * meldet keinen Fehler, sondern liefert stillschweigend Nullen - daraus
 * wuerde ohne Pruefung ein raetselhafter Fehler weit spaeter im Ablauf.
 *
 * Im Pebble-Emulator hilft auch der zweite Weg nicht, dort sind beide
 * Fassungen der Antwort unbrauchbar. Siehe README, Abschnitt Emulator.
 */
function looksLikeJpeg(bytes) {
  return !!bytes && bytes.length > 100 && bytes[0] === 0xFF && bytes[1] === 0xD8;
}

function fromBuffer(response) {
  if (!response) return null;
  if (response instanceof Uint8Array) return response;
  try {
    var view = new Uint8Array(response);
    if (view.length > 0) return view;
  } catch (err) {
    // weiter unten
  }
  var len = response.byteLength || response.length;
  if (typeof len === 'number' && len > 0) {
    var out = new Uint8Array(len);
    for (var i = 0; i < len; i++) {
      out[i] = response[i] & 0xFF;
    }
    return out;
  }
  return null;
}

function fromText(text) {
  if (!text || !text.length) return null;
  var out = new Uint8Array(text.length);
  for (var i = 0; i < text.length; i++) {
    out[i] = text.charCodeAt(i) & 0xFF;
  }
  return out;
}

function toBytes(response, text) {
  var bytes = fromBuffer(response);
  if (looksLikeJpeg(bytes)) return bytes;

  var fallback = fromText(text);
  if (looksLikeJpeg(fallback)) return fallback;

  return bytes && bytes.length > 100 ? bytes : fallback;
}

function Preview() {
  this.busy = false;
  this.aborted = false;
  this.chunkSize = 1024;      // wird vom Handshake mit der Uhr ueberschrieben
  this.preferred = 0;         // welcher der Schnappschuss-Wege bei dieser Kamera geht
  this.onStatus = null;       // function(text, istFehler)
  this.send = null;           // function(dict, ok, fail)
}

// --- Farbraum der Uhr ------------------------------------------------------

/** Ein Kanal auf die vier Stufen des Pebble-Farbraums bringen. */
function level(v) {
  var l = Math.round(v / 85);
  return l < 0 ? 0 : (l > 3 ? 3 : l);
}

/** GColor8: zwei Bit je Kanal, oberste zwei Bit sind volle Deckkraft. */
function pebbleByte(r, g, b) {
  return 0xC0 | (level(r) << 4) | (level(g) << 2) | level(b);
}

// --- Bildaufbereitung ------------------------------------------------------

/** Mittelwert-Verkleinerung. Beim Verkleinern ist das der ehrlichste Filter. */
function resample(src, sw, sh, tw, th) {
  var out = new Uint8Array(tw * th * 3);
  for (var y = 0; y < th; y++) {
    var y0 = (y * sh / th) | 0;
    var y1 = (((y + 1) * sh / th) | 0);
    if (y1 <= y0) y1 = y0 + 1;
    for (var x = 0; x < tw; x++) {
      var x0 = (x * sw / tw) | 0;
      var x1 = (((x + 1) * sw / tw) | 0);
      if (x1 <= x0) x1 = x0 + 1;

      var r = 0, g = 0, b = 0, n = 0;
      for (var yy = y0; yy < y1 && yy < sh; yy++) {
        for (var xx = x0; xx < x1 && xx < sw; xx++) {
          var o = (yy * sw + xx) * 3;
          r += src[o]; g += src[o + 1]; b += src[o + 2];
          n++;
        }
      }
      var t = (y * tw + x) * 3;
      out[t]     = (r / n) | 0;
      out[t + 1] = (g / n) | 0;
      out[t + 2] = (b / n) | 0;
    }
  }
  return out;
}

/**
 * Die sechzehn Farben waehlen, die dem Bild am meisten bringen.
 *
 * Buehnenbilder sind farbarm: viel Dunkel, ein Lichtkegel, ein paar Toene
 * darin. Eine feste Palette wuerde die Haelfte ihrer Eintraege an Farben
 * verschwenden, die gar nicht vorkommen. Deshalb zaehlt diese Funktion, was
 * wirklich im Bild ist, und nimmt davon die sechzehn haeufigsten.
 */
function buildPalette(rgb, count) {
  var hist = {};
  for (var i = 0; i < rgb.length; i += 3) {
    var key = (level(rgb[i]) << 4) | (level(rgb[i + 1]) << 2) | level(rgb[i + 2]);
    hist[key] = (hist[key] || 0) + 1;
  }
  var keys = Object.keys(hist).sort(function (a, b) { return hist[b] - hist[a]; });

  function toRgb(v) {
    return [((v >> 4) & 3) * 85, ((v >> 2) & 3) * 85, (v & 3) * 85];
  }

  // Ein dunkles Buehnenbild besteht zu neun Zehnteln aus fast schwarzen
  // Bildpunkten. Nimmt man stur die haeufigsten Farben, gehen fast alle
  // sechzehn Plaetze an kaum unterscheidbare Dunkeltoene, und der
  // Lichtkegel - das einzig Interessante - bekommt zwei. Deshalb muessen
  // die Farben einen Mindestabstand halten; erst wenn sich so nicht genug
  // finden, wird die Anforderung gelockert.
  var palette = [];
  var minDist = TUNING.minPaletteDist;
  while (palette.length < count && minDist >= 0) {
    for (var k = 0; k < keys.length && palette.length < count; k++) {
      var c = toRgb(parseInt(keys[k], 10));
      var weit = true;
      for (var q = 0; q < palette.length; q++) {
        var dr = c[0] - palette[q][0], dg = c[1] - palette[q][1], db = c[2] - palette[q][2];
        if (dr * dr + dg * dg + db * db <= minDist) { weit = false; break; }
      }
      if (weit) palette.push(c);
    }
    minDist = minDist > 0 ? (minDist >> 2) - 1 : -1;
  }

  var grau = [0, 85, 170, 255];
  for (var f = 0; palette.length < count; f++) {
    var g = grau[f % 4];
    palette.push([g, g, g]);
  }
  return palette;
}

function nearest(palette, r, g, b) {
  var best = 0, bestDist = Infinity;
  for (var i = 0; i < palette.length; i++) {
    var dr = r - palette[i][0], dg = g - palette[i][1], db = b - palette[i][2];
    // Gewichtet nach der Empfindlichkeit des Auges.
    var d = dr * dr * 3 + dg * dg * 6 + db * db;
    if (d < bestDist) { bestDist = d; best = i; }
  }
  return best;
}

/**
 * Auf die Palette abbilden und den Rundungsfehler an die Nachbarn weitergeben
 * (Floyd-Steinberg). Ohne das bekommen weiche Verlaeufe sichtbare Stufen.
 */
function quantize(rgb, w, h, palette) {
  var work = new Int16Array(rgb.length);
  for (var i = 0; i < rgb.length; i++) work[i] = rgb[i];

  var indices = new Uint8Array(w * h);

  for (var y = 0; y < h; y++) {
    for (var x = 0; x < w; x++) {
      var o = (y * w + x) * 3;
      var r = work[o], g = work[o + 1], b = work[o + 2];
      var idx = nearest(palette, r, g, b);
      indices[y * w + x] = idx;

      var lim = TUNING.errorLimit;
      var er = clampError(r - palette[idx][0], lim);
      var eg = clampError(g - palette[idx][1], lim);
      var eb = clampError(b - palette[idx][2], lim);

      spread(work, w, h, x + 1, y,     er, eg, eb, 7);
      spread(work, w, h, x - 1, y + 1, er, eg, eb, 3);
      spread(work, w, h, x,     y + 1, er, eg, eb, 5);
      spread(work, w, h, x + 1, y + 1, er, eg, eb, 1);
    }
  }
  return indices;
}

/**
 * Den Rundungsfehler an einen Nachbarn weitergeben - aber nur zur Haelfte.
 *
 * Mit dem vollen Fehler bekommt eine dunkle Buehne bunte Sprenkel: der
 * Fehler eines fast schwarzen Bildpunkts schaukelt sich ueber die Nachbarn
 * zu sichtbaren Farbtupfern auf. Die halbe Weitergabe glaettet Verlaeufe
 * immer noch, laesst dunkle Flaechen aber dunkel.
 */
function clampError(e, limit) {
  return e < -limit ? -limit : (e > limit ? limit : e);
}

function spread(work, w, h, x, y, er, eg, eb, weight) {
  if (x < 0 || x >= w || y >= h) return;
  var o = (y * w + x) * 3;
  work[o]     += (er * weight) >> TUNING.ditherShift;
  work[o + 1] += (eg * weight) >> TUNING.ditherShift;
  work[o + 2] += (eb * weight) >> TUNING.ditherShift;
}

/** Zwei Bildpunkte je Byte - das halbiert, was ueber Bluetooth muss. */
function pack4bit(indices, w, h) {
  var rowBytes = (w + 1) >> 1;
  var out = new Uint8Array(rowBytes * h);
  for (var y = 0; y < h; y++) {
    for (var x = 0; x < w; x++) {
      var v = indices[y * w + x] & 15;
      var o = y * rowBytes + (x >> 1);
      if ((x & 1) === 0) {
        out[o] = (out[o] & 0x0F) | (v << 4);
      } else {
        out[o] = (out[o] & 0xF0) | v;
      }
    }
  }
  return out;
}

// --- Schnappschuss holen ---------------------------------------------------

/**
 * Panasonic bietet je nach Modell zwei Wege zum Einzelbild. Der erste, der
 * antwortet, wird fuer die naechsten Male gemerkt.
 */
Preview.prototype.snapshotUrls = function (base, resolution) {
  var nonce = Date.now() % 100000;
  return [
    base + '/cgi-bin/camera?resolution=' + resolution + '&quality=1&page=' + nonce,
    base + '/cgi-bin/view.cgi?action=snapshot&n=' + nonce
  ];
};

Preview.prototype.fetch = function (camera, base, resolution, cb) {
  var self = this;
  var all = this.snapshotUrls(base, resolution);
  // Den zuletzt erfolgreichen Weg zuerst versuchen.
  var urls = [all[this.preferred]];
  for (var i = 0; i < all.length; i++) {
    if (i !== this.preferred) urls.push(all[i]);
  }
  var order = [this.preferred];
  for (var j = 0; j < all.length; j++) {
    if (j !== this.preferred) order.push(j);
  }
  var attempt = 0;

  function tryNext() {
    if (attempt >= urls.length) {
      cb(null, 'Kein Bild von der Kamera');
      return;
    }
    var which = order[attempt];
    var url = urls[attempt++];
    var xhr = new XMLHttpRequest();
    xhr.responseType = 'arraybuffer';
    xhr.timeout = SNAPSHOT_TIMEOUT_MS;

    xhr.onload = function () {
      if (xhr.status === 200 && xhr.response) {
        var bytes = toBytes(xhr.response, xhr.responseText);
        if (bytes && bytes.length > 100) {
          self.preferred = which;
          cb(bytes, null);
        } else {
          cb(null, 'Bilddaten unbrauchbar');
        }
      } else if (xhr.status === 401) {
        cb(null, 'Anmeldung abgelehnt');
      } else {
        tryNext();
      }
    };
    xhr.onerror = function () { tryNext(); };
    xhr.ontimeout = function () { cb(null, 'Bild: keine Antwort'); };

    try {
      if (camera.user) {
        xhr.open('GET', url, true, camera.user, camera.pass || '');
      } else {
        xhr.open('GET', url, true);
      }
      xhr.send();
    } catch (err) {
      tryNext();
    }
  }
  tryNext();
};

// --- Gesamtablauf ----------------------------------------------------------

Preview.prototype.abort = function () {
  if (this.busy) {
    this.aborted = true;
  }
};

Preview.prototype.setChunkSize = function (bytes) {
  // Etwas Luft fuer die Schluessel und den Rahmen der Nachricht lassen.
  var usable = bytes - 64;
  if (usable < 128) usable = 128;
  if (usable > 4096) usable = 4096;
  this.chunkSize = usable;
};

Preview.prototype.capture = function (camera, base, width, resolution) {
  if (this.busy) return;
  this.busy = true;
  this.aborted = false;

  var self = this;
  this.status('Bild wird geholt');

  this.fetch(camera, base, resolution, function (bytes, err) {
    if (err || self.aborted) {
      self.busy = false;
      if (err) self.status(err, true);
      return;
    }
    var img;
    try {
      img = jpeg.decodeDC(bytes);
    } catch (e) {
      self.busy = false;
      console.log('Vorschau: ' + e.message + ' (' + bytes.length + ' Byte, ' +
                  'Beginn ' + bytes[0] + ' ' + bytes[1] + ')');
      self.status('Bild: ' + e.message, true);
      return;
    }
    if (self.aborted) { self.busy = false; return; }

    // Seitenverhaeltnis der Kamera beibehalten, Breite gibt den Ton an.
    var tw = width;
    var th = Math.round(width * img.height / img.width);
    if (th < 1) th = 1;
    if (th % 2) th++;

    var small = resample(img.data, img.width, img.height, tw, th);
    var palette = buildPalette(small, 16);
    var indices = quantize(small, tw, th, palette);
    var packed = pack4bit(indices, tw, th);

    self.transmit(tw, th, palette, packed);
  });
};

Preview.prototype.transmit = function (w, h, palette, data) {
  var self = this;
  var chunks = Math.ceil(data.length / this.chunkSize);

  var palBytes = [];
  for (var i = 0; i < palette.length; i++) {
    palBytes.push(pebbleByte(palette[i][0], palette[i][1], palette[i][2]));
  }

  console.log('Vorschau: ' + w + 'x' + h + ', ' + data.length +
              ' Byte in ' + chunks + ' Haeppchen zu je ' + this.chunkSize);

  this.send({
    IMG_W: w, IMG_H: h, IMG_LEN: data.length, IMG_PAL: palBytes
  }, function () {
    sendChunk(0, 0);
  }, function (err) {
    self.busy = false;
    console.log('Vorschau: Bildkopf abgelehnt: ' + JSON.stringify(err));
    self.status('Bild nicht zugestellt', true);
  });

  function sendChunk(index, retries) {
    if (self.aborted) {
      self.busy = false;
      self.status('');
      return;
    }
    if (index >= chunks) {
      self.busy = false;
      self.status('');
      return;
    }
    var start = index * self.chunkSize;
    var slice = Array.prototype.slice.call(data.subarray(start, start + self.chunkSize));

    // Der Versatz statt einer laufenden Nummer: dann muss die Uhr die
    // Haeppchengroesse nicht kennen und darf sie sogar wechseln.
    self.send({ IMG_OFF: start, IMG_DATA: slice }, function () {
      sendChunk(index + 1, 0);
    }, function (err) {
      console.log('Vorschau: Haeppchen ' + index + ' abgelehnt: ' + JSON.stringify(err));
      if (retries < MAX_RETRIES) {
        setTimeout(function () { sendChunk(index, retries + 1); }, 120);
      } else {
        self.busy = false;
        self.status('Bild abgebrochen', true);
      }
    });
  }
};

Preview.prototype.status = function (text, isError) {
  if (this.onStatus) this.onStatus(text, !!isError);
};

module.exports = Preview;

// Fuer Tests und Vergleichsbilder.
module.exports._internals = {
  resample: resample, buildPalette: buildPalette,
  quantize: quantize, pack4bit: pack4bit, pebbleByte: pebbleByte,
  tuning: TUNING
};
