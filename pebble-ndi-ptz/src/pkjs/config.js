/*
 * Clay configuration page (shown in the Pebble phone app under "Settings").
 */
module.exports = [
  {
    type: 'heading',
    defaultValue: 'NDI PTZ Remote'
  },
  {
    type: 'text',
    defaultValue: 'Die Uhr steuert die Kameras über die NDI-PTZ-Bridge ' +
      '(bridge/ndi_ptz_bridge.py) in deinem Netzwerk. Handy und Bridge ' +
      'müssen im gleichen WLAN sein.'
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Bridge' },
      {
        type: 'input',
        messageKey: 'CFG_HOST',
        label: 'Host / IP der Bridge',
        defaultValue: '',
        attributes: { placeholder: 'z.B. 192.168.1.50', type: 'text' }
      },
      {
        type: 'input',
        messageKey: 'CFG_PORT',
        label: 'Port',
        defaultValue: '8765',
        attributes: { type: 'number', min: 1, max: 65535 }
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Steuerung' },
      {
        type: 'select',
        messageKey: 'CFG_SPEED',
        label: 'Standard-Tempo',
        defaultValue: '1',
        options: [
          { label: 'Langsam', value: '0' },
          { label: 'Mittel', value: '1' },
          { label: 'Schnell', value: '2' }
        ]
      },
      {
        type: 'toggle',
        messageKey: 'CFG_INVERT_PAN',
        label: 'Pan umkehren',
        defaultValue: false
      },
      {
        type: 'toggle',
        messageKey: 'CFG_INVERT_TILT',
        label: 'Tilt umkehren',
        defaultValue: false
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Motion-Steuerung (Handgelenk)' },
      {
        type: 'slider',
        messageKey: 'CFG_SENSITIVITY',
        label: 'Empfindlichkeit',
        description: '1 = grosse Handbewegung nötig, 10 = sehr feinfühlig',
        defaultValue: 5,
        min: 1,
        max: 10,
        step: 1
      },
      {
        type: 'slider',
        messageKey: 'CFG_DEADZONE',
        label: 'Totzone (%)',
        description: 'Bewegungen unterhalb dieser Schwelle werden ignoriert',
        defaultValue: 8,
        min: 0,
        max: 30,
        step: 1
      }
    ]
  },
  {
    type: 'submit',
    defaultValue: 'Speichern'
  }
];
