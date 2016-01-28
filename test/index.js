'use strict';
var spawn = require('child_process').spawn;
var path = require('path');
var assert = require('assert');

var request = require('../').request;

var echoServerPath = path.join(__dirname, 'echo-server');
var DEFAULT_UA = 'http-multi-sync/' + require('../package.json').version;

var SESSION_ID="d13bda3a-83b2-4faf-adcb-3805dd90a842";

function mkReq(path, options) {
  options = options || {};
  options.path = path;
  options.host = options.host || 'api.snapnet.it';
  options.port = options.port || 9898;
  options.headers = {};
  options.headers['User-Agent']  = DEFAULT_UA;
  options.headers['X-BB-SESSION'] = SESSION_ID;
  console.log("HEADERS:"+ JSON.stringify(options.headers));
  var res = request(options).end();
  try {
    res.echo = JSON.parse(res.body.toString());
  } catch (err) {
    err.message = err.message + ' while parsing ' + res.body.toString();
    throw err;
  }
  return res;
}

var tests = [
  function simpleMulti() {
    console.log("sample");
    var options = {};
    options.copyname= "file";
    options.file= "README.md";
    var res = mkReq('/file', options);
    assert.equal(res.echo.url, '/file');
    assert.equal(res.echo.headers.accept, '*/*');
  }
  ];

function runTests() { // ad-hoc reinvention of tape
  console.log('1..%d', tests.length);
  // simple GET request
  var failed = tests.filter(function(t, idx) {
    try {
      t();
      console.log('ok %d - %s', idx + 1, t.name);
      return false;
    } catch (err) {
      console.log('not ok %d - %s', idx + 1, t.name);
      console.log('  ' + err.stack.split('\n').join('\n  '));
      return true;
    }
  });

  process.exit(failed.length ? 1 : 0);
}

runTests();