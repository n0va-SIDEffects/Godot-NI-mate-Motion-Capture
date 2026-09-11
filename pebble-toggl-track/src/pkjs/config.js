/*
 * Settings page for the phone app. Built as a data: URL so nothing has to be
 * hosted; on save it navigates to pebblejs://close#<json> which triggers the
 * "webviewclosed" event in index.js.
 */

var strings = require('./strings');

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

function projectOptions(projects, selectedId, noProjectLabel) {
  var html = '<option value="0"' + (!selectedId ? ' selected' : '') + '>' + escapeHtml(noProjectLabel) + '</option>';
  (projects || []).forEach(function (p) {
    html += '<option value="' + p.id + '"' + (p.id === selectedId ? ' selected' : '') + '>' + escapeHtml(p.name) + '</option>';
  });
  return html;
}

var SUPPORT_URL = 'https://buymeacoffee.com/SIDEffects';

function buildConfigUrl(settings, projects, diag) {
  var t = strings.t;
  diag = diag || {};
  settings = normalise(settings);
  var favHtml = '';
  for (var i = 0; i < MAX_FAVORITES; i++) {
    var f = settings.favorites[i] || { description: '', projectId: 0 };
    favHtml += '<div class="fav"><input id="fd' + i + '" type="text" placeholder="' + escapeHtml(t('favoritePlaceholder', i + 1)) + '" value="' + escapeHtml(f.description) + '">' +
      '<select id="fp' + i + '">' + projectOptions(projects, f.projectId, t('noProject')) + '</select></div>';
  }
  var noProjects = !projects || projects.length === 0;
  var roundOptions = [0, 5, 15, 30].map(function (m) {
    return '<option value="' + m + '"' + (settings.roundMinutes === m ? ' selected' : '') + '>' +
      (m === 0 ? escapeHtml(t('roundOff')) : escapeHtml(t('minutes', m))) + '</option>';
  }).join('');

  var html = '' +
    '<!DOCTYPE html><html><head><meta charset="utf-8">' +
    '<meta name="viewport" content="width=device-width,initial-scale=1">' +
    '<title>' + escapeHtml(t('title')) + '</title>' +
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
    'button.coffee{background:#ffdd00;color:#000;margin-top:10px}' +
    'a{color:#e57cd8}.hint{font-size:13px;color:#777;margin-top:6px}' +
    '.support{margin-top:30px;padding:16px;background:#fff;border-radius:10px;border:1px solid #e5e5e5}' +
    '.diag{margin-top:20px;color:#777;font-size:12px}.diag pre{white-space:pre-wrap;word-break:break-all;background:#fff;padding:10px;border-radius:8px}' +
    '</style></head><body>' +
    '<h1>' + escapeHtml(t('title')) + '</h1>' +
    '<p>' + escapeHtml(t('tagline')) + '</p>' +
    '<form id="f">' +
    '<h2>' + escapeHtml(t('account')) + '</h2>' +
    '<label for="token">' + escapeHtml(t('token')) + '</label>' +
    '<input id="token" type="text" autocapitalize="off" autocorrect="off" spellcheck="false" ' +
    'placeholder="' + escapeHtml(t('tokenPlaceholder')) + '" value="' + escapeHtml(settings.token) + '">' +
    '<div class="hint">' + t('tokenHint') + '</div>' +
    '<label for="wid">' + escapeHtml(t('workspace')) + '</label>' +
    '<input id="wid" type="text" inputmode="numeric" placeholder="' + escapeHtml(t('workspacePlaceholder')) + '" value="' + escapeHtml(settings.workspaceId) + '">' +
    '<h2>' + escapeHtml(t('favorites')) + '</h2>' +
    '<p>' + escapeHtml(t('favoritesText')) + '</p>' +
    (noProjects ? '<p class="hint">' + escapeHtml(t('favoritesNoProjects')) + '</p>' : '') +
    favHtml +
    '<h2>' + escapeHtml(t('reminders')) + '</h2>' +
    '<label class="row"><input id="rr" type="checkbox"' + (settings.remind.running ? ' checked' : '') + '> ' + escapeHtml(t('remindRunning')) + '</label>' +
    '<div class="fav"><label>' + escapeHtml(t('afterHours')) + '<input id="rh" type="number" min="1" max="16" value="' + settings.remind.maxHours + '"></label>' +
    '<label>' + escapeHtml(t('orAtHour')) + '<input id="rl" type="number" min="0" max="23" value="' + settings.remind.lateHour + '"></label></div>' +
    '<label class="row"><input id="rn" type="checkbox"' + (settings.remind.noTimer ? ' checked' : '') + '> ' + escapeHtml(t('remindNoTimer')) + '</label>' +
    '<label>' + escapeHtml(t('fromHour')) + '<input id="rs" type="number" min="0" max="23" value="' + settings.remind.startHour + '"></label>' +
    '<div class="hint">' + escapeHtml(t('remindHint')) + '</div>' +
    '<h2>' + escapeHtml(t('rounding')) + '</h2>' +
    '<label for="round">' + escapeHtml(t('roundLabel')) + '</label>' +
    '<select id="round">' + roundOptions + '</select>' +
    '<button type="submit">' + escapeHtml(t('save')) + '</button>' +
    '</form>' +
    '<div class="support"><h2 style="margin-top:0">' + escapeHtml(t('support')) + '</h2>' +
    '<p>' + escapeHtml(t('supportText')) + ' <a href="' + SUPPORT_URL + '" target="_blank">' + SUPPORT_URL.replace('https://', '') + '</a></p>' +
    '<button type="button" class="coffee" id="coffee">' + escapeHtml(t('coffee')) + '</button></div>' +
    '<details class="diag"><summary>Diagnose / Diagnostics</summary><pre>' +
    escapeHtml(['language: ' + strings.current(), 'projects: ' + (projects ? projects.length : 0) + ' cached']
      .concat(Object.keys(diag).map(function (k) { return k + ': ' + diag[k]; })).join('\n')) +
    '</pre></details>' +
    '<script>' +
    'function v(id){return document.getElementById(id).value;}' +
    'function c(id){return document.getElementById(id).checked;}' +
    'document.getElementById("coffee").addEventListener("click",function(){var w=null;' +
    'try{w=window.open("' + SUPPORT_URL + '","_blank");}catch(e){}if(!w){window.location.href="' + SUPPORT_URL + '";}});' +
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
  DEFAULTS: DEFAULTS,
  SUPPORT_URL: SUPPORT_URL
};
