# Phone-side JavaScript (PebbleKit JS)

`src/pkjs/index.js` is bundled into the `.pbw` and run by the phone app while
the watchapp is open. With `"enableMultiJS": true` you can `require('./file')`
other modules in `src/pkjs/`. Write ES5: `var`, `function`, callbacks. Older
iOS/Android runtimes lack `let`, arrows, template strings, Promises and even
`btoa` in places.

## Events

```js
var keys = require('message_keys');          // { CMD: 10000, TEXT: 10001, ... }

Pebble.addEventListener('ready', function () { /* watchapp opened, JS booted */ });
Pebble.addEventListener('appmessage', function (e) {
  var p = e.payload;                         // keys may be numeric ids or names
  var cmd = p[keys.CMD] !== undefined ? p[keys.CMD] : p.CMD;
});
Pebble.addEventListener('showConfiguration', function () { Pebble.openURL(url); });
Pebble.addEventListener('webviewclosed', function (e) { /* e.response */ });
```

Other useful calls: `Pebble.getActiveWatchInfo()` (platform, firmware),
`Pebble.getTimelineToken(ok, fail)`, `Pebble.showSimpleNotificationOnPebble`.

## Serialised send queue

`Pebble.sendAppMessage` fails if another message is in flight. Always queue:

```js
var queue = [], sending = false;
function enqueue(dict) { queue.push({ dict: dict, tries: 0 }); pump(); }
function pump() {
  if (sending || !queue.length) return;
  var item = queue[0]; sending = true;
  Pebble.sendAppMessage(item.dict, function () {
    sending = false; queue.shift(); pump();
  }, function () {
    sending = false;
    if (++item.tries >= 2) queue.shift();     // give up after one retry
    setTimeout(pump, 200);
  });
}
function msg(cmd, fields) {                    // build dict with numeric keys
  var d = {}; d[keys.CMD] = cmd;
  Object.keys(fields || {}).forEach(function (k) {
    if (fields[k] !== null && fields[k] !== undefined) d[keys[k]] = fields[k];
  });
  return d;
}
```

Numbers become `int32` on the watch, strings become `cstring`, arrays of
numbers become byte arrays. Clip strings on UTF-8 byte boundaries to the C
buffer size minus one.

## HTTP

```js
function request(method, url, headers, body, cb) {
  var xhr = new XMLHttpRequest(), done = false;
  function finish(err, data) { if (!done) { done = true; cb(err, data); } }
  xhr.open(method, url, true);
  Object.keys(headers || {}).forEach(function (h) { xhr.setRequestHeader(h, headers[h]); });
  try { xhr.timeout = 15000; } catch (e) {}
  xhr.onload = function () {
    if (xhr.status >= 200 && xhr.status < 300) {
      try { finish(null, xhr.responseText ? JSON.parse(xhr.responseText) : null); }
      catch (e) { finish('bad JSON'); }
    } else { finish('HTTP ' + xhr.status); }
  };
  xhr.onerror = function () { finish('no connection'); };
  xhr.ontimeout = function () { finish('timeout'); };
  xhr.send(body ? JSON.stringify(body) : null);
}
```

Basic auth needs base64 — include a small encoder rather than relying on
`btoa`. Keep API tokens in `localStorage`, never in the C code.

## Settings page without hosting

Build the HTML in JS, open it as a `data:` URL, and let the page navigate to
`pebblejs://close#<encoded JSON>` on save:

```js
function configUrl(settings) {
  var html = '<!DOCTYPE html><html><head><meta charset="utf-8">' +
    '<meta name="viewport" content="width=device-width,initial-scale=1"></head><body>' +
    '<form id="f"><label>Token <input id="t" value="' + escapeHtml(settings.token) + '"></label>' +
    '<button>Speichern</button></form><script>' +
    'document.getElementById("f").onsubmit=function(e){e.preventDefault();' +
    'location="pebblejs://close#"+encodeURIComponent(JSON.stringify({token:document.getElementById("t").value}));};' +
    '</script></body></html>';
  return 'data:text/html;charset=utf-8,' + encodeURIComponent(html);
}
Pebble.addEventListener('webviewclosed', function (e) {
  if (!e.response || e.response === 'CANCELLED') return;
  var text = e.response; try { text = decodeURIComponent(text); } catch (err) {}
  var cfg = JSON.parse(text);
  localStorage.setItem('settings', JSON.stringify(cfg));
});
```

Requires `"capabilities": ["configurable"]` in package.json. The older
`pebble-clay` package does the same with a JSON schema but adds an npm
dependency to the build.

## Caching for instant start

The watch shows nothing until JS answers. Store the last result in
`localStorage` and send it in `ready` before the network round-trip, then
send fresh data only if it differs (compare `JSON.stringify`). Persist the
last status on the watch too (`persist_write_data`).

## Testing without a phone

`assets/pkjs-mock.js` installs fake `Pebble`, `localStorage` and
`XMLHttpRequest` globals in Node and maps `require('message_keys')` to the
keys from `package.json`. Pattern:

```js
var mock = require('<skill>/assets/pkjs-mock.js');
var rt = mock.install({ packageJson: 'package.json', fetch: function (m, url, body) {
  return [200, {...}];                                   // fake server
}});
require('./src/pkjs/index.js');
rt.fire('ready');
setTimeout(function () {
  assert(rt.sent.some(function (d) { return d.CMD === 10; }));
  rt.fromWatch({ CMD: 1 });
}, 50);
```

Run with `node tests/smoke.js`. This catches protocol typos, wrong key names
and broken callback chains long before an emulator run.
