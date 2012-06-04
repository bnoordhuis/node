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

var Timer = process.binding('timer_wrap').Timer;

// Timeout values > TIMEOUT_MAX are set to 1.
var TIMEOUT_MAX = 2147483647; // 2^31-1

var debug;
if (process.env.NODE_DEBUG && /timer/.test(process.env.NODE_DEBUG)) {
  debug = function() { require('util').error.apply(this, arguments); };
} else {
  debug = function() { };
}


var timer_handle = new Timer;
var timer_heap = [];
var timer_next = -1;
var timer_now = -1;


timer_handle.ontimeout = function() {
  var reqs = pending();

  timer_now = Date.now();
  timer_next = -1; // allow rescheduling

  for (var i = 0, k = reqs.length; i < k; ++i)
    for (var cb in reqs[i].callbacks)
      if (cb)
        cb();

  reschedule();
};


function pending() {
  var n = 0;

  while (timer_heap[n] && timer_heap[n].msec <= timer_now)
    n++;

  return timer_heap.splice(0, n);
}


function find(msec, add) {
  var values = timer_heap; // cache in local var == minor speedup
  var max = values.length;
  var min = 0;

  if (timer_now === -1)
    timer_now = Date.now(); // cache result, avoid unnecessary syscalls

  msec += timer_now;

  while (max - min > 1) {
    var n = min + (max - min >> 1);
    var v = values[n];

    if (v.msec === msec)
      return v;

    if (v.msec < msec)
      min = n;
    else
      max = n;
  }

  if (!add)
    return null;

  var v = { msec: msec, callbacks: [] };

  if (values[n] && values[n].msec < msec)
    values.splice(min, 0, v);
  else
    values.splice(max, 0, v);

  return v;
}


function insert(cb, msec) {
  var v = find(msec, true);
  var n = v.callbacks.length;
  v.callbacks.push(cb);
  reschedule();
  return [v, n];
}


function remove(req) {
  var v = req[0];
  var n = req[1];
  v.callbacks[n] = null;
}


function reschedule() {
  if (timer_heap.length === 0)
    return;

  var msec = timer_heap[0].msec;

  if (timer_next != -1 && timer_next <= msec)
    return;

  timer_next = msec;
  timer_handle.stop();
  timer_handle.start(msec - timer_now, 0);
}


exports.setTimeout = function(callback, delay) {
  return insert(callback, delay);
};


exports.clearTimeout = function(timeoutId) {
  remove(timeoutId);
};


exports.setInterval = function(callback, delay) {
  var intervalId = { _handle: insert(fn, delay) };

  function fn() {
    callback();
    intervalId._handle = insert(fn, delay);
  }

  return intervalId;
};


exports.clearInterval = function(intervalId) {
  remove(intervalId._handle);
  intervalId._handle = null;
};
