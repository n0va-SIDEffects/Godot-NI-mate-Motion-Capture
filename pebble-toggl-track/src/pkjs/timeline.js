/*
 * Timeline pin for the running entry (Rebble/Core timeline web API).
 * One pin id is reused, so updating replaces and stopping deletes it.
 * The pin carries an "openWatchApp" action with a launch code the watch
 * treats as "stop the timer".
 */

var API = 'https://timeline-api.rebble.io/v1/user/pins/';
var PIN_ID = 'toggl-timer-running';
var LAUNCH_STOP = 3;        // must match LAUNCH_STOP in src/c/main.c
var LAUNCH_OPEN = 0;

var tokenCache = null;

function getToken(cb) {
  if (tokenCache) { return cb(null, tokenCache); }
  try {
    Pebble.getTimelineToken(function (token) {
      tokenCache = token;
      cb(null, token);
    }, function (err) {
      cb('timeline token: ' + (err || 'unavailable'));
    });
  } catch (e) {
    cb('timeline token: ' + e);
  }
}

function request(method, body, cb) {
  getToken(function (err, token) {
    if (err) { return cb(err); }
    var xhr = new XMLHttpRequest();
    var done = false;
    var finish = function (e) { if (!done) { done = true; cb(e); } };
    xhr.open(method, API + PIN_ID, true);
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.setRequestHeader('X-User-Token', token);
    try { xhr.timeout = 15000; } catch (e) {}
    xhr.onload = function () { finish(xhr.status >= 200 && xhr.status < 300 ? null : 'timeline HTTP ' + xhr.status); };
    xhr.onerror = function () { finish('timeline: no connection'); };
    xhr.ontimeout = function () { finish('timeline: timeout'); };
    xhr.send(body ? JSON.stringify(body) : null);
  });
}

// status: the object built by statusFromEntry() in index.js
function pinFor(status, t) {
  var title = status.description || status.projectName || 'Toggl';
  var subtitle = status.projectName && status.description ? status.projectName : (t('pinRunning'));
  return {
    id: PIN_ID,
    time: new Date(status.start * 1000).toISOString(),
    layout: {
      type: 'genericPin',
      title: title,
      subtitle: subtitle,
      tinyIcon: 'system://images/TIMELINE_CALENDAR',
      body: t('pinBody')
    },
    actions: [
      { title: t('pinStop'), type: 'openWatchApp', launchCode: LAUNCH_STOP },
      { title: t('pinOpen'), type: 'openWatchApp', launchCode: LAUNCH_OPEN }
    ]
  };
}

function put(status, t, cb) { request('PUT', pinFor(status, t), cb); }
function remove(cb) { request('DELETE', null, cb); }

module.exports = { put: put, remove: remove, PIN_ID: PIN_ID, LAUNCH_STOP: LAUNCH_STOP };
