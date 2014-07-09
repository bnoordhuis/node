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

#include "debugger.h"
#include "debugger-js.h"  // Generated from src/debugger.js by js2c.
#include "env.h"
#include "env-inl.h"
#include "node_version.h"
#include "queue.h"
#include "util.h"
#include "util-inl.h"
#include "uv.h"
#include "v8-debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace node {

using v8::Break;
using v8::Context;
using v8::Debug;
using v8::DebugEvent;
using v8::External;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Locker;
using v8::Null;
using v8::Object;
using v8::Persistent;
using v8::Script;
using v8::String;
using v8::TryCatch;
using v8::V8;
using v8::Value;

// GlobalDebugger drives the debug thread.  It has its own event loop.
class GlobalDebugger {
 public:
  static const char* SendAndReceive(const char* command);
 private:
  static const char* channel;
  static uv_once_t call_once;
  static uv_thread_t debug_thread;
  static uv_mutex_t mutex;
  static uv_cond_t condvar;
  static uv_loop_t* event_loop;
  static uv_async_t async_handle;
  static Isolate* isolate;
  static void CreateDebugThread();
  static void DebugThreadMain(void*);
  static void OnAlloc(uv_handle_t* handle, size_t size, uv_buf_t* buf);
  static void OnAsync(uv_async_t* handle);
  static void OnConnection(uv_stream_t* handle, int err);
  static void OnRead(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf);
  static void OnWrite(uv_write_t* req, int err);
  static void AcceptFunction(const FunctionCallbackInfo<Value>& info);
  static void BindFunction(const FunctionCallbackInfo<Value>& info);
  static void CloseFunction(const FunctionCallbackInfo<Value>& info);
  static void ListenFunction(const FunctionCallbackInfo<Value>& info);
  static void OneByteToUtf8Function(const FunctionCallbackInfo<Value>& info);
  static void PrintFunction(const FunctionCallbackInfo<Value>& info);
  static void ReadStartFunction(const FunctionCallbackInfo<Value>& info);
  static void ReadStopFunction(const FunctionCallbackInfo<Value>& info);
  static void SocketFunction(const FunctionCallbackInfo<Value>& info);
  static void StrerrorFunction(const FunctionCallbackInfo<Value>& info);
  static void WriteFunction(const FunctionCallbackInfo<Value>& info);
  static Local<Value> Call(const char* name, int argc, Local<Value> argv[]);
  GlobalDebugger();  // Prevent instantiation.
};

template <typename T>
inline uint32_t ident(const T* that) {
  return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(that->data));
}

template <typename T>
inline Local<Value> ident(Isolate* isolate, const T* that) {
  return Integer::NewFromUnsigned(isolate, ident(that));
}

template <typename T>
inline void set_ident(T* that, uint32_t value) {
  that->data = reinterpret_cast<void*>(static_cast<uintptr_t>(value));
}

const char* GlobalDebugger::channel;
uv_once_t GlobalDebugger::call_once;
uv_thread_t GlobalDebugger::debug_thread;
uv_loop_t* GlobalDebugger::event_loop;
uv_async_t GlobalDebugger::async_handle;
uv_mutex_t GlobalDebugger::mutex;
uv_cond_t GlobalDebugger::condvar;
Isolate* GlobalDebugger::isolate;

const char* GlobalDebugger::SendAndReceive(const char* command) {
  ::uv_once(&call_once, CreateDebugThread);
  ::uv_mutex_lock(&mutex);
  CHECK_EQ(NULL, channel);
  channel = command;
  CHECK_EQ(0, ::uv_async_send(&async_handle));
  do {
    ::uv_cond_wait(&condvar, &mutex);
  } while (channel == command);
  const char* const answer = channel;
  channel = NULL;
  ::uv_mutex_unlock(&mutex);
  return answer;
}

void GlobalDebugger::CreateDebugThread() {
  CHECK_EQ(0, ::uv_cond_init(&condvar));
  CHECK_EQ(0, ::uv_mutex_init(&mutex));
  ::uv_mutex_lock(&mutex);
  CHECK_EQ(0, ::uv_thread_create(&debug_thread, DebugThreadMain, 0));
  do {
    ::uv_cond_wait(&condvar, &mutex);
  } while (isolate == NULL);  // Wait for debug thread's ready signal.
  ::uv_mutex_unlock(&mutex);
}

