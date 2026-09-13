/* Demo backend: a PUT with a stop time stops the running entry (one-request stop). */
global.localStorage = { _d: {}, getItem: function (k) { return this._d[k] || null; }, setItem: function (k, v) { this._d[k] = String(v); }, removeItem: function (k) { delete this._d[k]; } };
var toggl = require('../src/pkjs/toggl.js');
var api = toggl.createClient('demo');
api.start(1, 101, 'Demo entry', function (err, e) {
  if (err) { throw err; }
  var startMs = Date.parse(e.start);
  api.update(1, e.id, { stop: new Date(startMs + 900000).toISOString(), duration: 900 }, function (err2, u) {
    if (err2) { throw err2; }
    if (u.duration !== 900 || !u.stop) { throw new Error('update did not apply'); }
    api.current(function (err3, cur) {
      if (cur !== null) { throw new Error('demo entry still running after PUT with stop'); }
      api.recentEntries(30, function (err4, list) {
        var found = list.filter(function (x) { return x.id === e.id; })[0];
        if (!found || found.duration !== 900) { throw new Error('stopped entry missing from list'); }
        console.log('demo smoke test: OK');
      });
    });
  });
});
