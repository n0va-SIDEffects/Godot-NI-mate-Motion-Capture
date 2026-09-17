/*
 * The settings page shown by the Pebble phone app.
 */
var DONATION_URL = 'https://buymeacoffee.com/SIDEffects';

var config = [
  {
    type: 'heading',
    defaultValue: 'Pulsmonitor'
  },
  {
    type: 'text',
    defaultValue: 'Herzschlag als Kurve, Ton und Vibration. Die Tasten der Uhr schalten Vibration (Auswahl) und Ton (oben) auch direkt um.'
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Ton' },
      {
        type: 'toggle',
        messageKey: 'SOUND_ON',
        label: 'Piep bei jedem Schlag',
        description: 'Nur Uhren mit Lautsprecher. Ist die Uhr stummgeschaltet, bleibt es still.',
        defaultValue: true
      },
      {
        type: 'slider',
        messageKey: 'VOLUME',
        label: 'Lautstärke',
        description: 'Über etwa 70 verzerrt der kleine Lautsprecher hörbar.',
        defaultValue: 65,
        min: 0,
        max: 100,
        step: 5
      },
      {
        type: 'select',
        messageKey: 'PITCH',
        label: 'Tonhöhe',
        defaultValue: 81,
        options: [
          { label: 'Tief (660 Hz)', value: 76 },
          { label: 'Monitor (880 Hz)', value: 81 },
          { label: 'Hoch (1046 Hz)', value: 84 }
        ]
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Vibration' },
      {
        type: 'toggle',
        messageKey: 'VIBE_ON',
        label: 'Klick bei jedem Schlag',
        description: 'Der Motor klickt hörbar mit. Für einen reinen Monitor-Ton hier ausschalten.',
        defaultValue: true
      },
      {
        type: 'select',
        messageKey: 'VIBE_MS',
        label: 'Länge',
        defaultValue: 25,
        options: [
          { label: 'Kurz', value: 15 },
          { label: 'Normal', value: 25 },
          { label: 'Kräftig', value: 40 }
        ]
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Anzeige' },
      {
        type: 'select',
        messageKey: 'BACKLIGHT',
        label: 'Beleuchtung',
        description: 'Dauerhaft an zieht spürbar Akku und ist für kurzes Zuschauen gedacht.',
        defaultValue: 0,
        options: [
          { label: 'Wie sonst auch', value: 0 },
          { label: 'Bei jedem Schlag kurz', value: 1 },
          { label: 'Gedimmt pulsierend', value: 3 },
          { label: 'Dauerhaft an', value: 2 }
        ]
      },
      {
        type: 'color',
        messageKey: 'LIGHT_COLOR',
        label: 'Farbe der Beleuchtung',
        description: 'Nur Uhren mit farbiger Beleuchtung, also die Pebble Time 2.',
        defaultValue: 0xffffff,
        allowGray: true
      },
      {
        type: 'slider',
        messageKey: 'LIGHT_FLOOR',
        label: 'Grundhelligkeit beim Pulsieren',
        description: 'Wie hell die Beleuchtung zwischen zwei Schlägen stehen bleibt. Bei 0 geht sie ganz aus.',
        defaultValue: 15,
        min: 0,
        max: 100,
        step: 5
      },
      {
        type: 'select',
        messageKey: 'SWEEP_MS',
        label: 'Kurvengeschwindigkeit',
        defaultValue: 20,
        options: [
          { label: 'Langsam', value: 40 },
          { label: 'Normal', value: 20 },
          { label: 'Schnell', value: 13 }
        ]
      },
      {
        type: 'select',
        messageKey: 'TRACE_COLOR',
        label: 'Kurvenfarbe',
        description: 'Uhren ohne Farbdisplay zeichnen immer weiß.',
        defaultValue: 0,
        options: [
          { label: 'Grün', value: 0 },
          { label: 'Rot', value: 1 },
          { label: 'Weiß', value: 2 },
          { label: 'Gelb', value: 3 },
          { label: 'Türkis', value: 4 }
        ]
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Demo' },
      {
        type: 'toggle',
        messageKey: 'DEMO',
        label: 'Puls simulieren',
        description: 'Erzeugt einen Puls ohne Sensor, etwa zum Vorführen. Auf der Uhr schaltet ein langer Druck auf die untere Taste dasselbe um.',
        defaultValue: false
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Speichern'
  }
];

if (DONATION_URL) {
  config.push({
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Unterstützen' },
      {
        type: 'text',
        defaultValue: 'Die App ist kostenlos und ohne Werbung. Wenn sie dir Freude macht, freue ' +
                      'ich mich über einen Kaffee: <a href="' + DONATION_URL +
                      '" target="_blank">' + DONATION_URL.replace(/^https:\/\//, '') + '</a>'
      },
      { type: 'button', id: 'donate', primary: true, defaultValue: '☕ Buy me a coffee' }
    ]
  });
}

module.exports = config;
