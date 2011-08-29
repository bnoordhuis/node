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
var https = require('https');
var cp = require('child_process');
var fs = require('fs');


function do_server() {
  var options = {
    key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
    cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
  };

  // simulate a server aborting the request half-way through
  var server = https.createServer(options, function(req, res) {
    res.writeHead(200, {'Content-Type':'text/plain',
                        'Transfer-Encoding':'chunked'});
    res.write('PING');
    res.destroy();
    process.exit(42);
  });

  server.listen(common.PORT, '127.0.0.1', function() {
    console.error('READY'); // signals parent
  });
}


function do_client() {
  var closed = false;

  var req = https.get({host:'127.0.0.1', port:common.PORT}, function(res) {
    res.on('data', function(data) {
      assert.equal('PING', data);
    });

    res.on('close', function() {
      closed = true;
    });
  });

  process.on('exit', function() {
    assert.equal(true, closed);
  });
}


if (process.argv[2] == '--server') {
  do_server();
}
else {
  // start the server in a child process
  var child = cp.spawn(process.argv[0], [process.argv[1], '--server']);

  // wait for the server to become ready
  child.stderr.on('data', function(data) {
    assert.equal("READY\n", data);
    do_client();
  });

  child.on('exit', function(status) {
    assert.equal(42, status);
  });
}
