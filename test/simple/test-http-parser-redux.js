// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common');
var assert = require('assert');

var http_parser = require('http_parser');

var REQUEST  = http_parser.REQUEST;
var RESPONSE = http_parser.RESPONSE;

function expect(type, data, result) {
  var p = new http_parser.Parser(type);
  var b = new Buffer(data);
  var r = p.feed(b, 0, b.length);
  assert.equal(r, result);
}

function expect_ok(type, data) {
  expect(type, data, true);
}

function expect_fail(type, data) {
  expect(type, data, false);
}

expect_ok(REQUEST, 'GET / HTTP/1.0\r\n');
expect_fail(REQUEST, 'XXX / HTTP/1.0\r\n');
