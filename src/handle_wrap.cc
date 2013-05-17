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

#include "node.h"
#include "ngx-queue.h"
#include "handle_wrap.h"
#include <limits.h>  // CHAR_BIT

namespace node {

using v8::Arguments;
using v8::Array;
using v8::Context;
using v8::Function;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::Persistent;
using v8::String;
using v8::TryCatch;
using v8::Undefined;
using v8::Value;


// defined in node.cc
extern ngx_queue_t handle_wrap_queue;
static Persistent<String> close_sym;


void HandleWrap::Initialize(Handle<Object> target) {
  /* Doesn't do anything at the moment. */
}


Handle<Value> HandleWrap::Ref(const Arguments& args) {
  HandleScope scope(node_isolate);

  UNWRAP_NO_ABORT(HandleWrap)

  if (wrap != NULL && wrap->handle__ != NULL) {
    uv_ref(wrap->handle__);
    wrap->flags_ &= ~kUnref;
  }

  return v8::Undefined(node_isolate);
}


Handle<Value> HandleWrap::Unref(const Arguments& args) {
  HandleScope scope(node_isolate);

  UNWRAP_NO_ABORT(HandleWrap)

  if (wrap != NULL && wrap->handle__ != NULL) {
    uv_unref(wrap->handle__);
    wrap->flags_ |= kUnref;
  }

  return v8::Undefined(node_isolate);
}


Handle<Value> HandleWrap::Close(const Arguments& args) {
  HandleScope scope(node_isolate);

  HandleWrap *wrap = static_cast<HandleWrap*>(
      args.This()->GetAlignedPointerFromInternalField(0));

  // guard against uninitialized handle or double close
  if (wrap == NULL || wrap->handle__ == NULL) {
    return Undefined(node_isolate);
  }

  assert(!wrap->object_.IsEmpty());
  uv_close(wrap->handle__, OnClose);
  wrap->handle__ = NULL;

  if (args[0]->IsFunction()) {
    if (close_sym.IsEmpty() == true) close_sym = NODE_PSYMBOL("close");
    wrap->object_->Set(close_sym, args[0]);
    wrap->flags_ |= kCloseCallback;
  }

  return Undefined(node_isolate);
}


HandleWrap::HandleWrap(Handle<Object> object, uv_handle_t* h) {
  flags_ = 0;
  handle__ = h;
  if (h) {
    h->data = this;
  }

  HandleScope scope(node_isolate);
  assert(object_.IsEmpty());
  assert(object->InternalFieldCount() > 0);
  object_ = v8::Persistent<v8::Object>::New(node_isolate, object);
  object_->SetAlignedPointerInInternalField(0, this);
  ngx_queue_insert_tail(&handle_wrap_queue, &handle_wrap_queue_);

  // Turn the address of the handle into a base32 encoded string.
  static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  uintptr_t address = reinterpret_cast<uintptr_t>(h);
  char string[CHAR_BIT * sizeof(address) / 5 + 1];
  unsigned n = 0;
  while (address != 0) {
    string[n++] = alphabet[address & 31];
    address >>= 5;
  }
  object->Set(String::New("id"), String::New(string, n));
}


void HandleWrap::SetHandle(uv_handle_t* h) {
  handle__ = h;
  h->data = this;
}


HandleWrap::~HandleWrap() {
  assert(object_.IsEmpty());
  ngx_queue_remove(&handle_wrap_queue_);
}


void HandleWrap::OnClose(uv_handle_t* handle) {
  HandleWrap* wrap = static_cast<HandleWrap*>(handle->data);

  // The wrap object should still be there.
  assert(wrap->object_.IsEmpty() == false);

  // But the handle pointer should be gone.
  assert(wrap->handle__ == NULL);

  if (wrap->flags_ & kCloseCallback) {
    assert(close_sym.IsEmpty() == false);
    MakeCallback(wrap->object_, close_sym, 0, NULL);
  }

  wrap->object_->SetAlignedPointerInInternalField(0, NULL);
  wrap->object_.Dispose(node_isolate);
  wrap->object_.Clear();

  delete wrap;
}


}  // namespace node
