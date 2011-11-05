var http = require("http");

var concurrency = 30;
var port = 12346;
var n = 7; // several orders of magnitude slower
var bytes = 1024*5;

var requests = 0;
var responses = 0;

var body = "";
for (var i = 0; i < bytes; i++) {
  body += "C";
}

var server = http.createServer(function (req, res) {
  res.writeHead(200, {
    "Content-Type": "text/plain",
    "Content-Length": body.length
  });
  res.write(body);
  res.end();
})
server.listen(port, '127.0.0.1');

function responseListener (res) {
  res.on("end", function () {
    if (requests < n) {
      var req = res.client.request("/");
      req.on('response', responseListener);
      req.end();
      requests++;
    }

    if (++responses == n) {
      server.close();
    }
  });
}

for (var i = 0; i < concurrency; i++) {
  var client = http.createClient(port, '127.0.0.1');
  client.id = i;
  var req = client.request("/");
  req.on('response', responseListener);
  req.end();
  requests++;
}

process.on('exit', function ()  {
  console.log('%d requests seen', requests);
});
