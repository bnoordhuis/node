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

var NUM_CLIENTS = 100;
var NUM_REQUESTS = 500; // # requests per client

var http = require('http');

var body = Buffer("Hello, node.js world.\n");

var headers = {
  'Content-Type': 'text/plain',
  'Content-Length': '' + body.length
};

var activeClients = 0;
var requestsCompleted = 0;
var startTime = 0;

var server = http.createServer(function(req, res) {
  res.writeHead(200, headers);
  res.write(body);
  res.end();
});

var options = {
  agent: new http.Agent({ maxSockets: NUM_CLIENTS }),
  host: '127.0.0.1',
  port: 8000,
  path: '/'
};

server.listen(8000, '127.0.0.1', function() {
  activeClients = NUM_CLIENTS;
  startTime = Date.now();

  for (var i = 0; i < activeClients; ++i) {
    http.get(options, onResponse.bind({ numRequests: NUM_REQUESTS }));
  }
});

function onResponse(res) {
  res.on('end', onEnd);
  if (--this.numRequests)
    http.get(options, onResponse.bind(this));
  else if (--activeClients == 0)
    server.close();
}

function onEnd() {
  requestsCompleted++;
  this.destroy();
}

function report() {
  var ms = Date.now() - startTime;

  console.error('%d requests completed (%d reqs/s)',
                requestsCompleted,
                ~~(requestsCompleted / (ms / 1000)));

  if (activeClients)
    console.error('still %d clients active', activeClients);
}

function kill() {
  report();
  process.exit(42);
}

setTimeout(kill, 5000);