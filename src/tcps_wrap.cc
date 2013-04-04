#include "node.h"

namespace node {

#define SET_ERRNO()                                                           \
  do {                                                                        \
    uv_err_t err = uv_last_error(uv_default_loop());                          \
    SetErrno(err);                                                            \
  }                                                                           \
  while (0)

using namespace v8;

class TCPSWrap {
public:
  static void Initialize(Handle<Object> target);
  static Handle<Value> New(const Arguments& args);
  static Handle<Value> Bind(const Arguments& args);
  static Handle<Value> Listen(const Arguments& args);
  static Handle<Value> Connect(const Arguments& args);
  static Handle<Value> ReadStart(const Arguments& args);
  static Handle<Value> Write(const Arguments& args);
  static Handle<Value> Close(const Arguments& args);
private:
  static void OnConnection(uv_stream_t* handle, int status);
  static uv_buf_t OnAlloc(uv_handle_t* handle, size_t suggested_size);
  static void OnRead(uv_stream_t* handle, ssize_t nread, uv_buf_t buf);
  static void OnWrite(uv_write_t* req, int status);
  static void OnConnect(uv_connect_t* req, int status);
  static void OnClose(uv_handle_t* handle);
  static void WeakCallback(Isolate*, Persistent<Value>, void* arg);
  static Persistent<Function> constructor_;
  static Persistent<String> onconnection_sym_;
  static Persistent<String> onconnect_sym_;
  static Persistent<String> onwrite_sym_;
  static Persistent<String> onread_sym_;
  Persistent<String> write_str_;
  Persistent<String> read_str_;
  Persistent<Object> object_;
  uv_connect_t connect_req_;
  uv_write_t write_req_;
  uv_tcp_t handle_;
};

Persistent<Function> TCPSWrap::constructor_;
Persistent<String> TCPSWrap::onconnection_sym_;
Persistent<String> TCPSWrap::onconnect_sym_;
Persistent<String> TCPSWrap::onwrite_sym_;
Persistent<String> TCPSWrap::onread_sym_;


void TCPSWrap::Initialize(Handle<Object> target) {
  HandleScope handle_scope(node_isolate);
  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(String::New("TCPS"));
  t->PrototypeTemplate()->Set(String::New("bind"),
                              FunctionTemplate::New(Bind)->GetFunction());
  t->PrototypeTemplate()->Set(String::New("listen"),
                              FunctionTemplate::New(Listen)->GetFunction());
  t->PrototypeTemplate()->Set(String::New("connect"),
                              FunctionTemplate::New(Connect)->GetFunction());
  t->PrototypeTemplate()->Set(String::New("readStart"),
                              FunctionTemplate::New(ReadStart)->GetFunction());
  t->PrototypeTemplate()->Set(String::New("write"),
                              FunctionTemplate::New(Write)->GetFunction());
  t->PrototypeTemplate()->Set(String::New("close"),
                              FunctionTemplate::New(Close)->GetFunction());
  target->Set(String::New("TCPS"), t->GetFunction());
  constructor_ = Persistent<Function>::New(node_isolate, t->GetFunction());
  onconnection_sym_ = Persistent<String>::New(node_isolate,
                                              String::New("onconnection"));
  onconnect_sym_ = Persistent<String>::New(node_isolate,
                                           String::New("onconnect"));
  onwrite_sym_ = Persistent<String>::New(node_isolate, String::New("onwrite"));
  onread_sym_ = Persistent<String>::New(node_isolate, String::New("onread"));
}


Handle<Value> TCPSWrap::New(const Arguments& args) {
  HandleScope handle_scope(node_isolate);
  assert(args.IsConstructCall() == true);
  TCPSWrap* w = new TCPSWrap;
  uv_tcp_init(uv_default_loop(), &w->handle_);
  w->object_ = Persistent<Object>::New(node_isolate, args.This());
  w->object_->SetAlignedPointerInInternalField(0, w);
  return w->object_;
}


Handle<Value> TCPSWrap::Bind(const Arguments& args) {
  HandleScope handle_scope(node_isolate);
  void* ptr = args.This()->GetAlignedPointerFromInternalField(0);
  TCPSWrap* w = static_cast<TCPSWrap*>(ptr);
  assert(args.Length() == 2);
  String::Utf8Value address(args[0]);
  uint32_t port = args[1]->Uint32Value();
  assert(address.length() != 0);
  assert(port <= 0xffff);
  sockaddr_in sa = uv_ip4_addr(*address, port);
  int rc = uv_tcp_bind(&w->handle_, sa);
  if (rc) SET_ERRNO();
  return handle_scope.Close(Integer::New(rc));
}


Handle<Value> TCPSWrap::Listen(const Arguments& args) {
  HandleScope handle_scope(node_isolate);
  void* ptr = args.This()->GetAlignedPointerFromInternalField(0);
  TCPSWrap* w = static_cast<TCPSWrap*>(ptr);
  assert(args.Length() == 1);
  uint32_t backlog = args[0]->Uint32Value();
  int rc = uv_listen(reinterpret_cast<uv_stream_t*>(&w->handle_),
                     backlog,
                     OnConnection);
  if (rc) SET_ERRNO();
  return handle_scope.Close(Integer::New(rc));
}


Handle<Value> TCPSWrap::Connect(const Arguments& args) {
  HandleScope handle_scope(node_isolate);
  void* ptr = args.This()->GetAlignedPointerFromInternalField(0);
  TCPSWrap* w = static_cast<TCPSWrap*>(ptr);
  assert(args.Length() == 2);
  String::Utf8Value address(args[0]);
  uint32_t port = args[1]->Uint32Value();
  assert(address.length() != 0);
  assert(port <= 0xffff);
  sockaddr_in sa = uv_ip4_addr(*address, port);
  int rc = uv_tcp_connect(&w->connect_req_, &w->handle_, sa, OnConnect);
  if (rc) SET_ERRNO();
  return handle_scope.Close(Integer::New(rc));
}


Handle<Value> TCPSWrap::ReadStart(const Arguments& args) {
  HandleScope handle_scope(node_isolate);
  void* ptr = args.This()->GetAlignedPointerFromInternalField(0);
  TCPSWrap* w = static_cast<TCPSWrap*>(ptr);
  int rc = uv_read_start(reinterpret_cast<uv_stream_t*>(&w->handle_),
                         OnAlloc,
                         OnRead);
  if (rc) SET_ERRNO();
  return handle_scope.Close(Integer::New(rc));
}


Handle<Value> TCPSWrap::Write(const Arguments& args) {
  HandleScope handle_scope(node_isolate);
  void* ptr = args.This()->GetAlignedPointerFromInternalField(0);
  TCPSWrap* w = static_cast<TCPSWrap*>(ptr);
  assert(args.Length() == 2);
  assert(args[0]->IsString());
  assert(args[1]->IsUint32());
  assert(w->write_str_.IsEmpty());  // Only one write at a time.
  w->write_str_ = Persistent<String>::New(node_isolate, args[0].As<String>());
  uint8_t* data = w->write_str_->UnsafeMutablePointer();
  uint32_t length = args[1]->Uint32Value();
  uv_buf_t buf = { reinterpret_cast<char*>(data), length };
  int rc = uv_write(&w->write_req_,
                    reinterpret_cast<uv_stream_t*>(&w->handle_),
                    &buf,
                    1,
                    OnWrite);
  if (rc) SET_ERRNO();
  return handle_scope.Close(Integer::New(rc));
}


Handle<Value> TCPSWrap::Close(const Arguments& args) {
  HandleScope handle_scope(node_isolate);
  void* ptr = args.This()->GetAlignedPointerFromInternalField(0);
  TCPSWrap* w = static_cast<TCPSWrap*>(ptr);
  uv_close(reinterpret_cast<uv_handle_t*>(&w->handle_), OnClose);
  w->object_->SetAlignedPointerInInternalField(0, NULL);
  return Undefined();
}


void TCPSWrap::OnConnection(uv_stream_t* handle, int status) {
  HandleScope handle_scope(node_isolate);
  TCPSWrap* w = container_of(handle, TCPSWrap, handle_);
  Local<Object> conn = constructor_->NewInstance();
  void* ptr = conn->GetAlignedPointerFromInternalField(0);
  TCPSWrap* nw = static_cast<TCPSWrap*>(ptr);
  int rc = uv_accept(reinterpret_cast<uv_stream_t*>(&w->handle_),
                     reinterpret_cast<uv_stream_t*>(&nw->handle_));
  assert(rc == 0);
  MakeCallback(w->object_,
               onconnection_sym_,
               1,
               reinterpret_cast<Handle<Value>*>(&conn));
}


uv_buf_t TCPSWrap::OnAlloc(uv_handle_t* handle, size_t suggested_size) {
  HandleScope handle_scope(node_isolate);
  TCPSWrap* w = container_of(handle, TCPSWrap, handle_);
  // TODO(bnoordhuis) Limit suggested_size to something reasonable.
  w->read_str_ = Persistent<String>::New(node_isolate,
                                         String::New(suggested_size));
  uint8_t* data = w->read_str_->UnsafeMutablePointer();
  uv_buf_t buf = { reinterpret_cast<char*>(data), suggested_size };
  return buf;
}


void TCPSWrap::OnRead(uv_stream_t* handle, ssize_t nread, uv_buf_t buf) {
  HandleScope handle_scope(node_isolate);
  TCPSWrap* w = container_of(handle, TCPSWrap, handle_);
  Local<Integer> read_len = Integer::New(nread);
  Local<String> read_str = *w->read_str_;
  w->read_str_.Dispose(node_isolate);
  w->read_str_.Clear();
  Local<Value> argv[] = { read_str, read_len };
  MakeCallback(w->object_, onread_sym_, ARRAY_SIZE(argv), argv);
}


void TCPSWrap::OnWrite(uv_write_t* req, int status) {
  HandleScope handle_scope(node_isolate);
  TCPSWrap* w = container_of(req, TCPSWrap, write_req_);
  w->write_str_.Dispose(node_isolate);
  w->write_str_.Clear();
  Local<Value> argv[] = { Integer::New(status) };
  MakeCallback(w->object_, onwrite_sym_, ARRAY_SIZE(argv), argv);
}


void TCPSWrap::OnConnect(uv_connect_t* req, int status) {
  HandleScope handle_scope(node_isolate);
  TCPSWrap* w = container_of(req, TCPSWrap, connect_req_);
  Local<Value> argv[] = { Integer::New(status) };
  MakeCallback(w->object_, onconnect_sym_, ARRAY_SIZE(argv), argv);
}


void TCPSWrap::OnClose(uv_handle_t* handle) {
  TCPSWrap* w = container_of(handle, TCPSWrap, handle_);
  w->object_.MakeWeak(node_isolate, w, WeakCallback);
}


void TCPSWrap::WeakCallback(Isolate*, Persistent<Value>, void* arg) {
  TCPSWrap* w = static_cast<TCPSWrap*>(arg);
  w->object_.Dispose(node_isolate);
  delete w;
}


}  // namespace node

NODE_MODULE(node_tcps_wrap, node::TCPSWrap::Initialize)
