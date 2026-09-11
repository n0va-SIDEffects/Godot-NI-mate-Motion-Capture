// Telefonseite der Theremin-App: Konfigurationsseite ueber Clay und
// Abgleich der Einstellungen mit der Uhr.
var Clay = require('pebble-clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig);

// Auswahlfelder erwarten Textwerte, Schalter Wahrheitswerte.
var TOGGLE_KEYS = { InvertPitch: true, InvertVol: true, Gate: true };

// Die Uhr schickt beim Start (und nach Aenderungen im Uhr-Menue) ihre
// aktuellen Einstellungen. Damit zeigt die Konfigurationsseite den echten Stand.
Pebble.addEventListener('appmessage', function(e) {
  var payload = e.payload || {};
  var settings = {};
  Object.keys(payload).forEach(function(key) {
    settings[key] = TOGGLE_KEYS[key] ? !!payload[key] : String(payload[key]);
  });
  clay.setSettings(settings);
});

Pebble.addEventListener('ready', function() {
  console.log('Theremin PKJS bereit');
});