void GlobalDebugger::DebugThreadMain(void*) {
  ::uv_mutex_lock(&mutex);
  event_loop = ::uv_loop_new();
  CHECK_NE(NULL, event_loop);
  CHECK_EQ(0, ::uv_async_init(event_loop, &async_handle, OnAsync));
  isolate = Isolate::New();
  CHECK_NE(NULL, isolate);
  {
    Locker locker(isolate);
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    Local<Context> context = Context::New(isolate);
    Context::Scope context_scope(context);
    Local<String> script_source = String::NewFromUtf8(isolate, debugger_native);
    Local<Script> script = Script::Compile(script_source);
    CHECK_EQ(false, script.IsEmpty());  // Exception.
    Local<Value> return_value = script->Run();
    CHECK_EQ(false, return_value.IsEmpty());  // Exception.
    CHECK_EQ(true, return_value->IsFunction());
    Local<Function> entry_point = return_value.As<Function>();
    Local<Object> bindings = Object::New(isolate);
    bindings->Set(
        String::NewFromUtf8(isolate, "NODE_VERSION_STRING"),
        String::NewFromUtf8(isolate, NODE_VERSION_STRING));
    bindings->Set(
        String::NewFromUtf8(isolate, "V8_VERSION_STRING"),
        String::NewFromUtf8(isolate, V8::GetVersion()));
    bindings->Set(
        String::NewFromUtf8(isolate, "UV_EOF"),
        Integer::New(isolate, UV_EOF));
    bindings->Set(
        String::NewFromUtf8(isolate, "stdout"),
        External::New(isolate, stdout));
    bindings->Set(
        String::NewFromUtf8(isolate, "stderr"),
        External::New(isolate, stderr));
    bindings->Set(
        String::NewFromUtf8(isolate, "accept"),
        FunctionTemplate::New(isolate, AcceptFunction)->GetFunction());
    bindings->Set(
        String::NewFromUtf8(isolate, "bind"),
        FunctionTemplate::New(isolate, BindFunction)->GetFunction());
    bindings->Set(
        String::NewFromUtf8(isolate, "close"),
        FunctionTemplate::New(isolate, CloseFunction)->GetFunction());
    bindings->Set(
        String::NewFromUtf8(isolate, "listen"),
        FunctionTemplate::New(isolate, ListenFunction)->GetFunction());
    bindings->Set(
        String::NewFromUtf8(isolate, "oneByteToUtf8"),
        FunctionTemplate::New(isolate, OneByteToUtf8Function)->GetFunction());
    bindings->Set(
        String::NewFromUtf8(isolate, "print"),
        FunctionTemplate::New(isolate, PrintFunction)->GetFunction());
    bindings->Set(
        String::NewFromUtf8(isolate, "readStart"),
        FunctionTemplate::New(isolate, ReadStartFunction)->GetFunction());
    bindings->Set(
        String::NewFromUtf8(isolate, "readStop"),
        FunctionTemplate::New(isolate, ReadStopFunction)->GetFunction());
    bindings->Set(
        String::NewFromUtf8(isolate, "socket"),
        FunctionTemplate::New(isolate, SocketFunction)->GetFunction());
    bindings->Set(
        String::NewFromUtf8(isolate, "strerror"),
        FunctionTemplate::New(isolate, StrerrorFunction)->GetFunction());
    bindings->Set(
        String::NewFromUtf8(isolate, "write"),
        FunctionTemplate::New(isolate, WriteFunction)->GetFunction());
    Local<Value> argv[] = { bindings };
    return_value = entry_point->Call(context->Global(), ARRAY_SIZE(argv), argv);
    CHECK_EQ(false, return_value.IsEmpty());  // Exception.
    ::uv_cond_signal(&condvar);
    ::uv_mutex_unlock(&mutex);
    CHECK_EQ(0, ::uv_run(event_loop, UV_RUN_DEFAULT));
    ::uv_mutex_lock(&mutex);
  }
  isolate->Dispose();
  isolate = NULL;
  ::uv_close(reinterpret_cast<uv_handle_t*>(&async_handle), NULL);
  CHECK_EQ(0, ::uv_run(event_loop, UV_RUN_DEFAULT));
  ::uv_loop_delete(event_loop);
  event_loop = NULL;
  ::uv_mutex_unlock(&mutex);
}

