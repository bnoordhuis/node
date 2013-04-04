// In this benchmark, we connect a client to the server, and write
// as many bytes as we can in the specified time (default = 10s)

var common = require('../common.js');

// if there are --dur=N and --len=N args, then
// run the function with those settings.
// if not, then queue up a bunch of child processes.
var bench = common.createBenchmark(main, {
  len: [102400, 1024 * 1024 * 16],
  type: ['asc'],
  dur: [5]
});

var TCP = process.binding('tcps_wrap').TCPS;
var PORT = common.PORT;

var dur;
var len;
var type;

function main(conf) {
  dur = +conf.dur;
  len = +conf.len;
  type = conf.type;
  server();
}


function fail(syscall) {
  var errno = process._errno;
  var e = new Error(syscall + ' ' + errno);
  e.errno = e.code = errno;
  e.syscall = syscall;
  throw e;
}

function server() {
  var serverHandle = new TCP();
  var rc;

  rc = serverHandle.bind('127.0.0.1', PORT);
  if (rc)
    fail('bind');

  rc = serverHandle.listen(511);
  if (rc)
    fail('listen');

  serverHandle.onconnection = function(clientHandle) {
    if (!clientHandle)
      fail('connect');

    clientHandle.onwrite = function(status) {
      if (status)
        fail('write');
    };

    clientHandle.onread = function(string, length) {
      // we're not expecting to ever get an EOF from the client.
      // just lots of data forever.
      if (length < 0)
        fail('read');

      console.error('' + string);
      var rc = clientHandle.write(string, length);
      if (!rc)
        fail('write');
      console.error('server write');
    };

    clientHandle.readStart();
  };

  client();
}

function client() {
  var chunk;
  switch (type) {
    case 'asc':
      // latin1 strings are contigous chunks of memory when freshly allocated
      chunk = Buffer(new Array(len + 1).join('x')).toString('latin1');
      break;
    default:
      throw new Error('invalid type: ' + type);
      break;
  }

  var clientHandle = new TCP();
  var bytes = 0;
  var rc;

  rc = clientHandle.connect('127.0.0.1', PORT);
  if (rc)
    fail('connect');

  rc = clientHandle.readStart();
  if (rc)
    fail('readStart');

  clientHandle.onread = function(string, length) {
    bytes += length;
  };

  clientHandle.onconnect = function() {
    bench.start();

    setTimeout(function() {
      // multiply by 2 since we're sending it first one way
      // then then back again.
      bench.end(2 * (bytes * 8) / (1024 * 1024 * 1024));
    }, dur * 1000);

    write();
  };

  clientHandle.onwrite = write;

  function write() {
    clientHandle.write(chunk, chunk.length);
  }
}
