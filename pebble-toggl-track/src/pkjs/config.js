/*
 * Settings page for the phone app. Built as a data: URL so nothing has to be
 * hosted; on save it navigates to pebblejs://close#<json> which triggers the
 * "webviewclosed" event in index.js.
 */

var MAX_FAVORITES = 4;

var DEFAULTS = {
  token: '',
  workspaceId: '',
  favorites: [],                 // [{description, projectId}]
  remind: { running: true, maxHours: 4, lateHour: 22, noTimer: false, startHour: 9 },
  roundMinutes: 0
};

function clampInt(v, min, max, fallback) {
  var n = parseInt(v, 10);
  if (isNaN(n)) { return fallback; }
  return Math.max(min, Math.min(max, n));
}

// Fill in defaults and sanitise whatever came from storage or the page.
function normalise(cfg) {
  cfg = cfg || {};
  var r = cfg.remind || {};
  var favorites = (cfg.favorites || []).slice(0, MAX_FAVORITES).map(function (f) {
    return { description: String((f && f.description) || '').trim(), projectId: parseInt(f && f.projectId, 10) || 0 };
  });
  return {
    token: String(cfg.token || '').replace(/\s+/g, ''),
    workspaceId: String(cfg.workspaceId || '').replace(/\D+/g, ''),
    favorites: favorites,
    remind: {
      running: r.running !== false,
      maxHours: clampInt(r.maxHours, 1, 16, DEFAULTS.remind.maxHours),
      lateHour: clampInt(r.lateHour, 0, 23, DEFAULTS.remind.lateHour),
      noTimer: !!r.noTimer,
      startHour: clampInt(r.startHour, 0, 23, DEFAULTS.remind.startHour)
    },
    roundMinutes: clampInt(cfg.roundMinutes, 0, 60, 0)
  };
}

