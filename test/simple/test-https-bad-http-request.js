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

if (!process.versions.openssl) {
  console.error("Skipping because node compiled without OpenSSL.");
  process.exit(0);
}

var common = require('../common');
var assert = require('assert');
var https = require('https');
var tls = require('tls');
var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var connect_cb_called = 0;
var write_cb_called = 0;
var close_cb_called = 0;

var inputs = [
  // pure garbage
  "asdf",

  // bad request method
  "XXX /bad-method HTTP/1.0\r\n" +
  "X-Filler: 42\r\n" +
  "\r\n"
];

process.on('exit', function() {
  assert.equal(connect_cb_called, inputs.length);
  assert.equal(write_cb_called, inputs.length);
  assert.equal(close_cb_called, inputs.length);
});

run(0);

function run(index) {
  var req = inputs[index];

  if (!req)
    return;

  var server = https.createServer(options, function() {
    assert.ok(false); // should never run
  });

  server.listen(common.PORT, '127.0.0.1', function() {
    var conn = tls.connect(common.PORT, '127.0.0.1', function() {
      connect_cb_called++;

      conn.write(req, function() {
        write_cb_called++;

        conn.on('close', function() {
          close_cb_called++;
          server.close();
          run(index + 1);
        });

        conn.destroy();
      });
    });
  });
}