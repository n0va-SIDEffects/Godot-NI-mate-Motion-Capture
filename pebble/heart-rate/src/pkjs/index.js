/*
 * Phone-side glue. Clay renders the settings page from config.js and delivers the result to the
 * watch over AppMessage, so nothing else is needed here.
 *
 * Clay is vendored as plain JavaScript in vendor/clay.js rather than installed as a Pebble
 * package: the published package declares support only for the older platforms and would fail the
 * build for flint and gabbro, while the JavaScript half works everywhere.
 */
var Clay = require('./vendor/clay');
var clayConfig = require('./config');

// eslint-disable-next-line no-unused-vars
var clay = new Clay(clayConfig);