Local<Value> GlobalDebugger::Call(const char* name,
                                  int argc,
                                  Local<Value> argv[]) {
  Local<Context> context = isolate->GetCurrentContext();
  CHECK_EQ(false, context.IsEmpty());
  Local<Object> global_object = context->Global();
  Local<String> key = String::NewFromUtf8(isolate, name);
  Local<Value> value = global_object->Get(key);
  CHECK_EQ(true, value->IsFunction());
  Local<Function> callback = value.As<Function>();
  TryCatch try_catch;
  Local<Value> return_value = callback->Call(Null(isolate), argc, argv);
  if (try_catch.HasCaught()) {
    String::Utf8Value exception(try_catch.Exception());
    String::Utf8Value stack_trace(try_catch.StackTrace());
    ::fprintf(stderr, "Exception in debug agent: %s\n%s\n",
              *exception, *stack_trace);
    ::fflush(stderr);
    ::abort();
  }
  return return_value;
}

void GlobalDebugger::OnAsync(uv_async_t* handle) {
  CHECK_EQ(handle, &async_handle);
  ::uv_mutex_lock(&mutex);
  CHECK_NE(NULL, channel);
  Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);
  Local<String> command = String::NewFromUtf8(isolate, channel);
  Local<Value> argv[] = { command };
  Local<Value> return_value = Call("onasync", ARRAY_SIZE(argv), argv);
  CHECK_EQ(false, return_value.IsEmpty());  // Exception.
  String::Utf8Value string_value(return_value);
  const size_t size = string_value.length() + 1;
  char* const answer = new char[size];
  ::memcpy(answer, *string_value, size);
  channel = answer;
  ::uv_cond_signal(&condvar);
  ::uv_mutex_unlock(&mutex);
}

void GlobalDebugger::OnAlloc(uv_handle_t*, size_t size, uv_buf_t* buf) {
  buf->base = new char[size];
  buf->len = size;
}

void GlobalDebugger::OnConnection(uv_stream_t* handle, int err) {
  // XXX(bnoordhuis) Only plausible error at this time is hitting the open
  // file descriptor limit.  I don't think it makes sense to go on in that
  // case but on the other hand, killing the application is pretty drastic.
  CHECK_EQ(0, err);
  Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);
  Local<Value> arg = ident(isolate, handle);
  Local<Value> return_value = Call("onconnection", 1, &arg);
  CHECK_EQ(false, return_value.IsEmpty());  // Exception.
}

void GlobalDebugger::OnRead(uv_stream_t* handle,
                            ssize_t nread,
                            const uv_buf_t* buf) {
  Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);
  char* const data = buf->base;
  Local<Value> string = Null(isolate);
  if (nread > 0) {
    // Note: the one-byte encoding mangles multi-byte character sequences but
    // we JS land fixes that up later by recoding the string to UTF-8 after
    // initial processing is complete.  It's not terribly efficient but this
    // is not performance-critical code and it means we don't have to deal
    // with multi-byte characters that are split across reads.
    string = String::NewFromOneByte(isolate, reinterpret_cast<uint8_t*>(data),
                                    String::kNormalString, nread);
  }
  Local<Value> argv[] = {
    ident(isolate, handle),
    Integer::New(isolate, nread),
    string
  };
  Call("onread", ARRAY_SIZE(argv), argv);
  delete[] data;
}

void GlobalDebugger::OnWrite(uv_write_t* req, int err) {
  uv_stream_t* const handle = req->handle;
  delete reinterpret_cast<char*>(req);
  Isolate::Scope isolate_scope(isolate);
  HandleScope handle_scope(isolate);
  Local<Value> argv[] = {
    ident(isolate, handle),
    Integer::New(isolate, err)
  };
  Call("onwrite", ARRAY_SIZE(argv), argv);
}

