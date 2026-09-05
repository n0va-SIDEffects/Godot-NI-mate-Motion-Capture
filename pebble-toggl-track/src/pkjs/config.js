/*
 * Settings page for the phone app. Built as a data: URL so nothing has to be
 * hosted; on save it navigates to pebblejs://close#<json> which triggers the
 * "webviewclosed" event in index.js.
 */

function escapeHtml(s) {
  return String(s || '')
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function buildConfigUrl(settings) {
  var html = '' +
    '<!DOCTYPE html><html lang="de"><head><meta charset="utf-8">' +
    '<meta name="viewport" content="width=device-width,initial-scale=1">' +
    '<title>Toggl Track</title>' +
    '<style>' +
    'body{font-family:-apple-system,Helvetica,Arial,sans-serif;margin:0;padding:20px;background:#f4f4f6;color:#222}' +
    'h1{font-size:22px;margin:0 0 4px}p{color:#555;font-size:14px;line-height:1.4}' +
    'label{display:block;font-weight:600;margin:18px 0 6px;font-size:14px}' +
    'input{width:100%;box-sizing:border-box;font-size:16px;padding:12px;border:1px solid #ccc;border-radius:8px;background:#fff}' +
    'button{width:100%;margin-top:24px;padding:14px;font-size:17px;font-weight:600;color:#fff;background:#e57cd8;border:0;border-radius:8px}' +
    'a{color:#e57cd8}.hint{font-size:13px;color:#777;margin-top:6px}' +
    '</style></head><body>' +
    '<h1>Toggl Track</h1>' +
    '<p>Timer direkt von der Pebble starten und stoppen.</p>' +
    '<form id="f">' +
    '<label for="token">API-Token</label>' +
    '<input id="token" type="text" autocapitalize="off" autocorrect="off" spellcheck="false" ' +
    'placeholder="32 Zeichen" value="' + escapeHtml(settings.token) + '">' +
    '<div class="hint">Zu finden unter <a href="https://track.toggl.com/profile" target="_blank">track.toggl.com/profile</a> ganz unten (&quot;API Token&quot;).</div>' +
    '<label for="wid">Workspace-ID (optional)</label>' +
    '<input id="wid" type="text" inputmode="numeric" placeholder="Standard-Workspace" value="' + escapeHtml(settings.workspaceId) + '">' +
    '<div class="hint">Leer lassen, dann wird dein Standard-Workspace verwendet.</div>' +
    '<button type="submit">Speichern</button>' +
    '</form>' +
    '<script>' +
    'document.getElementById("f").addEventListener("submit",function(ev){ev.preventDefault();' +
    'var cfg={token:document.getElementById("token").value.replace(/\\s+/g,""),' +
    'workspaceId:document.getElementById("wid").value.replace(/\\D+/g,"")};' +
    'document.location="pebblejs://close#"+encodeURIComponent(JSON.stringify(cfg));});' +
    '</script></body></html>';
  return 'data:text/html;charset=utf-8,' + encodeURIComponent(html);
}

// Parses the response of the "webviewclosed" event. Returns null on cancel.
function parseConfigResponse(response) {
  if (!response || response === 'CANCELLED') {
    return null;
  }
  var text = response;
  try {
    text = decodeURIComponent(response);
  } catch (e) { /* already decoded */ }
  try {
    var cfg = JSON.parse(text);
    return {
      token: String(cfg.token || '').replace(/\s+/g, ''),
      workspaceId: String(cfg.workspaceId || '').replace(/\D+/g, '')
    };
  } catch (e) {
    console.log('Config response not parseable: ' + response);
    return null;
  }
}

module.exports = {
  buildConfigUrl: buildConfigUrl,
  parseConfigResponse: parseConfigResponse
};
