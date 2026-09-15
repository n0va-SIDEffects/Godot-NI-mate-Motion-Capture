/**
 * Baseline-JPEG bis zur DC-Ebene dekodieren.
 *
 * Ein JPEG speichert das Bild in Bloecken von 8x8 Pixeln. Der erste
 * Koeffizient jedes Blocks (der DC-Wert) ist nichts anderes als der
 * Mittelwert dieser 64 Pixel. Wer nur diese Werte liest, bekommt das Bild
 * in einem Achtel der Kantenlaenge - ohne die aufwendige Ruecktransformation
 * und ohne die AC-Koeffizienten je auszupacken.
 *
 * Fuer eine Bildkontrolle auf einem 200 Pixel breiten Display ist das genau
 * die richtige Aufloesung: Ein Schnappschuss mit 640x360 ergibt 80x45.
 *
 * Unterstuetzt wird Baseline (SOF0/SOF1) mit beliebigem Subsampling und mit
 * Restart-Markern. Progressive JPEGs (SOF2) meldet der Dekoder als Fehler;
 * die Schnappschuss-Schnittstelle der Kameras liefert Baseline.
 */

var ZIGZAG_DC = 0;

function JpegError(message) {
  this.name = 'JpegError';
  this.message = message;
}

// --- Huffman ---------------------------------------------------------------

/**
 * Tabelle nach dem im JPEG-Standard beschriebenen Verfahren aufbauen:
 * je Codelaenge der kleinste und groesste gueltige Code plus ein Zeiger
 * in die Werteliste.
 */
function buildHuffmanTable(counts, values) {
  var mincode = new Int32Array(17);
  var maxcode = new Int32Array(17);
  var valptr = new Int32Array(17);
  var code = 0, k = 0;

  for (var len = 1; len <= 16; len++) {
    if (counts[len - 1] === 0) {
      maxcode[len] = -1;
      code <<= 1;
      continue;
    }
    valptr[len] = k;
    mincode[len] = code;
    code += counts[len - 1];
    k += counts[len - 1];
    maxcode[len] = code - 1;
    code <<= 1;
  }
  return { mincode: mincode, maxcode: maxcode, valptr: valptr, values: values };
}

// --- Bitleser --------------------------------------------------------------

function BitReader(data, offset) {
  this.data = data;
  this.pos = offset;
  this.bitBuffer = 0;
  this.bitCount = 0;
  this.atMarker = false;
}

BitReader.prototype.nextBit = function () {
  if (this.bitCount === 0) {
    if (this.pos >= this.data.length) {
      this.atMarker = true;
      return 0;
    }
    var b = this.data[this.pos++];
    if (b === 0xFF) {
      var next = this.data[this.pos];
      if (next === 0x00) {
        this.pos++;              // eingefuegtes Nullbyte, 0xFF ist Nutzdaten
      } else {
        // Ein echter Marker: der Bilddatenstrom ist hier zu Ende.
        this.atMarker = true;
        return 0;
      }
    }
    this.bitBuffer = b;
    this.bitCount = 8;
  }
  this.bitCount--;
  return (this.bitBuffer >> this.bitCount) & 1;
};

BitReader.prototype.receive = function (length) {
  var v = 0;
  for (var i = 0; i < length; i++) {
    v = (v << 1) | this.nextBit();
  }
  return v;
};

/** Vorzeichenbehaftete Differenz nach JPEG-Konvention. */
BitReader.prototype.receiveAndExtend = function (length) {
  if (length === 0) return 0;
  var v = this.receive(length);
  return v < (1 << (length - 1)) ? v - (1 << length) + 1 : v;
};

BitReader.prototype.decodeHuffman = function (table) {
  var code = this.nextBit();
  for (var len = 1; len <= 16; len++) {
    if (table.maxcode[len] >= 0 && code <= table.maxcode[len]) {
      return table.values[table.valptr[len] + code - table.mincode[len]];
    }
    code = (code << 1) | this.nextBit();
  }
  throw new JpegError('Ungueltiger Huffman-Code');
};

/** Nach einem Restart-Marker sauber wieder aufsetzen. */
BitReader.prototype.restart = function () {
  this.bitCount = 0;
  this.atMarker = false;
  // Bis zum naechsten RSTn-Marker vorspulen und ihn ueberspringen.
  while (this.pos < this.data.length - 1) {
    if (this.data[this.pos] === 0xFF) {
      var m = this.data[this.pos + 1];
      if (m >= 0xD0 && m <= 0xD7) {
        this.pos += 2;
        return true;
      }
      if (m !== 0x00) {
        return false;            // anderer Marker: Bild zu Ende
      }
    }
    this.pos++;
  }
  return false;
};

// --- Segmente lesen --------------------------------------------------------

