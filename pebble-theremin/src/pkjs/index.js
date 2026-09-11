// Telefonseite der Theremin-App: Konfigurationsseite ueber Clay und
// Abgleich der Einstellungen mit der Uhr. Die Seite erscheint auf Deutsch,
// wenn das Handy auf Deutsch steht, sonst auf Englisch.
var Clay = require('pebble-clay');
var buildConfig = require('./config');
var customClay = require('./custom-clay');

var phoneLang = (typeof navigator !== 'undefined' && navigator.language) ? navigator.language : 'en';
var lang = /^de/i.test(phoneLang) ? 'de' : 'en';
var clay = new Clay(buildConfig(lang), customClay);

var TOGGLE_KEYS = { InvertPitch: true, InvertVol: true, Gate: true, Backlight: true };

Pebble.addEventListener('appmessage', function(e) {
  var payload = e.payload || {};
  var settings = {};
  Object.keys(payload).forEach(function(key) {
    settings[key] = TOGGLE_KEYS[key] ? !!payload[key] : String(payload[key]);
  });
  clay.setSettings(settings);
});

Pebble.addEventListener('ready', function() {
  console.log('Theremin PKJS bereit / ready (' + lang + ')');
});
