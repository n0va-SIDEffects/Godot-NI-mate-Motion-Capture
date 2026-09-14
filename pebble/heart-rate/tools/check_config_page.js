/*
 * Builds the settings page the way the phone would and checks it can actually run.
 *
 * Clay turns the custom function into text and drops it into the page, so anything that needs a
 * module loader breaks the page silently: it opens and stays blank. This catches that, and checks
 * every messageKey on the page is declared in package.json.
 *
 *     node tools/check_config_page.js
 */
var path = require('path');
var fs = require('fs');
var os = require('os');

var root = path.join(__dirname, '..');

// The bundled Clay pulls in the message keys the build generates. Node resolves that from inside
// clay.js, so the stand-in has to be on NODE_PATH before Node starts: set it and start again.
var keysPath = path.join(root, 'build', 'js', 'message_keys.json');
if (!fs.existsSync(keysPath)) {
  console.error('build/js/message_keys.json is missing: run "pebble build" first');
  process.exit(2);
}
if (!process.env.PULSMONITOR_STUB) {
  var stub = fs.mkdtempSync(path.join(os.tmpdir(), 'pulsmonitor-'));
  fs.mkdirSync(path.join(stub, 'message_keys'));
  fs.writeFileSync(path.join(stub, 'message_keys', 'package.json'),
                   '{"name":"message_keys","main":"index.js"}');
  fs.writeFileSync(path.join(stub, 'message_keys', 'index.js'),
                   'module.exports = ' + fs.readFileSync(keysPath, 'utf8') + ';');
  var result = require('child_process').spawnSync(
      process.execPath, [__filename],
      {stdio: 'inherit', env: Object.assign({}, process.env,
                                            {NODE_PATH: stub, PULSMONITOR_STUB: stub})});
  fs.rmSync(stub, {recursive: true, force: true});
  process.exit(result.status === null ? 1 : result.status);
}

// Clay reaches for browser storage while building the page; it copes without, but stub it so the
// check's output stays clean.
global.localStorage = {getItem: function () { return null; }, setItem: function () {}};

global.Pebble = {
  addEventListener: function () {},
  getActiveWatchInfo: function () { return {platform: 'emery'}; },
  getAccountToken: function () { return 'account'; },
  getWatchToken: function () { return 'watch'; }
};

var failures = 0;
function check(ok, what, detail) {
  console.log((ok ? 'ok  ' : 'FAIL') + ' ' + what + (detail ? ': ' + detail : ''));
  if (!ok) {
    failures++;
  }
}

var Clay = require(path.join(root, 'src/pkjs/vendor/clay.js'));
var config = require(path.join(root, 'src/pkjs/config.js'));
var custom = require(path.join(root, 'src/pkjs/custom-clay.js'));
var declared = require(path.join(root, 'package.json')).pebble.messageKeys;

var clay = new Clay(config, custom, {autoHandleEvents: false});
var page = decodeURIComponent(clay.generateUrl().replace(/^data:text\/html;charset=utf-8,/, ''));

var leftovers = page.match(/require\([^)]*\)/g);
check(!leftovers, 'the page carries no require() call',
      leftovers ? leftovers.join(', ') : 'none');

var keys = [];
var ids = [];
(function walk(items) {
  items.forEach(function (item) {
    if (item.messageKey) keys.push(item.messageKey);
    if (item.id) ids.push(item.id);
    if (item.items) walk(item.items);
  });
})(config);

var missing = keys.filter(function (k) { return declared.indexOf(k) < 0; });
var unused = declared.filter(function (k) { return keys.indexOf(k) < 0; });
check(missing.length === 0, 'every key on the page is declared in package.json',
      missing.join(', ') || 'none missing');
check(unused.length === 0, 'every declared key appears on the page',
      unused.join(', ') || 'none unused');
check(ids.indexOf('donate') >= 0 && page.indexOf('buymeacoffee.com') >= 0,
      'the donation button and its address are in the page');
check(page.length < 1500000, 'the page fits in a data URL', page.length + ' characters');

console.log('');
console.log(failures ? 'FAILURES' : 'all checks passed');
process.exit(failures ? 1 : 0);