function parseSegments(data) {
  var pos = 0;
  if (data[0] !== 0xFF || data[1] !== 0xD8) {
    throw new JpegError('Kein JPEG (SOI fehlt)');
  }
  pos = 2;

  var quantTables = [];      // nur der DC-Eintrag wird gebraucht
  var huffDC = [], huffAC = [];
  var frame = null;
  var restartInterval = 0;
  var scanOffset = -1;
  var scanComponents = null;

  while (pos < data.length - 1) {
    if (data[pos] !== 0xFF) { pos++; continue; }
    var marker = data[pos + 1];
    pos += 2;

    if (marker === 0xD8 || marker === 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
      continue;                                  // ohne Nutzlast
    }
    if (marker === 0xD9) break;                  // Bildende

    var length = (data[pos] << 8) | data[pos + 1];
    var segStart = pos + 2;
    var segEnd = pos + length;

    switch (marker) {
      case 0xDB:                                 // Quantisierungstabellen
        var q = segStart;
        while (q < segEnd) {
          var pq = data[q] >> 4, tq = data[q] & 15;
          q++;
          // Der DC-Wert steht an Position 0 der Zickzack-Folge.
          quantTables[tq] = pq ? ((data[q] << 8) | data[q + 1]) : data[q];
          q += pq ? 128 : 64;
        }
        break;

      case 0xC0:                                 // Baseline
      case 0xC1:                                 // erweitert sequenziell
        frame = readFrame(data, segStart);
        break;

      case 0xC2:
        throw new JpegError('Progressives JPEG wird nicht unterstuetzt');

      case 0xC4:                                 // Huffman-Tabellen
        var h = segStart;
        while (h < segEnd) {
          var tc = data[h] >> 4, th = data[h] & 15;
          h++;
          var counts = [], total = 0;
          for (var i = 0; i < 16; i++) {
            counts.push(data[h + i]);
            total += data[h + i];
          }
          h += 16;
          var values = [];
          for (var v = 0; v < total; v++) values.push(data[h + v]);
          h += total;
          var table = buildHuffmanTable(counts, values);
          if (tc === 0) huffDC[th] = table; else huffAC[th] = table;
        }
        break;

      case 0xDD:                                 // Restart-Abstand
        restartInterval = (data[segStart] << 8) | data[segStart + 1];
        break;

      case 0xDA:                                 // Beginn der Bilddaten
        var n = data[segStart];
        scanComponents = [];
        for (var c = 0; c < n; c++) {
          var cs = data[segStart + 1 + c * 2];
          var tables = data[segStart + 2 + c * 2];
          scanComponents.push({ id: cs, dc: tables >> 4, ac: tables & 15 });
        }
        scanOffset = segEnd;
        break;

      default:
        break;
    }

    if (scanOffset >= 0) break;
    pos = segEnd;
  }

  if (!frame) throw new JpegError('Bildkopf (SOF) fehlt');
  if (scanOffset < 0) throw new JpegError('Bilddaten (SOS) fehlen');

  return {
    frame: frame, quantTables: quantTables,
    huffDC: huffDC, huffAC: huffAC,
    restartInterval: restartInterval,
    scanOffset: scanOffset, scanComponents: scanComponents
  };
}

function readFrame(data, p) {
  var precision = data[p];
  var height = (data[p + 1] << 8) | data[p + 2];
  var width = (data[p + 3] << 8) | data[p + 4];
  var count = data[p + 5];
  if (precision !== 8) throw new JpegError('Nur 8 Bit je Kanal');
  if (count !== 1 && count !== 3) {
    throw new JpegError('Unerwartete Kanalzahl: ' + count);
  }

  var components = [];
  var maxH = 1, maxV = 1;
  for (var i = 0; i < count; i++) {
    var o = p + 6 + i * 3;
    var comp = {
      id: data[o], h: data[o + 1] >> 4, v: data[o + 1] & 15, tq: data[o + 2]
    };
    if (comp.h < 1 || comp.v < 1) throw new JpegError('Ungueltige Abtastung');
    if (comp.h > maxH) maxH = comp.h;
    if (comp.v > maxV) maxV = comp.v;
    components.push(comp);
  }

  var mcusPerLine = Math.ceil(width / (8 * maxH));
  var mcusPerColumn = Math.ceil(height / (8 * maxV));
  for (var j = 0; j < components.length; j++) {
    var cm = components[j];
    cm.blocksPerLine = mcusPerLine * cm.h;
    cm.blocksPerColumn = mcusPerColumn * cm.v;
    cm.samples = new Uint8Array(cm.blocksPerLine * cm.blocksPerColumn);
    cm.pred = 0;
  }

  return {
    width: width, height: height, components: components,
    maxH: maxH, maxV: maxV,
    mcusPerLine: mcusPerLine, mcusPerColumn: mcusPerColumn
  };
}

// --- Bilddaten: nur die DC-Werte einsammeln --------------------------------

