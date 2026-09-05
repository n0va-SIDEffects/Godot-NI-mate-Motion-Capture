/*
 * Clay configuration page (shown in the Pebble phone app under the gear icon).
 * The messageKeys used here must exist in package.json -> pebble.messageKeys.
 */
module.exports = [
  {
    type: 'heading',
    defaultValue: 'HELO Remote'
  },
  {
    type: 'text',
    defaultValue: 'Verbindung zum AJA HELO. Das Telefon muss im selben Netzwerk wie der HELO sein.'
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'HELO'
      },
      {
        type: 'input',
        messageKey: 'HELO_HOST',
        label: 'IP-Adresse oder Hostname',
        defaultValue: '',
        attributes: {
          placeholder: 'z.B. 192.168.1.50',
          autocapitalize: 'off',
          autocorrect: 'off'
        }
      },
      {
        type: 'input',
        messageKey: 'HELO_PORT',
        label: 'HTTP-Port',
        defaultValue: '80',
        attributes: {
          type: 'number',
          min: 1,
          max: 65535
        }
      },
      {
        type: 'input',
        messageKey: 'HELO_PASSWORD',
        label: 'Passwort (nur falls Benutzer-Auth am HELO aktiv ist)',
        defaultValue: '',
        attributes: {
          type: 'password',
          placeholder: 'leer lassen, wenn keine Auth'
        }
      }
    ]
  },
  {
    type: 'section',
    items: [
      {
        type: 'heading',
        defaultValue: 'Monitoring'
      },
      {
        type: 'slider',
        messageKey: 'POLL_INTERVAL',
        label: 'Abfrage-Intervall (Sekunden)',
        defaultValue: 3,
        min: 1,
        max: 15,
        step: 1
      },
      {
        type: 'toggle',
        messageKey: 'VIBRATE',
        label: 'Vibrieren bei Start/Stopp',
        defaultValue: true
      }
    ]
  },
  {
    type: 'text',
    defaultValue: 'Tasten: OBEN = Aufnahme, MITTE = Aktualisieren, UNTEN = Stream. ' +
      'Stoppen muss durch einen zweiten Druck innerhalb von 4 Sekunden bestaetigt werden.'
  },
  {
    type: 'submit',
    defaultValue: 'Speichern'
  }
];
