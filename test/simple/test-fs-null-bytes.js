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
var fs = require('fs');

function check(fun1, fun2) {
  var args = Array.prototype.slice.call(arguments, 2);
  var expected = /Path must be a string without null bytes./;
  if (fun1) assert.throws(function() { fun1.apply(null, args) }, expected);
  if (fun2) assert.throws(function() { fun2.apply(null, args) }, expected);
}

check(fs.appendFile,  fs.appendFileSync,  'foo\u0000bar');
check(fs.chmod,       fs.chmodSync,       'foo\u0000bar');
check(fs.chown,       fs.chownSync,       'foo\u0000bar');
check(fs.exists,      fs.existsSync,      'foo\u0000bar');
check(fs.link,        fs.linkSync,        'foo\u0000bar', 'foobar');
check(fs.link,        fs.linkSync,        'foobar', 'foo\u0000bar');
check(fs.lstat,       fs.lstatSync,       'foo\u0000bar');
check(fs.mkdir,       fs.mkdirSync,       'foo\u0000bar');
check(fs.open,        fs.openSync,        'foo\u0000bar', 'r');
check(fs.readFile,    fs.readFileSync,    'foo\u0000bar');
check(fs.readdir,     fs.readdirSync,     'foo\u0000bar');
check(fs.readlink,    fs.readlinkSync,    'foo\u0000bar');
check(fs.realpath,    fs.realpathSync,    'foo\u0000bar');
check(fs.rename,      fs.renameSync,      'foo\u0000bar', 'foobar');
check(fs.rename,      fs.renameSync,      'foobar', 'foo\u0000bar');
check(fs.rmdir,       fs.rmdirSync,       'foo\u0000bar');
check(fs.stat,        fs.statSync,        'foo\u0000bar');
check(fs.symlink,     fs.symlinkSync,     'foo\u0000bar', 'foobar');
check(fs.symlink,     fs.symlinkSync,     'foobar', 'foo\u0000bar');
check(fs.truncate,    fs.truncateSync,    'foo\u0000bar');
check(fs.unlink,      fs.unlinkSync,      'foo\u0000bar');
check(fs.unwatchFile, null,               'foo\u0000bar', assert.fail);
check(fs.utimes,      fs.utimesSync,      'foo\u0000bar', 0, 0);
check(fs.watch,       null,               'foo\u0000bar', assert.fail);
check(fs.watchFile,   null,               'foo\u0000bar', assert.fail);
check(fs.writeFile,   fs.writeFileSync,   'foo\u0000bar');
