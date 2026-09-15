/**
 * Prueft den DC-Dekoder gegen echte JPEG-Dateien.
 *
 *   node test/jpeg.test.js
 *
 * Als Vergleich dient das jeweilige Originalbild, mit einem Mittelwertfilter
 * auf ein Achtel verkleinert - denn genau das ist ein DC-Koeffizient: der
 * Mittelwert seines 8x8-Blocks. Weicht der Dekoder davon nur im Rahmen der
 * JPEG-Kompression ab, arbeitet er richtig.
 */

var fs = require('fs');
var path = require('path');
var jpeg = require('../src/pkjs/jpeg');

var fixtures = path.join(__dirname, 'fixtures');
var meta = JSON.parse(fs.readFileSync(path.join(fixtures, 'referenz.json'), 'utf8'));

// Die Vergleichsbilder liegen als rohe RGB-Bytes daneben; erzeugt werden
// Bilder und Vergleich von test/fixtures/erzeugen.py.
function referenz(info) {
  return fs.readFileSync(path.join(fixtures, info.ref));
}

var failed = 0;

function pruefe(name) {
  var info = meta[name];
  var bytes = new Uint8Array(fs.readFileSync(path.join(fixtures, info.file)));

  var t0 = Date.now();
  var img;
  try {
    img = jpeg.decodeDC(bytes);
  } catch (err) {
    failed++;
    console.log('  FEHL ' + name + ': ' + err.message);
    return;
  }
  var dauer = Date.now() - t0;

  if (img.width !== info.dcWidth || img.height !== info.dcHeight) {
    failed++;
    console.log('  FEHL ' + name + ': Groesse ' + img.width + 'x' + img.height +
                ', erwartet ' + info.dcWidth + 'x' + info.dcHeight);
    return;
  }

  // Mittlere und groesste Abweichung je Farbkanal
  var ref = referenz(info);
  var summe = 0, max = 0;
  for (var i = 0; i < img.data.length; i++) {
    var d = Math.abs(img.data[i] - ref[i]);
    summe += d;
    if (d > max) max = d;
  }
  var mittel = summe / img.data.length;

  // JPEG ist verlustbehaftet; unter 12 Stufen mittlerer Abweichung auf einer
  // Skala von 256 ist das Bild unverwechselbar dasselbe.
  var ok = mittel < 12;
  if (!ok) failed++;
  console.log('  ' + (ok ? 'ok  ' : 'FEHL') + ' ' + name.padEnd(9) +
              img.width + 'x' + img.height +
              '  Abweichung: Mittel ' + mittel.toFixed(1) + ', max ' + max +
              '  (' + dauer + ' ms)');
}

console.log('JPEG-Dekoder (nur DC-Ebene)');
Object.keys(meta).forEach(pruefe);

// Ein beschaedigter Datenstrom darf die App nicht mitreissen.
try {
  var kaputt = new Uint8Array(fs.readFileSync(path.join(fixtures, '420.jpg')));
  jpeg.decodeDC(kaputt.slice(0, Math.floor(kaputt.length / 2)));
  console.log('  ok   abgeschnittene Datei liefert ein Teilbild statt Absturz');
} catch (err) {
  // Eine saubere Fehlermeldung ist ebenfalls in Ordnung.
  if (err instanceof jpeg.JpegError || err.name === 'JpegError') {
    console.log('  ok   abgeschnittene Datei meldet sauberen Fehler');
  } else {
    failed++;
    console.log('  FEHL abgeschnittene Datei: ' + err.message);
  }
}

try {
  jpeg.decodeDC(new Uint8Array([1, 2, 3, 4]));
  failed++;
  console.log('  FEHL Unsinn wurde nicht erkannt');
} catch (err) {
  console.log('  ok   Unsinn wird als Fehler gemeldet');
}

console.log(failed === 0 ? '\nAlle Pruefungen bestanden.'
                         : '\n' + failed + ' fehlgeschlagen.');
process.exit(failed === 0 ? 0 : 1);