function escapeHtml(s) {
  return String(s || '')
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function projectOptions(projects, selectedId) {
  var html = '<option value="0"' + (!selectedId ? ' selected' : '') + '>Ohne Projekt</option>';
  (projects || []).forEach(function (p) {
    html += '<option value="' + p.id + '"' + (p.id === selectedId ? ' selected' : '') + '>' + escapeHtml(p.name) + '</option>';
  });
  return html;
}

function buildConfigUrl(settings, projects) {
  settings = normalise(settings);
  var favHtml = '';
  for (var i = 0; i < MAX_FAVORITES; i++) {
    var f = settings.favorites[i] || { description: '', projectId: 0 };
    favHtml += '<div class="fav"><input id="fd' + i + '" type="text" placeholder="Favorit ' + (i + 1) + ': Beschreibung" value="' + escapeHtml(f.description) + '">' +
      '<select id="fp' + i + '">' + projectOptions(projects, f.projectId) + '</select></div>';
  }
  var noProjects = !projects || projects.length === 0;

  var html = '' +
    '<!DOCTYPE html><html lang="de"><head><meta charset="utf-8">' +
    '<meta name="viewport" content="width=device-width,initial-scale=1">' +
    '<title>Toggl Track</title>' +
    '<style>' +
    'body{font-family:-apple-system,Helvetica,Arial,sans-serif;margin:0;padding:20px;background:#f4f4f6;color:#222}' +
    'h1{font-size:22px;margin:0 0 4px}h2{font-size:16px;margin:26px 0 8px;border-bottom:1px solid #ddd;padding-bottom:4px}' +
    'p{color:#555;font-size:14px;line-height:1.4;margin:6px 0}' +
    'label{display:block;font-weight:600;margin:14px 0 6px;font-size:14px}' +
    'label.row{display:flex;align-items:center;gap:10px;font-weight:500;margin:10px 0}' +
    'input[type=text],input[type=number],select{width:100%;box-sizing:border-box;font-size:16px;padding:11px;border:1px solid #ccc;border-radius:8px;background:#fff}' +
    'input[type=number]{width:90px}input[type=checkbox]{width:22px;height:22px}' +
    '.fav{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-bottom:8px}' +
    'button{width:100%;margin-top:24px;padding:14px;font-size:17px;font-weight:600;color:#fff;background:#e57cd8;border:0;border-radius:8px}' +
    'a{color:#e57cd8}.hint{font-size:13px;color:#777;margin-top:6px}' +
    '</style></head><body>' +
    '<h1>Toggl Track</h1>' +
    '<p>Timer direkt von der Pebble starten und stoppen.</p>' +
    '<form id="f">' +
    '<h2>Konto</h2>' +
    '<label for="token">API-Token</label>' +
    '<input id="token" type="text" autocapitalize="off" autocorrect="off" spellcheck="false" ' +
    'placeholder="32 Zeichen (oder „demo“ zum Ausprobieren)" value="' + escapeHtml(settings.token) + '">' +
    '<div class="hint">Zu finden unter <a href="https://track.toggl.com/profile" target="_blank">track.toggl.com/profile</a> ganz unten (&quot;API Token&quot;).</div>' +
    '<label for="wid">Workspace-ID (optional)</label>' +
    '<input id="wid" type="text" inputmode="numeric" placeholder="Standard-Workspace" value="' + escapeHtml(settings.workspaceId) + '">' +
    '<h2>Favoriten</h2>' +
    '<p>Bis zu vier Kacheln auf der Uhr: Beschreibung und Projekt. Ein Tipp startet den Eintrag.</p>' +
    (noProjects ? '<p class="hint">Projekte erscheinen hier, sobald die App einmal mit Toggl verbunden war. Erst Token speichern, dann Favoriten anlegen.</p>' : '') +
    favHtml +
    '<h2>Erinnerungen</h2>' +
    '<label class="row"><input id="rr" type="checkbox"' + (settings.remind.running ? ' checked' : '') + '> Vibrieren, wenn ein Timer vergessen wurde</label>' +
    '<div class="fav"><label>nach Stunden<input id="rh" type="number" min="1" max="16" value="' + settings.remind.maxHours + '"></label>' +
    '<label>oder ab Uhrzeit<input id="rl" type="number" min="0" max="23" value="' + settings.remind.lateHour + '"></label></div>' +
    '<label class="row"><input id="rn" type="checkbox"' + (settings.remind.noTimer ? ' checked' : '') + '> Werktags erinnern, wenn kein Timer läuft</label>' +
    '<label>ab Uhrzeit<input id="rs" type="number" min="0" max="23" value="' + settings.remind.startHour + '"></label>' +
    '<div class="hint">Die Uhr weckt die App zur Erinnerung auch, wenn sie geschlossen ist.</div>' +
    '<h2>Rundung</h2>' +
    '<label for="round">Beim Stoppen runden auf</label>' +
    '<select id="round">' +
    '<option value="0"' + (settings.roundMinutes === 0 ? ' selected' : '') + '>nicht runden</option>' +
    '<option value="5"' + (settings.roundMinutes === 5 ? ' selected' : '') + '>5 Minuten</option>' +
    '<option value="15"' + (settings.roundMinutes === 15 ? ' selected' : '') + '>15 Minuten</option>' +
    '<option value="30"' + (settings.roundMinutes === 30 ? ' selected' : '') + '>30 Minuten</option>' +
    '</select>' +
    '<button type="submit">Speichern</button>' +
    '</form>' +
    '<script>' +
    'function v(id){return document.getElementById(id).value;}' +
    'function c(id){return document.getElementById(id).checked;}' +
    'document.getElementById("f").addEventListener("submit",function(ev){ev.preventDefault();' +
    'var favs=[];for(var i=0;i<' + MAX_FAVORITES + ';i++){favs.push({description:v("fd"+i),projectId:parseInt(v("fp"+i),10)||0});}' +
    'var cfg={token:v("token").replace(/\\s+/g,""),workspaceId:v("wid").replace(/\\D+/g,""),favorites:favs,' +
    'remind:{running:c("rr"),maxHours:v("rh"),lateHour:v("rl"),noTimer:c("rn"),startHour:v("rs")},roundMinutes:v("round")};' +
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
    return normalise(JSON.parse(text));
  } catch (e) {
    console.log('Config response not parseable: ' + response);
    return null;
  }
}

module.exports = {
  buildConfigUrl: buildConfigUrl,
  parseConfigResponse: parseConfigResponse,
  normalise: normalise,
  DEFAULTS: DEFAULTS
};