void GlobalDebugger::AcceptFunction(const FunctionCallbackInfo<Value>& info) {
  Isolate* const isolate = info.GetIsolate();
  HandleScope handle_scope(isolate);
  CHECK_EQ(true, info[0]->IsExternal());
  CHECK_EQ(true, info[1]->IsExternal());
  uv_stream_t* const server_handle =
      static_cast<uv_stream_t*>(info[0].As<External>()->Value());
  uv_stream_t* const client_handle =
      static_cast<uv_stream_t*>(info[1].As<External>()->Value());
  const int err = ::uv_accept(server_handle, client_handle);
  info.GetReturnValue().Set(err);
}

void GlobalDebugger::BindFunction(const FunctionCallbackInfo<Value>& info) {
  Isolate* const isolate = info.GetIsolate();
  HandleScope handle_scope(isolate);
  CHECK_EQ(true, info[0]->IsExternal());
  CHECK_EQ(true, info[1]->IsString());
  CHECK_EQ(true, info[2]->IsUint32());
  uv_tcp_t* const handle =
      static_cast<uv_tcp_t*>(info[0].As<External>()->Value());
  String::Utf8Value host(info[1]);
  const uint32_t port = info[2]->Uint32Value();
  CHECK(port < 65536);
  STATIC_ASSERT(sizeof(sockaddr_in) <= sizeof(sockaddr_in6));
  char address[sizeof(sockaddr_in6)];
  int err = ::uv_ip4_addr(*host, port, reinterpret_cast<sockaddr_in*>(address));
  if (err != 0) {
    err = ::uv_ip6_addr(*host, port, reinterpret_cast<sockaddr_in6*>(address));
  }
  if (err == 0) {
    err = ::uv_tcp_bind(handle, reinterpret_cast<const sockaddr*>(&address), 0);
  }
  info.GetReturnValue().Set(err);
}

void GlobalDebugger::CloseFunction(const FunctionCallbackInfo<Value>& info) {
  Isolate* const isolate = info.GetIsolate();
  HandleScope handle_scope(isolate);
  CHECK_EQ(true, info[0]->IsExternal());
  uv_handle_t* const handle =
      static_cast<uv_handle_t*>(info[0].As<External>()->Value());
  ::uv_close(handle, NULL);
}

void GlobalDebugger::ListenFunction(const FunctionCallbackInfo<Value>& info) {
  Isolate* const isolate = info.GetIsolate();
  HandleScope handle_scope(isolate);
  CHECK_EQ(true, info[0]->IsExternal());
  CHECK_EQ(true, info[1]->IsInt32());
  uv_stream_t* const handle =
      static_cast<uv_stream_t*>(info[0].As<External>()->Value());
  const int32_t backlog = info[1]->Int32Value();
  const int err = ::uv_listen(handle, backlog, OnConnection);
  info.GetReturnValue().Set(err);
}

void GlobalDebugger::OneByteToUtf8Function(
    const FunctionCallbackInfo<Value>& info) {
  Isolate* const isolate = info.GetIsolate();
  HandleScope handle_scope(isolate);
  CHECK_EQ(true, info[0]->IsString());
  String::Value string_value(info[0]);
  const size_t size = string_value.length();
  uint8_t* const data = new uint8_t[size];
  for (size_t n = 0; n < size; n += 1) {
    data[n] = static_cast<uint8_t>(255 & (*string_value)[n]);
  }
  Local<String> s = String::NewFromUtf8(isolate, reinterpret_cast<char*>(data),
                                        String::kNormalString, size);
  delete[] data;
  info.GetReturnValue().Set(s);
}

void GlobalDebugger::PrintFunction(const FunctionCallbackInfo<Value>& info) {
  Isolate* const isolate = info.GetIsolate();
  HandleScope handle_scope(isolate);
  CHECK_EQ(true, info[0]->IsExternal());
  FILE* stream = static_cast<FILE*>(info[0].As<External>()->Value());
  for (int i = 1, n = info.Length(); i < n; i += 1) {
    String::Utf8Value string_value(info[i]);
    if (*string_value != NULL) {
      ::fwrite(*string_value, 1, string_value.length(), stream);
    }
  }
  ::fwrite("\n", 1, 1, stream);
  ::fflush(stream);
}

