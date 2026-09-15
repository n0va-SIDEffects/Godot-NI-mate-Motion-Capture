/*
 * Phone-side glue.
 *
 * Clay renders the settings page and sends the result to the watch, but only at the moment the
 * page is closed. Save the settings while the watchapp is not running and that message goes
 * nowhere: nothing is queued, and the watch never learns about the change. So the watch asks for
 * the settings when it starts, and this answers with whatever Clay last stored.
 *
 * Clay is vendored as plain JavaScript in vendor/clay.js rather than installed as a Pebble
 * package: the published package declares support only for the older platforms and would fail the
 * build for flint and gabbro, while the JavaScript half works everywhere.
 */
var Clay = require('./vendor/clay');
var clayConfig = require('./config');

var clay = new Clay(clayConfig, require('./custom-clay'));

//! Clay keeps the saved settings as plain values. AppMessage carries numbers, so booleans become
//! 0 and 1, and anything numeric that arrived as text becomes a number again.
function forTheWatch(settings) {
  var message = {};
  Object.keys(settings).forEach(function (key) {
    var value = settings[key];
    if (typeof value === 'boolean') {
      value = value ? 1 : 0;
    } else if (typeof value === 'string' && value !== '' && !isNaN(Number(value))) {
      value = Number(value);
    }
    if (typeof value === 'number') {
      message[key] = value;
    }
  });
  return message;
}

function storedSettings() {
  try {
    return JSON.parse(localStorage.getItem('clay-settings')) || {};
  } catch (e) {
    return {};
  }
}

Pebble.addEventListener('appmessage', function () {
  // The watch only ever sends one thing: "tell me the settings".
  console.log('The watch asked for the settings');
  var message = forTheWatch(storedSettings());
  if (Object.keys(message).length === 0) {
    return;   // nothing saved yet, so the watch keeps its own defaults
  }
  Pebble.sendAppMessage(message, function () {
    console.log('Sent stored settings to the watch');
  }, function (e) {
    console.log('Could not send stored settings: ' + JSON.stringify(e));
  });
});
