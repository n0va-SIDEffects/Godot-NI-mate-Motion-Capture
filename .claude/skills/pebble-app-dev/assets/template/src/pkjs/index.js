/* Phone side starter: answers CMD_HELLO with a text, offers a settings page. */
var keys = require('message_keys');

var CMD = { HELLO: 1, TEXT: 10 };
var settings = load('settings') || { name: '' };

function load(k) { try { return JSON.parse(localStorage.getItem(k)); } catch (e) { return null; } }
function save(k, v) { try { localStorage.setItem(k, JSON.stringify(v)); } catch (e) {} }

var queue = [], sending = false;
function enqueue(dict) { queue.push(dict); pump(); }
function pump() {
  if (sending || !queue.length) { return; }
  sending = true;
  Pebble.sendAppMessage(queue[0], function () { sending = false; queue.shift(); pump(); },
    function () { sending = false; queue.shift(); setTimeout(pump, 200); });
}

function sendText(text) {
  var d = {};
  d[keys.CMD] = CMD.TEXT;
  d[keys.TEXT] = String(text).slice(0, 60);
  enqueue(d);
}

function escapeHtml(s) {
  return String(s || '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/"/g, '&quot;');
}

function configUrl() {
  var html = '<!DOCTYPE html><html><head><meta charset="utf-8">' +
    '<meta name="viewport" content="width=device-width,initial-scale=1">' +
    '<style>body{font-family:sans-serif;padding:20px}input,button{width:100%;font-size:16px;padding:10px;margin:8px 0}</style>' +
    '</head><body><h2>Einstellungen</h2><form id="f"><label>Name<input id="n" value="' + escapeHtml(settings.name) + '"></label>' +
    '<button>Speichern</button></form><script>' +
    'document.getElementById("f").onsubmit=function(e){e.preventDefault();' +
    'location="pebblejs://close#"+encodeURIComponent(JSON.stringify({name:document.getElementById("n").value}));};' +
    '</script></body></html>';
  return 'data:text/html;charset=utf-8,' + encodeURIComponent(html);
}

Pebble.addEventListener('ready', function () {
  sendText(settings.name ? 'Hallo ' + settings.name : 'Bereit');
});

Pebble.addEventListener('appmessage', function (e) {
  var p = e.payload || {};
  var cmd = Number(p[keys.CMD] !== undefined ? p[keys.CMD] : p.CMD);
  if (cmd === CMD.HELLO) {
    sendText('Handy sagt: ' + new Date().toLocaleTimeString());
  }
});

Pebble.addEventListener('showConfiguration', function () { Pebble.openURL(configUrl()); });

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e.response || e.response === 'CANCELLED') { return; }
  var text = e.response;
  try { text = decodeURIComponent(text); } catch (err) {}
  try { settings = JSON.parse(text); } catch (err) { return; }
  save('settings', settings);
  sendText('Hallo ' + settings.name);
});