function decodeScan(data, parsed) {
  var frame = parsed.frame;
  var reader = new BitReader(data, parsed.scanOffset);
  var comps = frame.components;

  // Die Scan-Kopfdaten sagen, welche Huffman-Tabellen je Kanal gelten.
  for (var i = 0; i < parsed.scanComponents.length; i++) {
    var sc = parsed.scanComponents[i];
    for (var j = 0; j < comps.length; j++) {
      if (comps[j].id === sc.id) {
        comps[j].dcTable = parsed.huffDC[sc.dc];
        comps[j].acTable = parsed.huffAC[sc.ac];
      }
    }
  }

  var totalMcus = frame.mcusPerLine * frame.mcusPerColumn;
  var sinceRestart = 0;

  for (var mcu = 0; mcu < totalMcus; mcu++) {
    if (parsed.restartInterval && sinceRestart === parsed.restartInterval) {
      if (!reader.restart()) break;
      for (var r = 0; r < comps.length; r++) comps[r].pred = 0;
      sinceRestart = 0;
    }
    sinceRestart++;

    var mcuRow = (mcu / frame.mcusPerLine) | 0;
    var mcuCol = mcu % frame.mcusPerLine;

    for (var c = 0; c < comps.length; c++) {
      var comp = comps[c];
      for (var vy = 0; vy < comp.v; vy++) {
        for (var hx = 0; hx < comp.h; hx++) {
          var row = mcuRow * comp.v + vy;
          var col = mcuCol * comp.h + hx;
          decodeBlockDC(reader, comp, parsed.quantTables[comp.tq],
                        row * comp.blocksPerLine + col);
          if (reader.atMarker) {
            // Abgeschnittener Datenstrom: was da ist, reicht fuer eine
            // Vorschau - der Rest bleibt grau.
            return;
          }
        }
      }
    }
  }
}

function decodeBlockDC(reader, comp, quantDC, index) {
  var t = reader.decodeHuffman(comp.dcTable);
  var diff = t === 0 ? 0 : reader.receiveAndExtend(t);
  comp.pred += diff;

  // Der DC-Anteil der Ruecktransformation ist fuer alle 64 Pixel gleich:
  // Koeffizient mal Quantisierung geteilt durch acht, plus der Versatz
  // um 128, mit dem JPEG die Werte zentriert.
  var value = ((comp.pred * quantDC) >> 3) + 128;
  comp.samples[index] = value < 0 ? 0 : (value > 255 ? 255 : value);

  // Die AC-Koeffizienten werden nur ueberlesen, nicht ausgewertet.
  var k = 1;
  while (k < 64) {
    var rs = reader.decodeHuffman(comp.acTable);
    var s = rs & 15, r = rs >> 4;
    if (s === 0) {
      if (r < 15) break;        // Blockende
      k += 16;
    } else {
      k += r;
      reader.receive(s);
      k++;
    }
    if (reader.atMarker) return;
  }
}

// --- Zusammensetzen --------------------------------------------------------

function toRgb(frame) {
  var w = Math.ceil(frame.width / 8);
  var h = Math.ceil(frame.height / 8);
  var out = new Uint8Array(w * h * 3);
  var comps = frame.components;
  var grau = comps.length === 1;

  for (var y = 0; y < h; y++) {
    for (var x = 0; x < w; x++) {
      var o = (y * w + x) * 3;
      if (grau) {
        var g = sampleAt(comps[0], frame, x, y);
        out[o] = out[o + 1] = out[o + 2] = g;
      } else {
        var Y  = sampleAt(comps[0], frame, x, y);
        var Cb = sampleAt(comps[1], frame, x, y) - 128;
        var Cr = sampleAt(comps[2], frame, x, y) - 128;
        out[o]     = clamp8(Y + 1.402 * Cr);
        out[o + 1] = clamp8(Y - 0.344136 * Cb - 0.714136 * Cr);
        out[o + 2] = clamp8(Y + 1.772 * Cb);
      }
    }
  }
  return { width: w, height: h, data: out };
}

/** Unterabgetastete Kanaele auf das Raster des Vollbilds beziehen. */
function sampleAt(comp, frame, x, y) {
  var sx = (x * comp.h / frame.maxH) | 0;
  var sy = (y * comp.v / frame.maxV) | 0;
  if (sx >= comp.blocksPerLine) sx = comp.blocksPerLine - 1;
  if (sy >= comp.blocksPerColumn) sy = comp.blocksPerColumn - 1;
  return comp.samples[sy * comp.blocksPerLine + sx];
}

function clamp8(v) {
  v = v | 0;
  return v < 0 ? 0 : (v > 255 ? 255 : v);
}

/**
 * Liefert { width, height, data } mit RGB-Bytes in einem Achtel der
 * Kantenlaenge des Originals.
 */
function decodeDC(bytes) {
  var data = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
  var parsed = parseSegments(data);
  decodeScan(data, parsed);
  var result = toRgb(parsed.frame);
  result.fullWidth = parsed.frame.width;
  result.fullHeight = parsed.frame.height;
  return result;
}

module.exports = {
  decodeDC: decodeDC,
  JpegError: JpegError
};
