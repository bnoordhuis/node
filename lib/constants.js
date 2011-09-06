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

module.exports = remap_errors(process.binding('constants'));

// remaps host errnos to libuv error codes
function remap_errors(o) {
  if (!process.features.uv)
    return o;

  // generated with `perl -ne 'print "$1\n" if /^  UV_(E\w+)/' deps/uv/include/uv.h | perl -pe 'chomp; $_ = "\"$_\": $.,\n"'`
  var remapped = {
    "EACCESS": 2,
    "EAGAIN": 3,
    "EADDRINUSE": 4,
    "EADDRNOTAVAIL": 5,
    "EAFNOSUPPORT": 6,
    "EALREADY": 7,
    "EBADF": 8,
    "EBUSY": 9,
    "ECONNABORTED": 10,
    "ECONNREFUSED": 11,
    "ECONNRESET": 12,
    "EDESTADDRREQ": 13,
    "EFAULT": 14,
    "EHOSTUNREACH": 15,
    "EINTR": 16,
    "EINVAL": 17,
    "EISCONN": 18,
    "EMFILE": 19,
    "EMSGSIZE": 20,
    "ENETDOWN": 21,
    "ENETUNREACH": 22,
    "ENFILE": 23,
    "ENOBUFS": 24,
    "ENOMEM": 25,
    "ENONET": 26,
    "ENOPROTOOPT": 27,
    "ENOTCONN": 28,
    "ENOTSOCK": 29,
    "ENOTSUP": 30,
    "ENOENT": 31,
    "EPIPE": 32,
    "EPROTO": 33,
    "EPROTONOSUPPORT": 34,
    "EPROTOTYPE": 35,
    "ETIMEDOUT": 36,
    "ECHARSET": 37,
    "EAIFAMNOSUPPORT": 38,
    "EAINONAME": 39,
    "EAISERVICE": 40,
    "EAISOCKTYPE": 41,
    "ESHUTDOWN": 42
  };

  var pp = {};

  for (var k in o) {
    pp[k] = remapped[k] || o[k];
  }

  return Object.freeze(pp);
}