void GlobalDebugger::ReadStartFunction(
    const FunctionCallbackInfo<Value>& info) {
  Isolate* const isolate = info.GetIsolate();
  HandleScope handle_scope(isolate);
  CHECK_EQ(true, info[0]->IsExternal());
  uv_stream_t* const handle =
      static_cast<uv_stream_t*>(info[0].As<External>()->Value());
  CHECK_EQ(0, ::uv_read_start(handle, OnAlloc, OnRead));
}

void GlobalDebugger::ReadStopFunction(
    const FunctionCallbackInfo<Value>& info) {
  Isolate* const isolate = info.GetIsolate();
  HandleScope handle_scope(isolate);
  CHECK_EQ(true, info[0]->IsExternal());
  uv_stream_t* const handle =
      static_cast<uv_stream_t*>(info[0].As<External>()->Value());
  CHECK_EQ(0, ::uv_read_stop(handle));
}

void GlobalDebugger::SocketFunction(const FunctionCallbackInfo<Value>& info) {
  Isolate* const isolate = info.GetIsolate();
  HandleScope handle_scope(isolate);
  CHECK_EQ(true, info[0]->IsUint32());
  const uint32_t sid = info[0]->Uint32Value();
  uv_tcp_t* const handle = new uv_tcp_t;
  CHECK_EQ(0, ::uv_tcp_init(event_loop, handle));
  set_ident(handle, sid);
  info.GetReturnValue().Set(External::New(isolate, handle));
}

void GlobalDebugger::StrerrorFunction(const FunctionCallbackInfo<Value>& info) {
  Isolate* const isolate = info.GetIsolate();
  HandleScope handle_scope(isolate);
  CHECK_EQ(true, info[0]->IsInt32());
  const int32_t err = info[0]->Int32Value();
  const char* errmsg = ::uv_strerror(err);
  Local<String> string = String::NewFromUtf8(isolate, errmsg);
  info.GetReturnValue().Set(string);
}

void GlobalDebugger::WriteFunction(const FunctionCallbackInfo<Value>& info) {
  Isolate* const isolate = info.GetIsolate();
  HandleScope handle_scope(isolate);
  CHECK_EQ(true, info[0]->IsExternal());
  CHECK_EQ(true, info[1]->IsString());
  uv_stream_t* const handle =
      static_cast<uv_stream_t*>(info[0].As<External>()->Value());
  String::Utf8Value string_value(info[1]);
  const size_t size = string_value.length();
  char* const raw_data = new char[sizeof(uv_write_t) + size];
  uv_write_t* const req = reinterpret_cast<uv_write_t*>(raw_data);
  char* const data = raw_data + sizeof(*req);
  ::memcpy(data, *string_value, size);
  uv_buf_t buf;
  buf.base = data;
  buf.len = size;
  CHECK_EQ(0, ::uv_write(req, handle, &buf, 1, OnWrite));
}

void MessageHandler(const Debug::Message& message) {
  if (message.GetEvent() != Break) return;  // Uninteresting event.
}

// Break() can be called from a signal handler and may therefore only call
// async signal-safe functions.  That also means that it's not allowed to
// grab the mutex because doing so will deadlock when the thread already
// holds the lock.  (And besides, the pthread functions are not guaranteed
// to be signal-safe, only sem_post() is.)
void Debugger::Break(Isolate* isolate) {
  // Can't call Debug::DebugBreakForCommand() here, it allocates memory.
  Debug::DebugBreak(isolate);
}

Debugger::Debugger(Isolate* isolate) {
  // SetDebugEventListener() and SetMessageHandler() are isolate-ified but
  // their API doesn't reflect that; make sure we've entered the isolate.
  Isolate::Scope isolate_scope(isolate);
  Debug::SetMessageHandler(MessageHandler);
}

int Debugger::Start() {
  const char* answer =
      GlobalDebugger::SendAndReceive("{\"cmd\":\"startDebugger\"}");
  return 0;
}

unsigned short Debugger::port() const {
  return port_;
}

void Debugger::set_port(unsigned short value) {
  port_ = value;
}

}  // namespace node
