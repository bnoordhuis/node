// Copyright 2014, StrongLoop, Inc. <callback@strongloop.com>
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

;(function(B) {
  'use strict';

  var kDefaultHost = '127.0.0.1';
  var kDefaultPort = 5858;

  function CHECK(ok, message) {
    if (!ok) throw Error(message || 'Assertion failed.');
  }

  function UNREACHABLE() {
    CHECK(false, 'Unreachable location reached.');
  }

  var info = B.print.call.bind(B.print, B, B.stdout, '[debug agent] ');
  var warn = B.print.call.bind(B.print, B, B.stderr, '[debug agent] ');

  function Parser() {
    this.cont_ = Parser.make();
  }

  Parser.prototype = {
    parse: function(data) {
      this.cont_ = this.cont_(this, data);
    },
    onheaders: function(names, values) {
      print('names:', names);
      print('values:', values);
    },
    onbody: function(body) {
      print('body:', body);
    },
    onerror: function(errmsg) {
      print('errmsg:', errmsg);
    },
  };

  Parser.make = function() {
    return headers;  // Start state.

    var index = 0;
    var input = '';
    var length = 0;

    function headers(that, data) {
      input += data;
      var slice = input.slice(index);
      var match = slice.match(/\r?\n\r?\n/);  // Find end of headers.
      if (match === null) {
        index = input.length;
        return headers;  // Need more input.
      }
      index += match.index;
      var fields = input.slice(0, index);
      input = input.slice(index + match[0].length);  // Body.
      index = 0;
      // Split at (and consume) \r\n unless next line has leading whitespace.
      fields = fields.split(/\r?\n(?:(?=[^\t\r\n ]))/);
      fields = fields.map(function(s) { return s.split(/:/, 2); });
      var flattened = [].concat.apply([], fields);  // flatMap
      if (fields.length !== 2 * flattened.length) {
        return error(that);  // Failed split(/:/).
      }
      fields = flattened, flattened = null;
      fields = fields.map(function(s) { return s.trim(); });
      var names = fields.filter(function(_, i) { return i % 2 === 0; });
      var values = fields.filter(function(_, i) { return i % 2 !== 0; });
      that.onheaders(names, values);
      var n = fields.reduce(function(a, s, i) {
        return a === -1 && /^Content-Length$/i.test(s) ? i : a;
      }, -1);
      length = values[n] | 0;
      return body('');
    }

    function body(that, data) {
      input += data;
      if (input.length < length) {
        return body;  // Need more input.
      }
      var s = input.slice(0, length);
      input = input.slice(length);  // Headers of next request.
      s = B.oneByteToUtf8(s);  // Fix up multi-byte sequences.
      that.onbody(s);
      return headers('');
    }

    function error(that) {
      that.onerror('Parse error');
      return error;  // End state.
    }
  };

  function Handle() {
    this.id_ = ++Handle.ids;
    this.handle_ = B.socket(this.id_);
    this.parser_ = null;
    Handle.map[this.id_] = this;
  }
  Handle.ids = 0;
  Handle.map = {};

  Handle.get = function(id) {
    return Handle.map[id];
  };

  Handle.prototype = {
    close: function() {
      if (this.handle_ === null) return;
      delete Handle.map[this.id_];
      B.close(this.handle_);
      this.handle_ = null;
    },
    handle: function() {
      return this.handle_;
    },
    id: function() {
      return this.id_;
    },
    parser: function() {
      return this.parser_ || (this.parser_ = new Parser());
    },
  };

  this.onasync = function(ev) {
    ev = JSON.parse(ev);
    switch (ev.cmd) {
      case 'startDebugger':
        return startDebugger(ev);
    }
    UNREACHABLE();
  };

  this.onconnection = function(ident) {
    var server = Handle.get(ident);
    var client = new Handle();
    var err = B.accept(server.handle(), client.handle());
    if (err !== 0) {
      warn('accept error: ', B.strerror(err));
      client.close();
      return;
    }
    var headers =
       'Type: connect\r\n' +
       'V8-Version: ' + B.V8_VERSION_STRING + '\r\n' +
       'Protocol-Version: 1\r\n' +
       'Embedding-Host: node ' + B.NODE_VERSION_STRING + '\r\n' +
       'Content-Length: 0\r\n' +
       '\r\n';
    B.write(client.handle(), headers);
  };

  // Note: |data| is a one-byte encoded string.  Any multi-byte characters
  // are mangled at this point but we fix that up when we have the full body.
  // Headers don't need a post-processing step because they are ASCII-only.
  this.onread = function(ident, err, data) {
    var client = Handle.get(ident);
    if (err < 0) {
      if (err !== B.UV_EOF) warn('read error: ', B.strerror(err));
      client.close();
      return;
    }
    client.parser().parse(data);
  };

  this.onwrite = function(ident, err) {
    var client = Handle.get(ident);
    if (err === 0) {
      B.readStart(client.handle());
    } else {
      warn('write error: ', B.strerror(err));
      client.close();
    }
  };

  function startDebugger(ev) {
    var server = new Handle();
    var host = ev.host || kDefaultHost;
    var port = ev.port || kDefaultPort;
    var err = B.bind(server.handle(), host, port);
    if (err === 0) {
      err = B.listen(server.handle(), /* backlog */ 1);
    }
    if (err === 0) {
      info('listening on ', host, ':', port);
    } else {
      warn('bind error: ', host, ':', port, ': ', B.strerror(err));
      server.close();
    }
  }
});
