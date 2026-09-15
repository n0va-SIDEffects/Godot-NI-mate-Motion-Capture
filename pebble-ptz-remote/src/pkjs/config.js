/**
 * Konfigurationsseite (Clay). Wird in der Pebble-App auf dem Telefon
 * angezeigt. Die Kameradaten bleiben auf dem Telefon, nur die Bedienwerte
 * wandern auf die Uhr.
 */

var CAMERA_SLOTS = 8;

var config = [
  { type: 'heading', defaultValue: 'PTZ Remote' },
  {
    type: 'text',
    defaultValue: 'Fernsteuerung f&uuml;r Panasonic PTZ-Kameras (AW-Serie). ' +
      'Die Kamera muss im selben Netz erreichbar sein wie das Telefon.'
  },

  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Bedienung' },
      {
        type: 'slider',
        messageKey: 'cfgSpeed',
        label: 'Tempo (Stufe)',
        description: 'Wie viel von der maximalen Kamerageschwindigkeit die Uhr ausreizt. ' +
          'F&uuml;r Fahrten im laufenden Betrieb sind 1 bis 2 meist genug.',
        defaultValue: 3, min: 1, max: 5, step: 1
      },
      {
        type: 'slider',
        messageKey: 'cfgPresetCount',
        label: 'Presets im Men&uuml;',
        defaultValue: 8, min: 1, max: 24, step: 1
      },
      {
        type: 'select',
        messageKey: 'presetOffset',
        label: 'Preset-Z&auml;hlung',
        description: 'Panasonic z&auml;hlt intern ab 0. Wenn Preset 1 auf der Uhr die ' +
          'falsche Position abruft, hier umstellen.',
        defaultValue: '0',
        options: [
          { label: 'Preset 1 = Kamera-Speicher 0 (Standard)', value: '0' },
          { label: 'Preset 1 = Kamera-Speicher 1', value: '1' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'cfgVibrate',
        label: 'Vibration bei Fehler',
        defaultValue: true
      }
    ]
  },

  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Neigungssteuerung' },
      {
        type: 'text',
        defaultValue: 'Die Uhr misst die Neigung gegen die Schwerkraft. Nullpunkt ist ' +
          'immer die Haltung in dem Moment, in dem der Totmannschalter gedr&uuml;ckt wird.'
      },
      {
        type: 'slider',
        messageKey: 'cfgGyroSens',
        label: 'Empfindlichkeit',
        description: '1 verlangt eine deutliche Bewegung (rund 40 Grad f&uuml;r Vollausschlag), ' +
          '10 reagiert schon auf ein kurzes Kippen (rund 12 Grad).',
        defaultValue: 5, min: 1, max: 10, step: 1
      },
      {
        type: 'slider',
        messageKey: 'cfgGyroDead',
        label: 'Totzone',
        description: 'Wie ruhig die Hand sein darf, ohne dass die Kamera anf&auml;ngt zu ' +
          'fahren. Gr&ouml;&szlig;er hei&szlig;t gelassener.',
        defaultValue: 60, min: 20, max: 200, step: 10
      },
      {
        type: 'select',
        messageKey: 'cfgGyroLock',
        label: 'Achsen beim Start',
        defaultValue: '0',
        options: [
          { label: 'Pan + Tilt', value: '0' },
          { label: 'Pan + Zoom', value: '1' },
          { label: 'Nur Tilt', value: '2' }
        ]
      },
      { type: 'toggle', messageKey: 'cfgInvertPan',  label: 'Pan umkehren',  defaultValue: false },
      { type: 'toggle', messageKey: 'cfgInvertTilt', label: 'Tilt umkehren', defaultValue: false }
    ]
  }
];

config.push({
  type: 'section',
  items: [
    { type: 'heading', defaultValue: 'Vorschaubild' },
    {
      type: 'text',
      defaultValue: 'Die Uhr holt nach jeder Fahrt und nach jedem Preset ein frisches ' +
        'Einzelbild. Kein Livebild: die Bluetooth-Strecke zur Uhr schafft rund ' +
        '1,6 Kilobyte je Sekunde, ein Bild braucht je nach Gr&ouml;&szlig;e ein bis drei ' +
        'Sekunden. W&auml;hrend einer Fahrt wird nichts &uuml;bertragen, damit die ' +
        'Steuerbefehle Vorrang haben. L&auml;sst sich auch an der Uhr im Men&uuml; ' +
        'ein- und ausschalten.'
    },
    {
      type: 'toggle',
      messageKey: 'cfgPreview',
      label: 'Vorschau einschalten',
      defaultValue: false
    },
    {
      type: 'select',
      messageKey: 'cfgPreviewSize',
      label: 'Bildgr&ouml;&szlig;e',
      defaultValue: '1',
      options: [
        { label: 'Klein - 64 Punkte breit, rund 1 s', value: '0' },
        { label: 'Mittel - 96 Punkte breit, rund 2 s', value: '1' },
        { label: 'Gross - 128 Punkte breit, rund 3 s', value: '2' }
      ]
    }
  ]
});

// Ein Block je Kamera. Leer gelassene Bl&ouml;cke tauchen auf der Uhr nicht auf.
for (var i = 1; i <= CAMERA_SLOTS; i++) {
  config.push({
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Kamera ' + i },
      {
        type: 'input',
        messageKey: 'cam' + i + 'Name',
        label: 'Name',
        defaultValue: i === 1 ? 'Kamera 1' : '',
        attributes: { placeholder: 'z. B. B&uuml;hne links', maxlength: 24 }
      },
      {
        type: 'input',
        messageKey: 'cam' + i + 'Host',
        label: 'IP-Adresse',
        defaultValue: '',
        attributes: { placeholder: '192.168.0.10' }
      },
      {
        type: 'input',
        messageKey: 'cam' + i + 'Port',
        label: 'Port',
        defaultValue: '80',
        attributes: { placeholder: '80', type: 'number' }
      },
      {
        type: 'input',
        messageKey: 'cam' + i + 'User',
        label: 'Benutzer (optional)',
        defaultValue: '',
        attributes: { placeholder: 'admin' }
      },
      {
        type: 'input',
        messageKey: 'cam' + i + 'Pass',
        label: 'Passwort (optional)',
        defaultValue: '',
        attributes: { type: 'password' }
      }
    ]
  });
}

config.push({ type: 'submit', defaultValue: 'Speichern' });

module.exports = config;
