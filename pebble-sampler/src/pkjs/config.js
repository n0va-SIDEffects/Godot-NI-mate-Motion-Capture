// Configuration page shown in the Pebble phone app (rendered by Clay).
var soundNames = require('./sounds.json');   // generated from the watch's sound table at build time

var allOn = [];
for (var i = 0; i < soundNames.length; i++) allOn.push(true);

module.exports = [
  { type: 'heading', defaultValue: 'Sampler' },
  { type: 'text', defaultValue: 'Einstellungen für das Soundboard auf der Pebble Time 2.' },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Wiedergabe' },
      { type: 'slider', messageKey: 'Volume', label: 'Lautstärke', defaultValue: 80, min: 10, max: 100, step: 10,
        description: 'Bei Verzerrungen etwas zurücknehmen, der Lautsprecher ist winzig.' },
      { type: 'toggle', messageKey: 'Shake', label: 'Schütteln spielt Zufalls-Sound', defaultValue: true },
      { type: 'toggle', messageKey: 'Touch', label: 'Touch-Bedienung', defaultValue: true,
        description: 'Antippen spielt ab, Wischen blättert. Nur auf Uhren mit Touchscreen.' }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Sounds in der Liste' },
      { type: 'text', defaultValue: 'Nur angehakte Sounds erscheinen auf der Uhr. Der Zufalls-Sound wählt ebenfalls nur aus diesen.' },
      { type: 'checkboxgroup', messageKey: 'Enabled', label: 'Anzeigen', defaultValue: allOn, options: soundNames }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Handy-Samples' },
      { type: 'text', defaultValue: 'Bis zu vier eigene Samples. Erzeuge mit <code>tools/import_sample.py clip.mp3 --export name.ima</code> eine .ima-Datei (16 kHz IMA-ADPCM, max. 3 Sekunden) und lege sie unter einer https-Adresse ab (z. B. Dropbox mit <code>dl=1</code>, GitHub raw, eigener Server). Die Samples werden bei jedem Start der Uhr-App vom Handy geladen; das dauert einige Sekunden pro Sample.' },
      { type: 'input', messageKey: 'SampleName[0]', label: 'Sample 1: Name', attributes: { placeholder: 'z. B. Chef lacht', maxlength: 20 } },
      { type: 'input', messageKey: 'SampleUrl[0]',  label: 'Sample 1: URL',  attributes: { placeholder: 'https://…/sample.ima', type: 'url' } },
      { type: 'input', messageKey: 'SampleName[1]', label: 'Sample 2: Name', attributes: { maxlength: 20 } },
      { type: 'input', messageKey: 'SampleUrl[1]',  label: 'Sample 2: URL',  attributes: { type: 'url' } },
      { type: 'input', messageKey: 'SampleName[2]', label: 'Sample 3: Name', attributes: { maxlength: 20 } },
      { type: 'input', messageKey: 'SampleUrl[2]',  label: 'Sample 3: URL',  attributes: { type: 'url' } },
      { type: 'input', messageKey: 'SampleName[3]', label: 'Sample 4: Name', attributes: { maxlength: 20 } },
      { type: 'input', messageKey: 'SampleUrl[3]',  label: 'Sample 4: URL',  attributes: { type: 'url' } }
    ]
  },
  { type: 'submit', defaultValue: 'Speichern' }
];
